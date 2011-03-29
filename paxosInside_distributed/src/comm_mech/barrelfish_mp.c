/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Barrelfish message passing
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "ipc_interface.h"
#include "urpc.h"
#include "urpc_transport.h"
#include "../MessageTag.h"
#include "../Message.h"

// debug macro
#define DEBUG
#undef DEBUG

// Define NB_MESSAGES as the max number of messages in the channel
// Define URPC_MSG_WORDS as the size of the messages in uint64_t
// You can define USLEEP if you want to add a usleep(1) when busy waiting
// You can define NOP if you want to add a nop when busy waiting

#define NB_MSG_MAX_IN_TRANSIT NB_MESSAGES
//#define NB_MSG_MAX_IN_TRANSIT 1

// size of an ack in uint64_t
#define ACK_SIZE 1

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by Barrelfish message passing **********/

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int nb_learners;
static int total_nb_nodes;

static int nb_messages_in_transit_rcv; // Barrelfish requires an acknowledgement of sent messages. We send one peridically
static int nb_messages_in_transit_snd; // We send one peridically

// the leader and the clients need an array of nb_messages_in_transit:
//   -for the client: 1 counter per learner
//   -for the leader: 1 counter per client
static int* nb_messages_in_transit_multi;

static size_t buffer_size;
static size_t connection_size;

static void* *clients_to_leader_shmem; // shared areas between the clients and the leader
static void* leader_to_acceptor_shmem; // shared area between the leader and the acceptor
static void* *acceptor_to_learners_shmem; // shared areas between the acceptor and the learners
static void* **learners_to_client_shmem; // shared areas between the learners and the clients

static struct urpc_connection *clients_to_leader; // connection between client i and the leader
static struct urpc_connection leader_to_acceptor; // connection between leader and acceptor
static struct urpc_connection *acceptor_to_learners; // connection between the acceptor and learner i
static struct urpc_connection **learners_to_clients; // connection between the learner l and the client c

/* init a shared area of size s with pathname p and project id i.
 * Return a pointer to it, or NULL in case of errors.
 */
void* init_shared_memory_segment(char *p, size_t s, int i)
{
  key_t key;
  int shmid;
  void* ret;

  ret = NULL;

  key = ftok(p, i);
  if (key == -1)
  {
    printf("Did you touch %s?\n", p);
    perror("ftok error: ");
    return ret;
  }

  printf("Size of the shared memory segment to create: %qu\n",
      (unsigned long long) s);

  shmid = shmget(key, s, IPC_CREAT | 0666);
  if (shmid == -1)
  {
    int errsv = errno;

    perror("shmget error: ");

    switch (errsv)
    {
    case EACCES:
      printf("errno = EACCES\n");
      break;
    case EINVAL:
      printf("errno = EINVAL\n");
      break;
    case ENFILE:
      printf("errno = ENFILE\n");
      break;
    case ENOENT:
      printf("errno = ENOENT\n");
      break;
    case ENOMEM:
      printf("errno = ENOMEM\n");
      break;
    case ENOSPC:
      printf("errno = ENOSPC\n");
      break;
    case EPERM:
      printf("errno = EPERM\n");
      break;
    default:
      printf("errno = %i\n", errsv);
      break;
    }

    return ret;
  }

  ret = shmat(shmid, NULL, 0);

  return ret;
}

// create all the shared areas of memory
void initialize_shared_areas(void)
{
  // clients to leader
  clients_to_leader_shmem = (void**) malloc(sizeof(void*) * nb_clients);
  if (!clients_to_leader_shmem)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (int i = 0; i < nb_clients; i++)
  {
    clients_to_leader_shmem[i] = init_shared_memory_segment(
        (char*) "/tmp/paxosInside_barrelfish_mp_clients_to_leader_shmem",
        connection_size, 'a' + i);
  }

  // leader to acceptor
  leader_to_acceptor_shmem = init_shared_memory_segment(
      (char*) "/tmp/paxosInside_barrelfish_mp_leader_to_acceptor_shmem",
      connection_size, 'a');

  // acceptor to learners
  acceptor_to_learners_shmem = (void**) malloc(sizeof(void*) * nb_learners);
  if (!acceptor_to_learners_shmem)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (int i = 0; i < nb_learners; i++)
  {
    acceptor_to_learners_shmem[i] = init_shared_memory_segment(
        (char*) "/tmp/paxosInside_barrelfish_mp_acceptor_to_learners_shmem",
        connection_size, 'a' + i);
  }

  // learners to client
  learners_to_client_shmem = (void***) malloc(sizeof(void**) * nb_learners);
  if (!learners_to_client_shmem)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (int l = 0; l < nb_learners; l++)
  {
    learners_to_client_shmem[l] = (void**) malloc(sizeof(void*) * nb_clients);
    if (!learners_to_client_shmem[l])
    {
      perror("Allocation error");
      exit(errno);
    }

    for (int c = 0; c < nb_clients; c++)
    {
      char filename[256];
      sprintf(filename,
          "/tmp/paxosInside_barrelfish_mp_learner_%i_to_client_%i_shmem", l, c);

      learners_to_client_shmem[l][c] = init_shared_memory_segment(filename,
          connection_size, 'a' + c);
    }
  }
}

// initialize all the channels, according to this node's id
void intialize_channels()
{
  // initialize the channels between the clients and the leader
  clients_to_leader = (struct urpc_connection*) malloc(
      sizeof(struct urpc_connection) * nb_clients);
  if (!clients_to_leader)
  {
    perror("Allocation error");
    exit(errno);
  }

  if (node_id == 0)
  {
    for (int i = 0; i < nb_clients; i++)
    {
      urpc_transport_create(node_id, clients_to_leader_shmem[i],
          connection_size, buffer_size, &clients_to_leader[i], true);
    }
  }
  else if (node_id >= nb_paxos_nodes)
  {
    urpc_transport_create(node_id, clients_to_leader_shmem[node_id
        - nb_paxos_nodes], connection_size, buffer_size,
        &clients_to_leader[node_id - nb_paxos_nodes], false);

  }

  // initialize the channel between the leader and the acceptor
  if (node_id == 0)
  {
    urpc_transport_create(node_id, leader_to_acceptor_shmem, connection_size,
        buffer_size, &leader_to_acceptor, true);
  }
  else if (node_id == 1)
  {
    urpc_transport_create(node_id, leader_to_acceptor_shmem, connection_size,
        buffer_size, &leader_to_acceptor, false);
  }

  // initialize the channels between the acceptor and the learners
  acceptor_to_learners = (struct urpc_connection*) malloc(
      sizeof(struct urpc_connection) * nb_learners);
  if (!acceptor_to_learners)
  {
    perror("Allocation error");
    exit(errno);
  }

  if (node_id == 1)
  {
    for (int i = 0; i < nb_learners; i++)
    {
      urpc_transport_create(node_id, acceptor_to_learners_shmem[i],
          connection_size, buffer_size, &acceptor_to_learners[i], true);
    }
  }
  else if (node_id >= 2 && node_id < nb_paxos_nodes)
  {
    urpc_transport_create(node_id, acceptor_to_learners_shmem[node_id - 2],
        connection_size, buffer_size, &acceptor_to_learners[node_id - 2], false);
  }

  learners_to_clients = (struct urpc_connection**) malloc(
      sizeof(struct urpc_connection*) * nb_learners);
  if (!learners_to_clients)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (int l = 0; l < nb_learners; l++)
  {
    learners_to_clients[l] = (struct urpc_connection*) malloc(
        sizeof(struct urpc_connection) * nb_clients);
    if (!learners_to_clients[l])
    {
      perror("Allocation error");
      exit(errno);
    }

    for (int c = 0; c < nb_clients; c++)
    {
      if (node_id >= nb_paxos_nodes)
      {
        if (c == node_id - nb_paxos_nodes)
        {
          urpc_transport_create(node_id, learners_to_client_shmem[l][c],
              connection_size, buffer_size, &learners_to_clients[l][c], false);
        }
      }
      else if (node_id >= 2)
      {
        urpc_transport_create(node_id, learners_to_client_shmem[l][c],
            connection_size, buffer_size, &learners_to_clients[l][c], true);
      }
    }
  }
}

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  nb_learners = nb_paxos_nodes - 2;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  size_t urpc_msg_word = URPC_MSG_WORDS;
  size_t nb_messages = NB_MESSAGES;
  buffer_size = urpc_msg_word * 8 * nb_messages;
  connection_size = buffer_size * 2 + 2 * URPC_CHANNEL_SIZE;
  nb_messages_in_transit_rcv = nb_messages_in_transit_snd = 0;

  initialize_shared_areas();
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  intialize_channels();

  if (node_id == 0)
  {
    nb_messages_in_transit_multi = (int*) malloc(sizeof(int) * nb_clients);
    if (!nb_messages_in_transit_multi)
    {
      perror("Allocation error");
      exit(errno);
    }

    for (int i = 0; i < nb_clients; i++)
    {
      nb_messages_in_transit_multi[i] = 0;
    }
  }
  else
  {
    nb_messages_in_transit_multi = NULL;
  }
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  node_id = _client_id;

  intialize_channels();

  nb_messages_in_transit_multi = (int*) malloc(sizeof(int) * nb_learners);
  if (!nb_messages_in_transit_multi)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (int i = 0; i < nb_learners; i++)
  {
    nb_messages_in_transit_multi[i] = 0;
  }
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

void free_all(void)
{
  free(clients_to_leader_shmem);
  free(acceptor_to_learners_shmem);

  for (int l = 0; l < nb_learners; l++)
  {
    free(learners_to_client_shmem[l]);
  }
  free(learners_to_client_shmem);

  free(clients_to_leader_shmem);
  free(acceptor_to_learners_shmem);

  for (int l = 0; l < nb_learners; l++)
  {
    free(learners_to_client_shmem[l]);
  }
  free(learners_to_client_shmem);
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  free_all();

  if (node_id == 0)
  {
    free(nb_messages_in_transit_multi);
  }
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  free_all();

  free(nb_messages_in_transit_multi);
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  urpc_transport_send(&leader_to_acceptor, msg, length / sizeof(uint64_t));

  nb_messages_in_transit_snd++;

  if (nb_messages_in_transit_snd == NB_MSG_MAX_IN_TRANSIT)
  {
    Message m;

#ifdef DEBUG
    printf("Node %i is going to receive an ack from the acceptor\n", node_id);
#endif

    urpc_transport_recv(&leader_to_acceptor, (void*) m.content(), ACK_SIZE);

#ifdef DEBUG
    printf("Node %i has received an ack from the acceptor\n", node_id);
#endif

    nb_messages_in_transit_snd = 0;
  }
}

// send the message msg of size length to all the learners
void IPC_send_node_multicast(void *msg, size_t length)
{
  for (int l = 0; l < nb_learners; l++)
  {
#ifdef DEBUG
    printf("Node %i is going to send a multicast message to learner %i\n",
        node_id, l + 2);
#endif
    urpc_transport_send(&acceptor_to_learners[l], msg, length
        / sizeof(uint64_t));
  }

#ifdef DEBUG
  printf("Node %i has finished to send multicast messages\n", node_id);
#endif

  nb_messages_in_transit_snd++;

  if (nb_messages_in_transit_snd == NB_MSG_MAX_IN_TRANSIT)
  {
    Message m;

#ifdef DEBUG
    printf("Node %i is going to receive acks from the learners\n", node_id);
#endif

    for (int l = 0; l < nb_learners; l++)
    {
#ifdef DEBUG
      printf("Node %i is going to receive an ack from learner %i\n", node_id, l);
#endif

      urpc_transport_recv(&acceptor_to_learners[l], (void*) m.content(),
          ACK_SIZE);

#ifdef DEBUG
      printf("Node %i has received an ack from leader %i\n", node_id, l);
#endif
    }

    nb_messages_in_transit_snd = 0;
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  urpc_transport_send(&clients_to_leader[node_id - nb_paxos_nodes], msg, length
      / sizeof(uint64_t));

  nb_messages_in_transit_snd++;

  if (nb_messages_in_transit_snd == NB_MSG_MAX_IN_TRANSIT)
  {
    Message m;

#ifdef DEBUG
    printf("Node %i is going to receive an ack from the leader\n", node_id);
#endif

    urpc_transport_recv(&clients_to_leader[node_id - nb_paxos_nodes],
        (void*) m.content(), ACK_SIZE);

#ifdef DEBUG
    printf("Node %i has received an ack from the leader\n", node_id);
#endif

    nb_messages_in_transit_snd = 0;
  }
}

// send the message msg of size length to the client of id cid
// called by the learners
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
#ifdef DEBUG
  printf("Node %i is going to send a message to client %i\n", node_id, cid);
#endif

  urpc_transport_send(&learners_to_clients[node_id - 2][0], msg, length
      / sizeof(uint64_t));

  nb_messages_in_transit_snd++;

  if (nb_messages_in_transit_snd == NB_MSG_MAX_IN_TRANSIT)
  {
    Message m;

#ifdef DEBUG
    printf("Node %i is going to receive an ack from client %i\n", node_id, cid);
#endif

    urpc_transport_recv(&learners_to_clients[node_id - 2][0],
        (void*) m.content(), ACK_SIZE);

#ifdef DEBUG
    printf("Node %i has received an ack from client %i\n", node_id, cid);
#endif

    nb_messages_in_transit_snd = 0;
  }
}

// return a message received by node 0
// This is the leader. It receives messages from the clients
size_t recv_for_node0(void *msg, size_t length)
{
  Message m;
  size_t recv_size;

  while (1)
  {
    for (int i = 0; i < nb_clients; i++)
    {
      recv_size = urpc_transport_recv_nonblocking(&clients_to_leader[i],
          (void*) msg, length / sizeof(uint64_t));

      if (recv_size > 0)
      {
        nb_messages_in_transit_multi[i]++;

        if (nb_messages_in_transit_multi[i] == NB_MSG_MAX_IN_TRANSIT)
        {
#ifdef DEBUG
          printf("Node %i is sending an ack to client %i\n", node_id, i);
#endif

          urpc_transport_send(&clients_to_leader[i], m.content(), ACK_SIZE);

          nb_messages_in_transit_multi[i] = 0;
        }

        return recv_size;
      }
    }

    // sleep
#ifdef USLEEP
    usleep(1);
#endif
#ifdef NOP
    __asm__ __volatile__("nop");
#endif
  }
}

// return a message received by node 0
// This is the leader. It receives messages from the clients
size_t recv_for_client(void *msg, size_t length)
{
  Message m;
  size_t recv_size;

  while (1)
  {
    for (int i = 0; i < nb_learners; i++)
    {
      recv_size = urpc_transport_recv_nonblocking(
          &learners_to_clients[i][node_id - nb_paxos_nodes], (void*) msg,
          length / sizeof(uint64_t));

      if (recv_size > 0)
      {
        nb_messages_in_transit_multi[i]++;

        if (nb_messages_in_transit_multi[i] == NB_MSG_MAX_IN_TRANSIT)
        {
#ifdef DEBUG
          printf("Node %i is sending an ack to learner %i\n", node_id, i);
#endif

          urpc_transport_send(
              &learners_to_clients[i][node_id - nb_paxos_nodes], m.content(),
              ACK_SIZE);

          nb_messages_in_transit_multi[i] = 0;
        }

        return recv_size;
      }
    }

    // sleep
#ifdef USLEEP
    usleep(1);
#endif
#ifdef NOP
    __asm__ __volatile__("nop");
#endif
  }
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  Message m;
  size_t recv_size;

  if (node_id == 0)
  {
    recv_size = recv_for_node0(msg, length);
  }
  else if (node_id == 1)
  {
    recv_size = urpc_transport_recv(&leader_to_acceptor, msg, length);

    nb_messages_in_transit_rcv++;

    if (nb_messages_in_transit_rcv == NB_MSG_MAX_IN_TRANSIT)
    {
#ifdef DEBUG
      printf("Node %i is sending an ack to the leader\n", node_id);
#endif

      urpc_transport_send(&leader_to_acceptor, m.content(), ACK_SIZE);

      nb_messages_in_transit_rcv = 0;
    }
  }
  else if (node_id < nb_paxos_nodes)
  {
    recv_size = urpc_transport_recv(&acceptor_to_learners[node_id - 2], msg,
        length);

    nb_messages_in_transit_rcv++;

    if (nb_messages_in_transit_rcv == NB_MSG_MAX_IN_TRANSIT)
    {
#ifdef DEBUG
      printf("Node %i is sending an ack to the acceptor\n", node_id);
#endif

      urpc_transport_send(&acceptor_to_learners[node_id - 2], m.content(),
          ACK_SIZE);

      nb_messages_in_transit_rcv = 0;
    }
  }
  else
  {
    recv_size = recv_for_client(msg, length);
  }

  return recv_size * sizeof(uint64_t);
}
