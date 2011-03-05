/*
 * Client.cc
 *
 * A client for PaxosInside
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "Client.h"
#include "Request.h"
#include "Response.h"
#include "comm_mech/ipc_interface.h"

#define MSG_DEBUG
//#undef MSG_DEBUG

Client::Client(int client_id, uint64_t nbi)
{
  cid = client_id;
  nb_iter = nbi;
  value = 0;

  printf("[Client %i].<new, %lu, %lu>\n", cid, nb_iter, value);
}

Client::~Client(void)
{
}

// do the client job
void Client::run(void)
{
  Message m;

  // in order to ensure that the paxos nodes are launched
  sleep(1);

  //todo: compute throughput
  //todo: compute latency?

  for (uint64_t i = 0; i < nb_iter; i++)
  {
    Request r(client_id(), next_value());

#ifdef MSG_DEBUG
    printf("Client %i is sending a request (%i, %lu)\n", client_id(), r.cid(),
        r.value());
#endif

    IPC_send_client_to_node(r.content(), r.length());

    size_t s = IPC_receive(m.content(), m.length());

#ifdef MSG_DEBUG
    printf(
        "Client %i has received a message of size %lu (%lu received) and tag %i\n",
        cid, (unsigned long) m.length(), (unsigned long) s, m.tag());
#endif

    if (m.tag() == RESPONSE)
    {
      handle_response((Response*) &m);
    }
    else
    {
      printf(
          "Client %i has received a non-valid message of size %lu (%lu received) and tag %i\n",
          cid, (unsigned long) m.length(), (unsigned long) s, m.tag());
    }
  }

#ifdef MSG_DEBUG
  printf("Client %i has finished its %lu iterations\n", client_id(), nb_iter);
#endif

  while (1) {
    sleep(1);
  }
}

void Client::handle_response(Response* r)
{
  uint64_t value = r->value();

#ifdef MSG_DEBUG
  printf("Client %i handles response %lu\n", cid, value);
#endif
}
