/*
 * Proposer.h
 *
 * Everything for the Proposer
 */

#ifndef PROPOSER_H_
#define PROPOSER_H_

#include "Role.h"

class PaxosNode;

class Proposer
{
public:
 Proposer(PaxosNode* _paxosNode);
 ~Proposer();

private:
  static const Role role = PROPOSER;
  PaxosNode *paxosNode;
};

#endif /* PROPOSER_H_ */
