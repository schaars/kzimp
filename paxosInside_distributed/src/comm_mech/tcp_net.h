/*
 * tcp_net.h
 *
 * Useful functions to use with the TCP sockets
 *
 */

#ifndef TCP_NET_H_
#define TCP_NET_H_

int recvMsg(int s, void *buf, size_t len);

void sendMsg(int s, void *msg, size_t size);

#endif /* TCP_NET_H_ */
