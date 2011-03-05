/*
 * Communication mechanism: interface
 */

// Initialize resources for everyone
// First initialization function called
void IPC_initialize(int _nb_paxos_nodes, int _nb_clients);

// Initialize resources for the node of id _node_id
void IPC_initialize_node(int _node_id);

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id);

// Clean resource
// Called by the parent process, after the death of the children.
void IPC_clean(void);

// Clean resources created for the node.
void IPC_clean_node(void);

// Clean resources created for the producer.
void IPC_clean_client(void);

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length);

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length);

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length);

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid);

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length);
