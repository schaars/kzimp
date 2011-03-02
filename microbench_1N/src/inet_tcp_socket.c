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
#define DEBUG
#undef DEBUG

/********** All the variables needed by TCP sockets **********/

#define BOOTSTRAP_PORT 4242

#define MIN_MSG_SIZE (sizeof(char))

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes

static uint64_t nb_cycles_send;
static uint64_t nb_cycles_recv;
static uint64_t nb_cycles_first_recv;

// if INET_SYSCALLS_MEASUREMENT then the number of cycles is counted.
// The value is modified directly by tcp_net.c
#ifdef INET_SYSCALLS_MEASUREMENT
uint64_t nb_syscalls_send;
uint64_t nb_syscalls_recv;
uint64_t nb_syscalls_first_recv;
#endif

static int *sockets; // sockets used to communicate

// Initialize resources for both the producer and the consumers
// First initialization function called
void IPC_initialize(int _nb_receivers, int _request_size)
{
  nb_receivers = _nb_receivers;

  request_size = _request_size;
  if (request_size < MIN_MSG_SIZE)
  {
    request_size = MIN_MSG_SIZE;
  }

  nb_cycles_send = 0;
  nb_cycles_recv = 0;
  nb_cycles_first_recv = 0;

#ifdef INET_SYSCALLS_MEASUREMENT
  nb_syscalls_send = 0;
  nb_syscalls_recv = 0;
  nb_syscalls_first_recv = 0;
#endif
}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  int bootstrap_socket;
  struct sockaddr_in bootstrap_sin;
  int i;

#ifdef TCP_NAGLE
  int flag;
#endif

  core_id = _core_id;

  // create bootstrap socket
  bootstrap_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (bootstrap_socket == -1)
  {
    perror("[IPC_initialize_producer] Error while creating the socket! ");
    exit(errno);
  }

  // TCP NO DELAY
#ifdef TCP_NAGLE
  flag = 1;
  int result = setsockopt(bootstrap_socket, IPPROTO_TCP, TCP_NODELAY,
      (char*) &flag, sizeof(int));
  if (result == -1)
  {
    perror("[IPC_initialize_producer] Error while setting TCP NO DELAY! ");
  }
#endif

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
#ifdef TCP_NAGLE
    int flag = 1;
    int result = setsockopt(sockets[i], IPPROTO_TCP, TCP_NODELAY,
        (char *) &flag, sizeof(int));
    if (result == -1)
    {
      perror(
          "[IPC_initialize_producer] Error while setting TCP NO DELAY for accepted socket! ");
    }
#endif

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
#ifdef TCP_NAGLE
  int flag = 1;
  int result = setsockopt(sockets[0], IPPROTO_TCP, TCP_NODELAY, (char*) &flag,
      sizeof(int));
  if (result == -1)
  {
    perror("[IPC_initialize_consumer] Error while setting TCP NO DELAY! ");
  }
#endif

  // connect to producer
  struct sockaddr_in addr;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(BOOTSTRAP_PORT);

  while (1)
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

  free(sockets);
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  close(sockets[0]);

  free(sockets);
}

// Return the number of cycles spent in the send() operation
uint64_t get_cycles_send()
{
  return nb_cycles_send;
}

// Return the number of cycles spent in the recv() operation
uint64_t get_cycles_recv()
{
  return nb_cycles_recv - nb_cycles_first_recv;
}

// Send a message to all the cores
// The message id will be msg_id
void IPC_sendToAll(int msg_size, char msg_id)
{
  int i;
  char msg[MESSAGE_MAX_SIZE];

  bzero(msg, msg_size);

  msg[0] = msg_id;

#ifdef DEBUG
  printf(
      "[producer %i] going to send message %i of size %i to %i recipients\n",
      core_id, msg[0], msg_size, nb_receivers);
#endif

  for (i = 0; i < nb_receivers; i++)
  {
    sendMsg(sockets[i], msg, msg_size, &nb_cycles_send);
  }
}

// Get a message for this core
// return the size of the message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
int IPC_receive(int msg_size, char *msg_id)
{
  char msg[MESSAGE_MAX_SIZE];

#ifdef DEBUG
  printf("Waiting for a new message\n");
#endif

  // read in one stroke the whole message
#ifdef ONE_SYSCALL_RECV
  int header_size = 0;
#else
  int header_size = recvMsg(sockets[0], (void*) msg, MIN_MSG_SIZE,
      &nb_cycles_recv);
#endif

  // get the message
  int s = 0;

  int left = msg_size - header_size;
  if (left > 0)
  {
    s = recvMsg(sockets[0], (void*) (msg + header_size), left, &nb_cycles_recv);
  }

#ifdef COMPUTE_CYCLES
  // forget the first message (this message is not counted in the statistics)
  if (nb_cycles_first_recv == 0)
  {
    nb_cycles_first_recv = nb_cycles_recv;
  }
#endif

#ifdef INET_SYSCALLS_MEASUREMENT
  if (nb_syscalls_first_recv == 0)
  {
    nb_syscalls_first_recv = nb_syscalls_recv;
  }
#endif

  // get the id of the message
  *msg_id = msg[0];

#ifdef DEBUG
  printf("[consumer %i] received message %i of size %i, should be %i\n",
      core_id, *msg_id, s + header_size, msg_size);
#endif

  if (s + header_size == msg_size)
  {
    return msg_size;
  }
  else
  {
    return 0;
  }
}
