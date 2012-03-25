/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: kzimp
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ipc_interface.h"
#include "mpi.h"

// debug macro
#define DEBUG
//#undef DEBUG

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by ULM **********/
static int node_id;
static int nb_nodes;


// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes)
{
  nb_nodes = _nb_nodes;

  int rc;

  rc = MPI_Init(&argc,&argv);
  if (rc != MPI_SUCCESS) {
     printf ("Error starting MPI program. Terminating.\n");
     MPI_Abort(MPI_COMM_WORLD, rc);
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  MPI_Barrier(MPI_COMM_WORLD);
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean resources created for the node.
void IPC_clean_node(void)
{
   MPI_Finalize();
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length)
{
  MPI_Bcast(msg, length, MPI_CHAR, 0, MPI_COMM_WORLD);
}

// send the message msg of size length to the node 0
void IPC_send_unicast(void *msg, size_t length, int nid)
{
  MPI_Send(msg, length, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
size_t IPC_receive(void *msg, size_t length)
{
  ssize_t recv_size;

  if (node_id == 0)
  {
     //recv a message from all nodes but me
     //TODO: I receive 1 message in this function. Use a global variable for round-robin receive.
     MPI_Status status;
     for (int j=0; j<nb_nodes; j++) {
       MPI_Recv(msg, MESSAGE_MAX_SIZE, MPI_CHAR, i, 0, MPI_COMM_WORLD, &status);
       recv_size = MESSAGE_MAX_SIZE;
     }
  }
  else
  {
    MPI_Bcast(msg, MESSAGE_MAX_SIZE_CHKPT_REQ, MPI_CHAR, 0, MPI_COMM_WORLD);
    recv_size = MESSAGE_MAX_SIZE_CHKPT_REQ;
  }

  return (size_t) recv_size;
}

#endif
