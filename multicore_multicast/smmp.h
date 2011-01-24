/*
 * Shared Memory Message Passing
 * Version:     2010
 * Author:	Pierre Louis Aublin <pierre-louis.aublin@inria.fr>
 */

#ifndef _SMMP_H_
#define _SMMP_H_

#include "multicore_multicast.h"
/* multicore_multicast.h provides the following:
 *   MAX_NB_CORES
 *   MESSAGE_MAX_SIZE
 *   int core_id, nb_cores
 */

/* initialize the shared memory with max_nb_msg 
 * Return -1 if failed
 */
int smmp_init(char *pathname, int max_nb_msg);

/* send buf of size len to dest through shared memory.
 * If dest = -1 then send to all cores.
 * Return -1 if failed.
 */
int smmp_send(void *buf, int len, int dest);

/* receive a message in buf of size len from shared memory,
 * with a memcpy. Return -1 if failed.
 */
int smmp_recv_memcpy(void *buf, int len);

/* receive a message in buf of size *len from shared memory.
 * Return -1 if failed, the written length otherwise.
 */
int smmp_recv(void **buf, int *len);

/* Say that we read the message at address buf of size len,
 * updating the  bitmap for core core_id.
 * Return -1 if problem
 */
int smmp_message_is_read(void *buf, int len);

#endif
