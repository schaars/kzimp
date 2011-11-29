/*
 * tcp_net.h
 *
 * Useful functions to use with the TCP sockets
 *
 */

#ifndef TCP_NET_H_
#define TCP_NET_H_

#include <stdint.h>

int recvMsg(int s, void *buf, size_t len, uint64_t *nb_cycles);

void sendMsg(int s, void *msg, int size, uint64_t *nb_cycles);

int recvMsg_nonBlock(int s, void *buf, size_t len);

#endif /* TCP_NET_H_ */
