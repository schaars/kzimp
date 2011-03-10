/*
 * Client.cc
 *
 * A client for PaxosInside
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Client.h"
#include "Message.h"
#include "Request.h"
#include "Response.h"
#include "comm_mech/ipc_interface.h"
#include "time.h"

#define MSG_DEBUG
//#undef MSG_DEBUG

Client::Client(int client_id, uint64_t nbi)
{
  cid = client_id;
  nb_iter = nbi;
  value = 0;

  init_clock_mhz();

  printf("[Client %i].<new, %lu, %lu>\n", cid, nb_iter, value);
}

Client::~Client(void)
{
}

// do the client job
void Client::run(void)
{
  Message m;
  uint64_t thr_start_time, thr_stop_time;

  // in order to ensure that the paxos nodes are launched
  sleep(2);

  printf("Client %i is starting %lu iterations\n", client_id(), nb_iter);

  rdtsc(thr_start_time);

  for (uint64_t i = 0; i < nb_iter; i++)
  {
    expected_value = next_value();
    Request r(client_id(), expected_value);

#ifdef MSG_DEBUG
    printf("Client %i is sending a request (%i, %lu)\n", client_id(), r.cid(),
        r.value());
#endif

#ifdef ULM
    IPC_send_client_to_node(r.content(), r.length(), r.get_msg_pos());
#else
    IPC_send_client_to_node(r.content(), r.length());
#endif

    while (1)
    {
      size_t s = IPC_receive(m.content(), m.length());

#ifdef MSG_DEBUG
      printf(
          "Client %i has received a message of size %lu (%lu received) and tag %i\n",
          cid, (unsigned long) m.length(), (unsigned long) s, m.tag());
#endif

      if (s > MESSAGE_HEADER_SIZE && m.tag() == RESPONSE && handle_response(
          (Response*) &m))
      {
        break;
      }
      else
      {
        // either this message is not valid or the received value is not the expected one
      }
    }
  }

  rdtsc(thr_stop_time);

#ifdef MSG_DEBUG
  printf("Client %i has finished its %lu iterations\n", client_id(), nb_iter);
#endif

  // compute throughput
  // thr_elapsed_time is in usec
  uint64_t thr_elapsed_time = diffTime(thr_stop_time, thr_start_time);
  double elapsed_time_sec = (double) thr_elapsed_time / 1000000.0;
  double throughput = ((double) nb_iter) / elapsed_time_sec;

  FILE *results_file = fopen(LOG_FILE, "a");
  if (!results_file)
  {
    fprintf(stderr, "[Client %i] Unable to open %s in append mode.\n",
        client_id(), LOG_FILE);
  }

  printf("Client= %i\tthr= %f prop/sec\ttime= %f sec\n", client_id(),
      throughput, elapsed_time_sec);

  if (results_file)
  {
    fprintf(results_file, "Client= %i\tthr= %f prop/sec\ttime= %f sec\n",
        client_id(), throughput, elapsed_time_sec);
  }

  fclose(results_file);

  fflush(NULL);
  sync();

  // create an empty file to announce that this client has finished
  char filename[256];
  sprintf(filename, "/tmp/paxosInside_client_%i_finished", client_id());
  int fd = open(filename, O_WRONLY | O_CREAT, 0666);
  close(fd);

  while (1)
  {
    sleep(1);
  }
}

bool Client::handle_response(Response* r)
{
  uint64_t value = r->value();

  return (value == expected_value);

#ifdef MSG_DEBUG
  printf("Client %i handles response %lu\n", cid, value);
#endif
}
