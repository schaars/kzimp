/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: POSIX message queue
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <mqueue.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ipc_interface.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by POSIX message queues **********/

#define MIN_MSG_SIZE (sizeof(char))

#define MAX(a, b)     (((a) > (b)) ? (a) : (b))

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes
static int msg_max_size_in_queue;

static uint64_t nb_cycles_send;
static uint64_t nb_cycles_recv;
static uint64_t nb_cycles_first_recv;

static mqd_t *consumers;
static mqd_t consumer_queue; // pointer to this consumer's queue for reading

// Initialize resources for both the producer and the consumers
// First initialization function called
void IPC_initialize(int _nb_receivers, int _request_size)
{
   nb_receivers = _nb_receivers;

   request_size = _request_size;
   if (request_size < MIN_MSG_SIZE)
   {
      request_size = MIN_MSG_SIZE;
   }

   nb_cycles_send = 0;
   nb_cycles_recv = 0;
   nb_cycles_first_recv = 0;

   consumers = (mqd_t*) malloc(sizeof(mqd_t) * nb_receivers);
   if (!consumers)
   {
      perror("Allocation error");
      exit(errno);
   }


   /****** get and set message queues properties ******/
   struct rlimit rlim;
   int res;

#ifdef DEBUG
   res = getrlimit(RLIMIT_MSGQUEUE, &rlim);
   if (res == -1) {
      perror("getrlimit error");
   } else {
      printf("===== BEFORE MODIFICATION\nsoft=%i, hard=%i\n", (int)rlim.rlim_cur, (int)rlim.rlim_max);
   }
#endif


   rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
   res = setrlimit(RLIMIT_MSGQUEUE, &rlim);
   if (res == -1) {
      perror("setrlimit error");
      exit(-1);
   }

#ifdef DEBUG
   res = getrlimit(RLIMIT_MSGQUEUE, &rlim);
   if (res == -1) {
      perror("getrlimit error");
   } else {
      printf("===== AFTER MODIFICATION\nsoft=%i, hard=%i\n", (int)rlim.rlim_cur, (int)rlim.rlim_max);
   }
#endif


   int i;
   char filename[256];

   msg_max_size_in_queue = 0;
   for (i = 0; i < nb_receivers; i++)
   {
      sprintf(filename, "/posix_message_queue_microbench%i", i + 1);

      if ((consumers[i] = mq_open(filename, O_RDWR | O_CREAT, 0600, NULL)) == -1)
      {
         perror("mq_open");
         exit(1);
      }

      struct mq_attr attr;
      int ret = mq_getattr(consumers[i], &attr);
      if (!ret)
      {
         msg_max_size_in_queue = MAX(msg_max_size_in_queue, attr.mq_msgsize);

#ifdef DEBUG
         printf(
               "New queue. Attributes are:\n\tflags=%li, maxmsg=%li, msgsize=%li, curmsgs=%li\n",
               attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);
#endif
      }
      else
      {
         perror("mq_getattr");
      }
   }
}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
   core_id = _core_id;
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
   core_id = _core_id;

   consumer_queue = consumers[core_id - 1];
}

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
   int i;

   sleep(1);

   char filename[256];
   for (i = 0; i < nb_receivers; i++)
   {
      sprintf(filename, "/posix_message_queue_microbench%i", i + 1);
      mq_unlink(filename);
   }
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
   int i;

   for (i = 0; i < nb_receivers; i++)
   {
      mq_close(consumers[i]);
   }
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
   IPC_clean_producer();
}

// Return the number of cycles spent in the send() operation
uint64_t get_cycles_send()
{
   return nb_cycles_send;
}

// Return the number of cycles spent in the recv() operation
uint64_t get_cycles_recv()
{
   return nb_cycles_recv - nb_cycles_first_recv;
}

// Send a message to all the cores
// The message id will be msg_id
void IPC_sendToAll(int msg_size, char msg_id)
{
   uint64_t cycle_start, cycle_stop;
   int i;
   char *msg;

   if (msg_size < MIN_MSG_SIZE)
   {
      msg_size = MIN_MSG_SIZE;
   }

   msg = (char*) malloc(GET_MALLOC_SIZE(sizeof(char) * msg_size));
   if (!msg)
   {
      perror("IPC_sendToAll allocation error! ");
      exit(errno);
   }

   // malloc is lazy: the pages may not be really allocated yet.
   // We force the allocation and the fetch of the pages with bzero
   bzero(msg, msg_size);

   msg[0] = msg_id;

#ifdef DEBUG
   printf(
         "[producer %i] going to send message %i of size %i to %i recipients\n",
         core_id, msg[0], msg_size, nb_receivers);
#endif

   for (i = 0; i < nb_receivers; i++)
   {
      // writing the content
    #ifdef COMPUTE_CYCLES
  rdtsc(cycle_start);
#endif
      mq_send(consumers[i], msg, msg_size, 0);
    #ifdef COMPUTE_CYCLES
  rdtsc(cycle_stop);
#endif

      nb_cycles_send += cycle_stop - cycle_start;
   }

   free(msg);
}

// Get a message for this core
// return the size of the message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
int IPC_receive(int msg_size, char *msg_id)
{
   char *msg;

   if (msg_size < MIN_MSG_SIZE)
   {
      msg_size = MIN_MSG_SIZE;
   }

   msg = (char*) malloc(sizeof(char) * msg_max_size_in_queue);
   if (!msg)
   {
      perror("IPC_receive allocation error! ");
      exit(errno);
   }

#ifdef DEBUG
   printf("Waiting for a new message\n");
#endif

   uint64_t cycle_start, cycle_stop;

 #ifdef COMPUTE_CYCLES
  rdtsc(cycle_start);
#endif
   int recv_size = mq_receive(consumer_queue, msg, msg_max_size_in_queue, NULL);
 #ifdef COMPUTE_CYCLES
  rdtsc(cycle_stop);
#endif

   nb_cycles_recv += cycle_stop - cycle_start;

   if (nb_cycles_first_recv == 0)
   {
      nb_cycles_first_recv = nb_cycles_recv;
   }

   // get the id of the message
   *msg_id = msg[0];

#ifdef DEBUG
   printf(
         "[consumer %i] received message %i of size %i, should be %i\n",
         core_id, *msg_id, recv_size, msg_size);
#endif

   free(msg);

   if (recv_size == msg_size)
   {
      return msg_size;
   }
   else
   {
      return 0;
   }
}
