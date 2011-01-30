#!/usr/bin/env python3.1

# Note: developped for python v3.1. Should work with python v3.0, but not with python 2.x
#
# This script extract the latencies of each message
# Usage: ./extract_latencies.py <number_of_consumers>


import sys


nb_consumers = 1

# main hashtable:
#    message id -> (producer_time, consumer1_time, ..., consumerN_time)
HT = {}


# extract latencies from a producer
def extract_latencies_producer():
   fd = open("latencies_producer.log", 'r')

   for line in fd:
      zeline = line.split()

      message_id = int(zeline[0])
      time = int(zeline[1])

      content = HT.get(message_id)
      if content == None:
         HT[message_id] = [time]
      else:
         print("There is something that should not be there: [", message_id, "]=", content, sep="")

   fd.close()
######################################


# extract latencies from the consumer which has the id id
def extract_latencies_consumer(id):
   fd = open("latencies_consumer_" + str(id) + ".log", 'r')

   for line in fd:
      zeline = line.split()

      message_id = int(zeline[0])
      time = int(zeline[1])

      content = HT.get(message_id)
      if content == None:
         print("We should have something for", message_id)
      else:
         content.append(time)
         HT[message_id] = content

   fd.close()
######################################


# get the latency of each message. Save the latencies in messages_latencies.log
def get_per_message_latency():
   fd = open("messages_latencies.log", 'w')

   fd.write("#message_id\tlatency(usec)\n")
   for key in HT:
      content = HT.get(key)

      te = content.pop(0)
      tr = max(content)
      latency = tr - te

      fd.write(str(key) + "\t" + str(latency) + "\n")

   fd.close()
######################################


# Display a helpful message
def print_help():
   print("This script extract the latencies of each message")
   print("Usage: ./extract_latencies.py <number_of_consumers>")
######################################


# ENTRY POINT
if __name__ == "__main__":
   nb_args = len(sys.argv)
   if nb_args == 2:
      nb_consumers = int(sys.argv[1]) 
      extract_latencies_producer()

      for i in range(0, nb_consumers):
         extract_latencies_consumer(i+1)

      get_per_message_latency()
   else:
      print_help()
######################################
