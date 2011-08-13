#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-

import sys
import os
import math
from oset import oset
import scipy


################################################################################
# An experiment is caracterized by its data.
class Experiment:
   # Variables:
   #  -nb_summaries_to_take: number of summarizes to take into account for computing the results
   #  -all_nb_nodes: an ordered set of all the number of paxos nodes
   #  -all_msg_size: an ordered set of all the checkpoint response sizes
   #  -all_chkpt_size: an ordered set of all the checkpoint request sizes
   #  -all_nb_iter: an ordered set of all the number of iterations
   #  -all_channel_size: an ordered set of all the channel sizes
   #  -all_limit_thr: an ordered set of all the throughputs when there is a limitation
   #  -results: a dictionnary containing, for each pair (nb_nodes, msg_size,
   #    chkpt_size, nb_iter, channel_size, limit_thr), a list of tuples of the
   #    different values for each summary


   def __init__(self, nb_summaries):
      self.nb_summaries_to_take= nb_summaries
      self.all_nb_nodes = oset()
      self.all_msg_size = oset()
      self.all_chkpt_size = oset()
      self.all_nb_iter = oset()
      self.all_channel_size = oset()
      self.all_limit_thr = oset()
      self.results = {}


   def add_summary(self, filename):
      fd = open(filename, 'r')

      for line in fd:
         # do not read comments
         if line.startswith("#"):
            continue

         zeline = line.split('\t')
         if len(zeline) != 8:
            continue

         nb_nodes = int(zeline[0])
         msg_size = int(zeline[1])
         chkpt_size = int(zeline[2])
         nb_iter = int(zeline[3])
         channel_size = int(zeline[4])
         limit_thr = float(zeline[5])
         thr = float(zeline[6])
         lat = float(zeline[7])

         key = (nb_nodes, msg_size, chkpt_size, nb_iter, channel_size, limit_thr)
         t = self.results.get(key)
         if t == None:
            self.results[key] = ([thr], [lat])
         else:
            t[0].append(thr)
            t[1].append(lat)

         self.all_nb_nodes.add(nb_nodes)
         self.all_msg_size.add(msg_size)
         self.all_chkpt_size.add(chkpt_size)
         self.all_nb_iter.add(nb_iter)
         self.all_channel_size.add(channel_size)
         self.all_limit_thr.add(limit_thr)

      fd.close()

   def get_nb_summaries(self):
      print("The following experiments are to be redone because less than %d summaries:"%(self.nb_summaries_to_take))
      print("#nb_nodes\tmsg_size\tchkpt_size\tnb_iter\tchannel_size\tlimit_thr\tnb_summaries")
      
      for nb_nodes in self.all_nb_nodes:
         for msg_size in self.all_msg_size:
            for chkpt_size in self.all_chkpt_size:
               for nb_iter in self.all_nb_iter:
                  for channel_size in self.all_channel_size:
                     for limit_thr in self.all_limit_thr:

                        key = (nb_nodes, msg_size, chkpt_size, nb_iter, channel_size, limit_thr)
                        t = self.results.get(key)
                        if t == None:
                           continue

                        if len(t[0]) < self.nb_summaries_to_take:
                           print("\033[91m%d\t%d\033[0m\t%d\t%d\t%d\t%d\t\033[91m%d\033[0m"%(nb_nodes, msg_size, chkpt_size, nb_iter, channel_size, limit_thr, len(t[0])))
################################################################################


# ENTRY POINT
if __name__ == "__main__":
   if len(sys.argv) >= 2:
      E = Experiment(int(sys.argv[1]))
      for i in xrange(2, len(sys.argv)):
         E.add_summary(sys.argv[i])
      E.get_nb_summaries()
   else:
      print("Usage: %s <nb_summaries_to_take> <in_summary_file1> [ ... <in_summary_filen>]"%(sys.argv[0])) 
