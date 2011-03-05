/*
 * Communication mechanism: interface
 */

#include "Message.h"

// Initialize resources for everyone
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients);

// Initialize resources for the node of id _core_id
void IPC_initialize_node(int _core_id);

//todo: initialize client

// Clean ressource
// Called by the parent process, after the death of the children.
void IPC_clean(void);

// Clean ressources created for the producer.
void IPC_clean_node(void);

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length);

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length);

//todo: send to a client

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length);
