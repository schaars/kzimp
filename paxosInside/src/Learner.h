/*
 * Learner.h
 *
 * Everything for the Learner
 */

#ifndef LEARNER_H_
#define LEARNER_H_

#include "Role.h"

class PaxosNode;

class Learner
{
public:
 Learner(PaxosNode* _paxosNode);
 ~Learner();

private:
  static const Role role = LEARNER;
  PaxosNode *paxosNode;
};

#endif /* LEARNER_H_ */
