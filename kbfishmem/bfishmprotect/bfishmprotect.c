/* Barrelfish communication mechanism
 * Uses the module kbfishmem, which offers the required level of memory protection
 * Most of the code comes from Barrelfish OS.
 */

#include <stdio.h>

#include "bfishmprotect.h"

/********************* exported interface *********************/
/*
 * open a channel using the special file mprotectfile for memory protection.
 * The channel size is (for both direction) nb_messages * message_size.
 * is_receiver must be set to 1 if called by the receiver end, 0 otherwise.
 * Return the new channel if ok.
 */
struct ump_channel open_channel(char *mprotectfile, int nb_messages,
    int message_size, int is_receiver)
{
  struct ump_channel chan;

  //todo
  return chan;
}

/*
 * close the channel whose structure is *chan.
 */
void close_channel(struct ump_channel *chan)
{
  //todo
}

/*
 * send the message msg of size len through the channel *chan.
 * Is blocking.
 * Return the size of the sent message or an error code.
 */
int send_msg(struct ump_channel *chan, char *msg, size_t len)
{
  //todo
  return 0;
}

/*
 * receive a message and place it in msg of size len.
 * Is blocking.
 * Return the size of the received message.
 */
int recv_msg(struct ump_channel *chan, char *msg, size_t len)
{
  //todo
  return 0;
}

/*
 * receive a message and place it in msg of size len.
 * Is not blocking.
 * Return the size of the received message or 0 if there is no message
 */
int recv_msg_nonblocking(struct ump_channel *chan, char *msg, size_t len)
{
  //todo
  return 0;
}

// select on n channels that can be found in chans.
// Note that you give an array of channel pointers.
// Return NULL if there is no message, or a pointer to a channel on which a message is available.
struct ump_channel* select(struct ump_channel** chans, int n)
{
  //todo
  return NULL;
}
