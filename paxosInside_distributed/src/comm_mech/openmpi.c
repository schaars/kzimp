/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: openmpi
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>

#include "mpi.h"
#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

// Define MESSAGE_MAX_SIZE as the max size of a message in the channel

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/*
 * explanations on node id:
 *  0 -> leader
 *  1 -> acceptor
 *  > 1 && < nb_paxos_nodes -> learners
 *  nb_paxos_nodes -> client that receives the responses from the learners
 *  > nb_paxos_nodes -> client that sends the requests to the learner
 */

/********** All the variables needed by openmpi **********/

static int node_id;
static int nb_paxos_nodes;
static int nb_learners;
static int nb_clients;
static int total_nb_nodes;
static MPI_Comm learner_bcast_comm;

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_learners = nb_paxos_nodes - 2;
  nb_clients = _nb_clients;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  int rc = MPI_Init(NULL, NULL);
  if (rc != MPI_SUCCESS)
  {
    printf("Error starting MPI program. Terminating.\n");
    MPI_Abort(MPI_COMM_WORLD, rc);
  }
}

static void init_node(int _node_id)
{
  node_id = _node_id;

  /* create communicator between the learner and the acceptors */
  if (node_id >= 1 && node_id < nb_paxos_nodes)
  {
    // Extract the original group handle
    MPI_Group orig_group, new_group;
    MPI_Comm_group(MPI_COMM_WORLD, &orig_group);

    // add the processes to the new group
    int ranges1[1][3];
    ranges1[0][0] = 1;
    ranges1[0][1] = nb_paxos_nodes - 1;
    ranges1[0][2] = 1;
    MPI_Group_range_incl(orig_group, 1, ranges1, &new_group);

    MPI_Comm_create(MPI_COMM_WORLD, new_group, &learner_bcast_comm);

    int rank, new_rank;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Group_rank(new_group, &new_rank);
    printf("Node %d; rank= %d newrank= %d\n", node_id, rank, new_rank);
  }

  MPI_Barrier(MPI_COMM_WORLD);
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  init_node(_node_id);
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  init_node(_client_id);
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

static void clean_node(void)
{
  MPI_Finalize();
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  clean_node();
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  clean_node();
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  MPI_Send(msg, length, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  MPI_Bcast(msg, length, MPI_CHAR, 1, learner_bcast_comm);
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  MPI_Send(msg, length, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
}

// send the message msg of size length to the client of id cid
// called by the learners
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
  MPI_Send(msg, length, MPI_CHAR, nb_paxos_nodes, 0, MPI_COMM_WORLD);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  if (node_id > 1 && node_id < nb_paxos_nodes)
  {
    MPI_Bcast(msg, length, MPI_CHAR, 1, learner_bcast_comm);
  }
  else
  {
    MPI_Status status;
    MPI_Recv(msg, MESSAGE_MAX_SIZE, MPI_CHAR, MPI_ANY_SOURCE, 0,
        MPI_COMM_WORLD, &status);
  }

  return MESSAGE_MAX_SIZE;
}
