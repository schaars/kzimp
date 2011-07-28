#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
from oset import oset


def check_summary_chkpt(filename, max_stdev):
   fd = open(filename, 'r')

   print("The following experiments are to be redone because stdev greater than %.2f%% or equal to 0:"%(max_stdev))
   print("nb_nodes\tchkpt_size\tthr\tstdev\tstdev_perc\tlat\tstdev\stdev_perc")

   for line in fd:
      # do not read comments
      if line.startswith("#"):
         continue

      zeline = line.split('\t')
      if len(zeline) != 12:
         continue

      nb_nodes = int(zeline[0])
      chkpt_size = int(zeline[1])
      snap_size = int(zeline[2])
      nb_iter = int(zeline[3])
      chan_size = int(zeline[4])
      limit_thr = float(zeline[5])
      thr_mean = float(zeline[6])
      thr_stdev = float(zeline[7])
      thr_stdev_perc = float(zeline[8])
      lat_mean = float(zeline[9])
      lat_stdev = float(zeline[10])
      lat_stdev_perc = float(zeline[11])

      if thr_stdev_perc > max_stdev or thr_stdev_perc == 0.0:
         print("\033[91m%i\t%i\033[0m\t%.2f\t%.2f\t\033[91m%.2f\033[0m\t%.2f\t%.2f\t%.2f"%(nb_nodes, chkpt_size, thr_mean, thr_stdev, thr_stdev_perc, lat_mean, lat_stdev, lat_stdev_perc))
      elif lat_stdev_perc > max_stdev or lat_stdev_perc == 0.0:
         print("\033[91m%i\t%i\033[0m\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t\033[91m%.2f\033[0m"%(nb_nodes, chkpt_size, thr_mean, thr_stdev, thr_stdev_perc, lat_mean, lat_stdev, lat_stdev_perc))

   fd.close()


def check_summary_paxosInside(filename, max_stdev):
   fd = open(filename, 'r')

   print("The following experiments are to be redone because stdev greater than %.2f%% or equal to 0:"%(max_stdev))
   print("req_size\tthr\tstdev\tstdev_perc")

   for line in fd:
      # do not read comments
      if line.startswith("#"):
         continue

      zeline = line.split('\t')
      if len(zeline) != 9:
         continue

      nb_paxos_nodes = int(zeline[0])
      nb_clients = int(zeline[1])
      req_size = int(zeline[2])
      nb_iter = int(zeline[3])
      leader_acceptor = zeline[4]
      chan_size = int(zeline[5])
      thr = float(zeline[6])
      stdev = float(zeline[7])
      stdev_perc = float(zeline[8])

      if stdev_perc > max_stdev or stdev_perc == 0.0:
         print("\033[91m%i\033[0m\t%.2f\t%.2f\t\033[91m%.2f\033[0m"%(req_size, thr, stdev, stdev_perc))

   fd.close()


# ENTRY POINT
if __name__ == "__main__":
   if len(sys.argv) == 4:
      if sys.argv[1] == "paxosInside":
         check_summary_paxosInside(sys.argv[2], float(sys.argv[3]))
      elif sys.argv[1] == "chkpt":
         check_summary_chkpt(sys.argv[2], float(sys.argv[3]))
      else:
         print("Unknown bench: %s"%(sys.argv[1]))
         print("Usage: %s <paxosInside or chkpt> <file_summary> <max_stdev>"%(sys.argv[0])) 
   else:
      print("Usage: %s <paxosInside or chkpt> <file_summary> <max_stdev>"%(sys.argv[0])) 
