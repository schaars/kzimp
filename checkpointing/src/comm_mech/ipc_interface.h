/*
 * Communication mechanism: interface
 */

#ifndef IPC_INTERFACE_H
#define IPC_INTERFACE_H

#ifdef IPC_MSG_QUEUE
/* a message for IPC message queue */
struct ipc_message
{
  long mtype;
  char mtext[MESSAGE_MAX_SIZE]; // MESSAGE_MAX_SIZE is defined at compile time, when calling gcc
}__attribute__((__packed__, __aligned__(64)));
#endif

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

#if defined(IPC_MSG_QUEUE)

// send the message msg of size length to all the nodes
void IPC_send_multicast(struct ipc_message *msg, size_t length);

// send the message msg of size length to the node 0
void IPC_send_unicast(struct ipc_message *msg, size_t length, int nid);

#elif defined(ULM)

// allocate a message in shared memory.
// If dest is -1, then this message is going to be multicast.
// Otherwise it is sent to node 0.
void* IPC_ulm_alloc(size_t len, int *msg_pos_in_ring_buffer, int dest);

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length, int msg_pos_in_ring_buffer);

// send the message msg of size length to the node 0
void IPC_send_unicast(void *msg, size_t length, int nid,
    int msg_pos_in_ring_buffer);

#else

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length);

// send the message msg of size length to the node 0
void IPC_send_unicast(void *msg, size_t length, int nid);

#endif

#ifdef IPC_MSG_QUEUE

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
size_t IPC_receive(struct ipc_message *msg, size_t length);

#else

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
size_t IPC_receive(void *msg, size_t length);

#endif

#endif /* IPC_INTERFACE_H */
