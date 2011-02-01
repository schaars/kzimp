#ifndef _MPSOC2_H
#define _MPSOC_H

/****
 * Memory Passing Sockets
 */

/* initialize mpsoc. Basically it creates the shared memory zone to handle m messages
 * at most at the same time for n processes. pathname is the name of an
 * existing file, required by ftok. Returns 0 on success, -1 for errors
 */
int	mpsoc_init(char* pathname, int num_replicas, int m);

/* Allocate a message of size len in shared mem.
 * Return the address of the message and (in nw variable)
 * the position of the message in the ring buffer
 */
void* mpsoc_alloc(size_t len, int *nw);

/* Send len bytes of buf to a (or more) replicas.
 * nw is the position of the message in the ring buffer
 * If dest = -1 then send to all replicas, else send to replica dest
 * Returns the number sent, or -1 for errors.
 */
ssize_t mpsoc_sendto(const void *buf, size_t len, int nw, int dest);

/* Read len bytes into *buf.
   Returns the number of bytes read or -1 for errors */
ssize_t mpsoc_recvfrom(void **buf, size_t len);

// destroys the shared area
void mpsoc_destroy(void);

#endif
