/*
 * Acceptor.h
 *
 * Everything for the Acceptor
 */

#ifndef ACCEPTOR_H_
#define ACCEPTOR_H_

#include "Role.h"

class PaxosNode;

class Acceptor
{
public:
 Acceptor(PaxosNode* _paxosNode);
 ~Acceptor();

private:
  static const Role role = ACCEPTOR;
  PaxosNode *paxosNode;
};

#endif /* ACCEPTOR_H_ */
