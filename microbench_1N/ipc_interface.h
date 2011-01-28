/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: interface
 */

#include <stdbool.h>

// Initialize resources for both the producer and the consumers
// First initialization function called
void IPC_initialize(int _nb_receivers, int _request_size);

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id);

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id);

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void);

// Clean ressources created for the producer.
void IPC_clean_producer(void);

// Clean ressources created for the consumer.
void IPC_clean_consumer(void);

// Send a message to all the cores
void IPC_sendToAll(int msg_size);

// Get a message for this core
// return 1 if the received message is valid, 0 otherwise
int IPC_receive(int msg_size);
