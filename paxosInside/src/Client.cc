/*
 * Client.cc
 *
 * A client for PaxosInside
 */

#include <stdio.h>
#include <stdint.h>

#include "Client.h"
#include "Request.h"

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
  //todo: compute throughput
  //todo: compute latency?

  while (nb_iter > 0)
  {
    Request r(client_id(), next_value());

    // todo: send r to the leader
    // todo: recv response

    nb_iter--;
  }
}

