/*
 * Checkpoint.h
 *
 * What is a checkpoint
 */

#ifndef CHECKPOINT_H_
#define CHECKPOINT_H_

#include <stdint.h>

#define CACHE_LINE_SIZE 64

#define CHECKPOINT_SIZE (sizeof(int) * 2 + sizeof(uint64_t))
struct checkpoint
{
  int len; // length of the message
  int src; // sender of the message
  uint64_t cn; // checkpoint number of the message

  char __p[MESSAGE_MAX_SIZE - CHECKPOINT_SIZE];
}__attribute__((__packed__, __aligned__(64)));

class Checkpoint
{
public:
  Checkpoint(void);
  Checkpoint(int src, uint64_t _cn);
  ~Checkpoint(void);

  // return a pointer to the message
  char* content(void) const;

  // return the length of this message
  size_t length(void) const;

  // return the sender of the message
  int sender(void) const;

  // return the checkpoint number of this message
  uint64_t checkpoint_number(void) const;

private:
  char *msg;

  // cast content to a struct checkpoint*
  struct checkpoint *rep(void) const;

  void init_message(int len, int src, uint64_t cn);
};

// return a pointer to the message
inline char* Checkpoint::content(void) const
{
  return msg;
}

// return the length of this message
inline size_t Checkpoint::length(void) const
{
  return rep()->len;
}

// return the sender of the message
inline int Checkpoint::sender(void) const
{
  return rep()->src;
}

// return the checkpoint number of this message
inline uint64_t Checkpoint::checkpoint_number(void) const
{
  return rep()->cn;
}

// cast content to a struct checkpoint*
inline struct checkpoint *Checkpoint::rep(void) const
{
  return (struct checkpoint *) msg;
}

#endif /* CHECKPOINT_H_ */
