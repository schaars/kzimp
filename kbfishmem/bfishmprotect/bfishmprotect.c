/* Barrelfish communication mechanism
 * Uses the module kbfishmem, which offers the required level of memory protection
 * Most of the code comes from Barrelfish OS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "bfishmprotect.h"

#define SENDER_TO_RECEIVER_OFFSET 4096
#define RECEIVER_TO_SENDER_OFFSET 8192

#define BARRIER()   __asm volatile ("" : : : "memory")

#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)

#undef BFISH_MPROTECT_DEBUG

/// Special message types
enum ump_msgtype
{
  UMP_MSG = 0, UMP_ACK = 1,
};

#define MAX_NB_CHANNELS 256

// an array: memory protection file number -> the 2 corresponding channels
struct ump_channel all_channels[MAX_NB_CHANNELS][2];

/* create 2 channels that will use mprotectfile mprotectfile of number n for memory protection */
int create_channel(char *mprotectfile, int n)
{
  if (n > MAX_NB_CHANNELS)
  {
    printf("[%s:%i] Memory protection file number is too high: %i > %i\n",
        __func__, __LINE__, n, MAX_NB_CHANNELS);
    return -1;
  }

  all_channels[n][0].mprotectfile_nb = n;
  all_channels[n][1].mprotectfile_nb = n;

  // allocate shared area for the futexes
  all_channels[n][0].send_chan.f = futex_init(mprotectfile, 's');
  all_channels[n][0].recv_chan.f = futex_init(mprotectfile, 'r');
  all_channels[n][1].send_chan.f = all_channels[n][0].recv_chan.f;
  all_channels[n][1].recv_chan.f = all_channels[n][0].send_chan.f;

#ifdef BFISH_MPROTECT_DEBUG
  printf("Initializing channels @ %i\n", n);
  printf("Futex sender 2 receiver @ %p\n", all_channels[n][0].send_chan.f);
  printf("Futex receiver 2 sender @ %p\n", all_channels[n][0].recv_chan.f);
#endif

  return 0;
}

/* destroy the channel that is using mprotectfile number n for memory protection */
void destroy_channel(int n)
{
  if (n > MAX_NB_CHANNELS)
  {
    printf("[%s:%i] Memory protection file number is too high: %i > %i\n",
        __func__, __LINE__, n, MAX_NB_CHANNELS);
  }

  futex_destroy(all_channels[n][0].send_chan.f);
  futex_destroy(all_channels[n][0].recv_chan.f);
}

/*
 * open a channel using the special file mprotectfile for memory protection.
 * The channel size is (for both direction) nb_messages * message_size.
 * is_receiver must be set to 1 if called by the receiver end, 0 otherwise.
 * nb is the memory protection file number
 * Return the new channel if ok.
 */
struct ump_channel open_channel(char *mprotectfile, int nb, int nb_messages,
    int message_size, int is_receiver)
{
  struct ump_channel *chan;
  ump_index_t i;

  if (is_receiver)
  {
    chan = &all_channels[nb][1];
  }
  else
  {
    chan = &all_channels[nb][0];
  }

  chan->inchanlen = (size_t) nb_messages * sizeof(struct ump_message);
  chan->outchanlen = (size_t) nb_messages * sizeof(struct ump_message);

  if (is_receiver)
  {
    chan->fd = open(mprotectfile, O_RDWR | O_CREAT, 0666);
    if (chan->fd < 0)
    {
      perror("Reader opening file.");
      exit(-1);
    }

    chan->recv_chan.buf = (struct ump_message*) mmap(NULL, chan->inchanlen,
        PROT_READ, MAP_SHARED, chan->fd, SENDER_TO_RECEIVER_OFFSET);
    if (!chan->recv_chan.buf)
    {
      perror("Reader read_area mmap.");
      exit(-1);
    }

    chan->send_chan.buf = (struct ump_message*) mmap(NULL, chan->outchanlen,
        PROT_WRITE, MAP_SHARED, chan->fd, RECEIVER_TO_SENDER_OFFSET);
    if (!chan->send_chan.buf)
    {
      perror("Reader write_area mmap.");
      exit(-1);
    }
  }
  else
  {
    chan->fd = open(mprotectfile, O_RDWR);
    if (chan->fd < 0)
    {
      perror("Reader opening file.");
      exit(-1);
    }

    chan->recv_chan.buf = (struct ump_message*) mmap(NULL, chan->inchanlen,
        PROT_READ, MAP_SHARED, chan->fd, RECEIVER_TO_SENDER_OFFSET);
    if (!chan->recv_chan.buf)
    {
      perror("Reader read_area mmap.");
      exit(-1);
    }

    chan->send_chan.buf = (struct ump_message*) mmap(NULL, chan->outchanlen,
        PROT_WRITE, MAP_SHARED, chan->fd, SENDER_TO_RECEIVER_OFFSET);
    if (!chan->send_chan.buf)
    {
      exit(-1);
      perror("Reader write_area mmap.");
    }
  }

  chan->recv_chan.dir = UMP_INCOMING;
  chan->send_chan.dir = UMP_OUTGOING;

  chan->recv_chan.bufmsgs = nb_messages;
  chan->send_chan.bufmsgs = nb_messages;

  for (i = 0; i < chan->send_chan.bufmsgs; i++)
  {
    chan->send_chan.buf[i].header.raw = 0;
  }

  chan->max_recv_msgs = nb_messages;
  chan->max_send_msgs = nb_messages;

  chan->recv_chan.epoch = 1;
  chan->recv_chan.pos = 0;

  chan->send_chan.epoch = 1;
  chan->send_chan.pos = 0;

  chan->sent_id = 1;
  chan->seq_id = 0;
  chan->ack_id = 0;
  chan->last_ack = 0;

  return *chan;
}

/*
 * close the channel whose structure is *chan.
 */
void close_channel(struct ump_channel *chan)
{
  munmap(chan->recv_chan.buf, chan->inchanlen);
  munmap(chan->send_chan.buf, chan->outchanlen);

  close(chan->fd);
}

void recv_ack(struct ump_channel *chan)
{
  struct ump_message *ump_msg;

  while (!ump_endpoint_can_recv(&chan->recv_chan))
  {
#ifdef BFISH_MPROTECT_DEBUG
    printf("[%s:%i] Going to lock futex @ %p for channel %i.\n", __func__,
        __LINE__, chan->recv_chan.f, chan->mprotectfile_nb);
#endif
    futex_lock(chan->recv_chan.f);
  }

  ump_msg = ump_impl_recv(&chan->recv_chan);
  if (ump_msg == NULL)
  {
    printf("[%s:%i] Error: ump_msg should not be null\n", __func__, __LINE__);
    exit(-1);
  }

  // what kind of message is this?
  int msgtype = ump_control_process(chan, ump_msg->header.control);
  switch (msgtype)
  {
  case UMP_ACK: // this is an ack, we need to call recv again
    //printf("[%s:%i] Has received an ack\n", __func__, __LINE__);
    break;
  case UMP_MSG: // this is a message, we return it
    printf("[%s:%i] Should not have received a message; expecting an ack\n",
        __func__, __LINE__);
    break;
  default:
    printf("[%s:%i] Error: unknown message type %i\n", __func__, __LINE__,
        msgtype);
    break;
  }
}

/*
 * send the message msg of size len through the channel *chan.
 * Is blocking.
 * Return the size of the sent message or an error code.
 */
int send_msg(struct ump_channel *chan, char *msg, size_t len)
{
  struct ump_control ctrl;
  struct ump_message *ump_msg;

  /*
   printf(
   "[%s:%i] sent_id=%hu, ack_id=%hu, max_send_msgs=%hu, seq_id=%hu, last_ack=%hu\n",
   __func__, __LINE__, chan->sent_id, chan->ack_id, chan->max_send_msgs,
   chan->seq_id, chan->last_ack);
   */

  while (!ump_can_send(chan))
  {
    //printf("[%s:%i] Going to receive an ack\n", __func__, __LINE__);
    recv_ack(chan);
  }

  //code to send:
  ump_msg = ump_impl_get_next(&chan->send_chan, &ctrl);
  len = min(MESSAGE_BYTES, len);
  memcpy(ump_msg->data, msg, len);
  ump_control_fill(chan, &ctrl, UMP_MSG);
  BARRIER();
  ump_msg->header.control = ctrl;

#ifdef BFISH_MPROTECT_DEBUG
  printf("[%s:%i] Going to unlock futex @ %p for channel %i.\n", __func__,
      __LINE__, chan->send_chan.f, chan->mprotectfile_nb);
#endif
  futex_unlock(chan->send_chan.f);

  return len;
}

/*
 * receive a message and place it in msg of size len.
 * Is blocking.
 * Return the size of the received message
 */
int recv_msg(struct ump_channel *chan, char *msg, size_t len)
{
  struct ump_control ctrl;
  struct ump_message *ump_msg;
  int call_recv_again;

  /*
   printf(
   "[%s:%i] sent_id=%hu, ack_id=%hu, max_send_msgs=%hu, seq_id=%hu, last_ack=%hu\n",
   __func__, __LINE__, chan->sent_id, chan->ack_id, chan->max_send_msgs,
   chan->seq_id, chan->last_ack);
   */

  while (!ump_endpoint_can_recv(&chan->recv_chan))
  {
#ifdef BFISH_MPROTECT_DEBUG
    printf("[%s:%i] Going to lock futex @ %p for channel %i.\n", __func__,
        __LINE__, chan->recv_chan.f, chan->mprotectfile_nb);
#endif
    futex_lock(chan->recv_chan.f);
  }

  ump_msg = ump_impl_recv(&chan->recv_chan);
  if (ump_msg == NULL)
  {
    printf("[%s:%i] Error: ump_msg should not be null\n", __func__, __LINE__);
    exit(-1);
  }

  // what kind of message is this?
  int msgtype = ump_control_process(chan, ump_msg->header.control);
  switch (msgtype)
  {
  case UMP_ACK: // this is an ack, we need to call recv again
    //printf("[%s:%i] Has received an ack\n", __func__, __LINE__);
    call_recv_again = 1;
    break;
  case UMP_MSG: // this is a message, we return it
    //printf("[%s:%i] Has received a message\n", __func__, __LINE__);
    len = min(len, MESSAGE_BYTES);
    memcpy(msg, ump_msg->data, len);
    call_recv_again = 0;
    break;
  default:
    printf("[%s:%i] Error: unknown message type %i\n", __func__, __LINE__,
        msgtype);
    call_recv_again = 1;
    break;
  }

  if (ump_send_ack_is_needed(chan))
  {
    /*
     printf(
     "[%s:%i] sent_id=%hu, ack_id=%hu, max_send_msgs=%hu, seq_id=%hu, last_ack=%hu\n",
     __func__, __LINE__, chan->sent_id, chan->ack_id, chan->max_send_msgs,
     chan->seq_id, chan->last_ack);
     printf("[%s:%i] I need to send an ack\n", __func__, __LINE__);
     */

    // this shouldn't happen: I have received a message, thus I have updated my information
    // concerning acks.
    if (!ump_can_send(chan))
    {
      printf("[%s:%i] I need to send an ack but I cannot\n", __func__, __LINE__);
      exit(-1);
    }

    ump_msg = ump_impl_get_next(&chan->send_chan, &ctrl);
    ump_control_fill(chan, &ctrl, UMP_ACK);
    BARRIER();
    ump_msg->header.control = ctrl;

#ifdef BFISH_MPROTECT_DEBUG
    printf("[%s:%i] Going to unlock futex @ %p for channel %i.\n", __func__,
        __LINE__, chan->send_chan.f, chan->mprotectfile_nb);
#endif
    futex_unlock(chan->send_chan.f);
  }

  if (call_recv_again)
  {
    //printf("[%s:%i] Going to receive again\n", __func__, __LINE__);
    return recv_msg(chan, msg, len);
  }
  else
  {
    return len;
  }
}

/*
 * receive a message and place it in msg of size len.
 * Is not blocking.
 * Return the size of the received message or 0 if there is no message or -1 if an error has occured (cannot send the ack)
 */
int recv_msg_nonblocking(struct ump_channel *chan, char *msg, size_t len)
{
  struct ump_control ctrl;
  struct ump_message *ump_msg;
  int call_recv_again;

  /*
   printf(
   "[%s:%i] sent_id=%hu, ack_id=%hu, max_send_msgs=%hu, seq_id=%hu, last_ack=%hu\n",
   __func__, __LINE__, chan->sent_id, chan->ack_id, chan->max_send_msgs,
   chan->seq_id, chan->last_ack);
   */

  if (!ump_endpoint_can_recv(&chan->recv_chan))
  {
    return 0;
  }

  ump_msg = ump_impl_recv(&chan->recv_chan);
  if (ump_msg == NULL)
  {
    printf("[%s:%i] Error: ump_msg should not be null\n", __func__, __LINE__);
    exit(-1);
  }

  // what kind of message is this?
  int msgtype = ump_control_process(chan, ump_msg->header.control);
  switch (msgtype)
  {
  case UMP_ACK: // this is an ack, we need to call recv again
    //printf("[%s:%i] Has received an ack\n", __func__, __LINE__);
    call_recv_again = 1;
    break;
  case UMP_MSG: // this is a message, we return it
    //printf("[%s:%i] Has received a message\n", __func__, __LINE__);
    len = min(len, MESSAGE_BYTES);
    memcpy(msg, ump_msg->data, len);
    call_recv_again = 0;
    break;
  default:
    printf("[%s:%i] Error: unknown message type %i\n", __func__, __LINE__,
        msgtype);
    call_recv_again = 1;
    break;
  }

  if (ump_send_ack_is_needed(chan))
  {
    /*
     printf(
     "[%s:%i] sent_id=%hu, ack_id=%hu, max_send_msgs=%hu, seq_id=%hu, last_ack=%hu\n",
     __func__, __LINE__, chan->sent_id, chan->ack_id, chan->max_send_msgs,
     chan->seq_id, chan->last_ack);
     printf("[%s:%i] I need to send an ack\n", __func__, __LINE__);
     */

    // this shouldn't happen: I have received a message, thus I have updated my information
    // concerning acks.
    if (!ump_can_send(chan))
    {
      printf("[%s:%i] I need to send an ack but I cannot\n", __func__, __LINE__);
      exit(-1);
    }

    ump_msg = ump_impl_get_next(&chan->send_chan, &ctrl);
    ump_control_fill(chan, &ctrl, UMP_ACK);
    BARRIER();
    ump_msg->header.control = ctrl;
  }

  if (call_recv_again)
  {
    return 0;
  }
  else
  {
    return len;
  }
}

// select on n channels that can be found in chans.
// Note that you give an array of channel pointers.
// Return NULL if there is no message, or a pointer to a channel on which a message is available.
// Before returning NULL (if there is no message), performs nb_iter iterations.
// If nb_iter is 0 then the call is blocking.
struct ump_channel* bfish_mprotect_select(struct ump_channel* chans, int l,
    int nb_iter)
{
  int i, n;

  n = 1;
  while (1)
  {
    for (i = 0; i < l; i++)
    {
      if (ump_endpoint_can_recv(&chans[i].recv_chan))
      {
        return &chans[i];
      }
    }

    n++;
    if (nb_iter > 0 && n >= nb_iter)
    {
      break;
    }

    //WAIT();
  }

  return NULL;
}

