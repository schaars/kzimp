#ifndef _MPSOC_H
#define _MPSOC_H

/****
 * Memory Passing Sockets
 */

/* initialize mpsoc. Basically it creates the shared memory zone to handle m messages
 * at most at the same time for n processes. pathname is the name of an
 * existing file, required by ftok. Returns 0 on success, -1 for errors
 */
int	mpsoc_init(char* pathname, int num_replicas, int m);

/* Send len bytes of buf to a (or more) replicas.
 * If dest = -1 then send to all replicas, else send to replica dest
 * Returns the number sent, or -1 for errors.
 */
ssize_t mpsoc_sendto(const void *buf, size_t len, int dest);

/* Read len bytes into buf.
   Returns the number of bytes read or -1 for errors */
ssize_t mpsoc_recvfrom(void *buf, size_t len);


#endif
