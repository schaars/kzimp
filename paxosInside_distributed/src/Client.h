/*
 * Client.h
 *
 * A client for PaxosInside
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdint.h>

#define LOG_FILE "results.txt"

class Response;

class Client
{
public:
  Client(int client_id, uint64_t nbi);
  ~Client(void);

  // return the id of this client
  int client_id(void) const;

  // do the client job
  void run(void);

private:
  bool handle_response(Response* r);

  uint64_t next_value(void);

  int cid; // the client id
  uint64_t nb_iter;
  uint64_t value;
  uint64_t expected_value;
};

inline int Client::client_id(void) const
{
  return cid;
}

inline uint64_t Client::next_value(void)
{
  return ++value;
}

#endif /* CLIENT_H_ */
