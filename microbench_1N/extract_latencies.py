#!/usr/bin/env python3.1

# Note: developped for python v3.1. Should work with python v3.0, but not with python 2.x
#
# This script extract the latencies of each message
# Usage: ./extract_latencies.py <directory> <number_of_consumers> <nb lines>


import os
import sys


# extract the latencies by reading in parallel the different files
def extract_latencies_parallel(nb_consumers, nb_lines):
   fd_producer = open("latencies_producer.log", 'r')

   fd_consumers = []
   for c in range(1, nb_consumers+1):
      fd_consumers.append(open("latencies_consumer_" + str(c) + ".log", 'r'))

   fd_latencies = open("messages_latencies.log", 'w')


   for line_id in range(0, nb_lines):
      line_of_producer = fd_producer.readline().split()

      latency_of_consumers = []
      lat_stop = 0
      for c in range(0, nb_consumers):
         lat_stop_cur = int(fd_consumers[c].readline().split()[1])
         if lat_stop_cur > lat_stop:
            lat_stop = lat_stop_cur

      message_id = int(line_of_producer[0])
      lat_start = int(line_of_producer[1])

      latency = lat_stop - lat_start

      fd_latencies.write(str(message_id) + "\t" + str(latency) + "\n")

   fd_latencies.close()
   fd_producer.close()
   for c in range(0, nb_consumers):
      fd_consumers[c].close()
######################################


# Display a helpful message
def print_help():
   print("This script extract the latencies of each message")
   print("Usage: ./extract_latencies.py <directory> <number_of_consumers> <nb lines>")
######################################


# ENTRY POINT
if __name__ == "__main__":
   nb_args = len(sys.argv)
   if nb_args == 4:
      directory = sys.argv[1]
      nb_consumers = int(sys.argv[2])
      nb_lines = int(sys.argv[3])

      os.chdir(directory)
      extract_latencies_parallel(nb_consumers, nb_lines)

   else:
      print_help()
######################################
