/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Inet sockets using TCP
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/tcp.h> // for TCP_NODELAY
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ipc_interface.h"
#include "tcp_net.h"
#include "time.h"

// debug macro
//#define DEBUG
#undef DEBUG

/********** All the variables needed by TCP sockets **********/

#define BOOTSTRAP_PORT 6000

#define MIN_MSG_SIZE (sizeof(int) + sizeof(long))

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes

static int *sockets; // sockets used to communicate

// Initialize resources for both the producer and the consumers
// First initialization function called
void IPC_initialize(int _nb_receivers, int _request_size)
{
  nb_receivers = _nb_receivers;
  request_size = _request_size;

}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  int bootstrap_socket;
  struct sockaddr_in bootstrap_sin;
  int i, flag;

  core_id = _core_id;

  // create bootstrap socket
  bootstrap_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (bootstrap_socket == -1)
  {
    perror("[IPC_initialize_producer] Error while creating the socket! ");
    exit(errno);
  }

  // TCP NO DELAY
  flag = 1;
  int result = setsockopt(bootstrap_socket, IPPROTO_TCP, TCP_NODELAY,
      (char*) &flag, sizeof(int));
  if (result == -1)
  {
    perror("[IPC_initialize_producer] Error while setting TCP NO DELAY! ");
  }

  // bind
  bootstrap_sin.sin_addr.s_addr = htonl(INADDR_ANY);
  bootstrap_sin.sin_family = AF_INET;
  bootstrap_sin.sin_port = htons(BOOTSTRAP_PORT);

  if (bind(bootstrap_socket, (struct sockaddr*) &bootstrap_sin,
      sizeof(bootstrap_sin)) == -1)
  {
    perror("[IPC_initialize_producer] Error while binding to the socket! ");
    exit(errno);
  }

  // make the socket listening for incoming connections
  if (listen(bootstrap_socket, nb_receivers + 1) == -1)
  {
    perror("[IPC_initialize_producer] Error while calling listen! ");
    exit(errno);
  }

  sockets = (int*) malloc(sizeof(int) * nb_receivers);
  if (!sockets)
  {
    perror("[IPC_initialize_producer] allocation error");
    exit(errno);
  }

  // accepting connections
  for (i = 0; i < nb_receivers; i++)
  {
    struct sockaddr_in csin;
    int sinsize = sizeof(csin);
    sockets[i] = accept(bootstrap_socket, (struct sockaddr*) &csin,
        (socklen_t*) &sinsize);

    if (sockets[i] == -1)
    {
      perror("[IPC_initialize_producer] An invalid socket has been accepted! ");
      continue;
    }

    // TCP NO DELAY
    int flag = 1;
    int result = setsockopt(sockets[i], IPPROTO_TCP, TCP_NODELAY,
        (char *) &flag, sizeof(int));
    if (result == -1)
    {
      perror(
          "[IPC_initialize_producer] Error while setting TCP NO DELAY for accepted socket! ");
    }

#ifdef DEBUG
    // print some information about the accepted connection
    printf("[producer] A connection has been accepted from %s:%i\n", inet_ntoa(
            csin.sin_addr), ntohs(csin.sin_port));
#endif
  }

  close(bootstrap_socket);
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  core_id = _core_id;

  // we need only 1 socket: the one to connect to the producer
  sockets = (int*) malloc(sizeof(int) * 1);
  if (!sockets)
  {
    perror("[IPC_initialize_consumer] allocation error");
    exit(errno);
  }

  // create socket
  sockets[0] = socket(AF_INET, SOCK_STREAM, 0);
  if (sockets[0] == -1)
  {
    perror("[IPC_initialize_consumer] Error while creating the socket! ");
    exit(errno);
  }

  // TCP NO DELAY
  int flag = 1;
  int result = setsockopt(sockets[0], IPPROTO_TCP, TCP_NODELAY, (char*) &flag,
      sizeof(int));
  if (result == -1)
  {
    perror("[IPC_initialize_consumer] Error while setting TCP NO DELAY! ");
  }

  // connect to producer
  struct sockaddr_in addr;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(BOOTSTRAP_PORT);

  while (true)
  {
    if (connect(sockets[0], (struct sockaddr *) &(addr), sizeof(addr)) < 0)
    {
      perror("[IPC_initialize_consumer] Cannot connect");
      sleep(1);
    }
    else
    {
#ifdef DEBUG
      printf("[consumer %i] Connection successful from %i to producer!\n",
          core_id, core_id);
#endif
      break;
    }
  }

}

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
  int i;

  for (i = 0; i < nb_receivers; i++)
  {
    close(sockets[i]);
  }
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  close(sockets[0]);
}

// Send a message to all the cores
// The message id will be msg_id
// Return the total sent payload (i.e. size of the messages times number of consumers)
// if spent_cycles is not NULL, then add the number of spent cycles in *spent_cycles
int IPC_sendToAll(int msg_size, long msg_id, uint64_t *spent_cycles)
{
  int i;
  char *msg;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

  msg = (char*) malloc(sizeof(char) * msg_size);
  if (!msg)
  {
    perror("IPC_sendToAll allocation error! ");
    exit(errno);
  }

  bzero(msg, msg_size);

  int *msg_int = (int*) msg;
  msg_int[0] = msg_size;
  long *msg_long = (long*) (msg_int + 1);
  msg_long[0] = msg_id;

#ifdef DEBUG
  printf(
      "[producer %i] going to send message %li of size %i to %i recipients\n",
      core_id, msg_long[0], msg_size, nb_receivers);
#endif

  for (i = 0; i < nb_receivers; i++)
  {
    sendMsg(sockets[i], msg, msg_size, spent_cycles);
  }

  free(msg);

  return msg_size * nb_receivers;
}

// Get a message for this core
// return the size of the message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
// if spent_cycles is not NULL, then add the number of spent cycles in *spent_cycles
int IPC_receive(int msg_size, long *msg_id, uint64_t *spent_cycles)
{
  char *msg;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

  msg = (char*) malloc(sizeof(char) * msg_size);
  if (!msg)
  {
    perror("IPC_receive allocation error! ");
    exit(errno);
  }

#ifdef DEBUG
  printf("Waiting for a new message\n");
#endif

  // get the size of the message

  int header_size =
      recvMsg(sockets[0], (void*) msg, MIN_MSG_SIZE, spent_cycles);

#ifdef DEBUG
  printf("Has received %i so far. Waiting for %i\n", header_size, msg_size);
#endif

  // get the message
  int s = 0;
  int left = msg_size - header_size;
  if (left > 0)
  {
    s = recvMsg(sockets[0], (void*) (msg + header_size), left, spent_cycles);
  }

  // get the id of the message
  *msg_id = *((long*) ((int*) msg + 1));

#ifdef DEBUG
  printf("[consumer %i] received message %li of size %i, should be %i\n",
      core_id, *msg_id, s + header_size, msg_size);
#endif

  free(msg);

  if (s + header_size == msg_size)
  {
    return msg_size;
  }
  else
  {
    return 0;
  }
}
