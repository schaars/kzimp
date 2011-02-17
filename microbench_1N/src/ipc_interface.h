/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: interface
 */

#include <stdint.h>

/* the minimal size of a malloc, used so that there is no problem in the cache */
#define CACHE_LINE_SIZE 64
#define GET_MALLOC_SIZE(a) ((a) >= (CACHE_LINE_SIZE) ? (a) : (CACHE_LINE_SIZE))

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

// Return the number of cycles spent in the send() operation
uint64_t get_cycles_send();

// Return the number of cycles spent in the recv() operation
uint64_t get_cycles_recv();

// Send a message to all the cores
// The message id will be msg_id
void IPC_sendToAll(int msg_size, char msg_id);

// Get a message for this core
// return the size of the received message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
int IPC_receive(int msg_size, char *msg_id);
