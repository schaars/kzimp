/*
 * Communication mechanism: interface
 */

#ifndef IPC_INTERFACE_H
#define IPC_INTERFACE_H

// Initialize resources for everyone
// First initialization function called
void IPC_initialize(int _nb_paxos_nodes);

// Initialize resources for the node of id _node_id
void IPC_initialize_node(int _node_id);

// Clean resource
// Called by the parent process, after the death of the children.
void IPC_clean(void);

// Clean resources created for the node.
void IPC_clean_node(void);

// send the message msg of size length to all the nodes
void IPC_send_all_nodes(void *msg, size_t length);

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length);

#endif /* IPC_INTERFACE_H */
