/*
 * Checkpointer.cc
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "Checkpointer.h"
#include "CheckpointMessage.h"
#include "comm_mech/ipc_interface.h"

// to get some debug printf
#define MSG_DEBUG
#undef MSG_DEBUG

Checkpointer::Checkpointer(int _node_id)
{
  nid = _node_id;

  //todo: init checkpoint
}

Checkpointer::~Checkpointer(void)
{
}

// main loop. Receives messages
void Checkpointer::recv(void)
{
  Message m;

//todo
  /* Display the size of the different messages
   if (node_id() == 0)
   {
   Message *ms = new Message();
   Request *rs = new Request();
   Accept_req *as = new Accept_req();
   Learn *ls = new Learn();

   printf("Size of Message (%i) = %lu, addr%%64 = %lu\n", ms->tag(),
   ms->length(), (unsigned long) ms->content() % 64);
   printf("Size of Request (%i) = %lu, addr%%64 = %lu\n", rs->tag(),
   rs->length(), (unsigned long) rs->content() % 64);
   printf("Size of Accept_req (%i) = %lu, addr%%64 = %lu\n", as->tag(),
   as->length(), (unsigned long) as->content() % 64);
   printf("Size of Learn (%i) = %lu, addr%%64 = %lu\n", ls->tag(),
   ls->length(), (unsigned long) ls->content() % 64);

   delete ls;
   delete as;
   delete rs;
   delete ms;
   }

   return;
   */

  while (1)
  {
    size_t s = IPC_receive(m.content(), m.length());

#ifdef MSG_DEBUG
    printf(
        "Checkpointer %i has received a message of size %lu (%lu received) and tag %i\n",
        nid, (unsigned long) m.length(), (unsigned long) s, m.tag());
#endif

    switch (m.tag())
    {
    //todo
    case REQUEST:
      handle_request((Request*) &m);
      break;

    case ACCEPT_REQ:
      handle_accept_req((Accept_req*) &m);
      break;

    case LEARN:
      handle_learn((Learn*) &m);
      break;

    default:
      printf(
          "Checkpointer %i has received a non-valid message of size %lu (%lu received) and tag %i\n",
          nid, (unsigned long) m.length(), (unsigned long) s, m.tag());
      break;
    }
  }
}
