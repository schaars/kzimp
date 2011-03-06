/*
 * MessageTag.h
 *
 * Enumeration of the possible message tags
 */

#ifndef MESSAGE_TAG_H_
#define MESSAGE_TAG_H_

// BARRELFISH_ACK: used internally by Barrelfish MP in order to decrease the number
// of messages in transit
enum MessageTag
{
  REQUEST, ACCEPT_REQ, LEARN, RESPONSE, BARRELFISH_ACK, UNKNOWN
};

#endif /* MESSAGE_TAG_H_ */
