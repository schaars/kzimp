#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
import math
from oset import oset
import scipy


################################################################################
# An experiment is caracterized by its data.
class Experiment:
   # Variables::
   #TODO
   #  -all_nb_paxos_nodes: an ordered set of all the number of paxos nodes
   #  -all_nb_clients: an ordered set of all the number of clients
   #  -all_req_size: an ordered set of all the req sizes
   #  -all_nb_iter: an ordered set of all the number of iterations
   #  -all_leader_acceptor = a set of leader_acceptor strings
   #  -all_channel_size: an ordered set of all the channel sizes
   #  -results: a dictionnary containing, for each pair (nb_paxos_nodes, nb_clients,
   #    req_size, nb_iter, leader_acceptor, channel_size), a list of tuples of the
   #    different values for each summary

   def __init__(self):
   #TODO
      self.all_nb_paxos_nodes = oset()
      self.all_nb_clients = oset()
      self.all_req_size = oset()
      self.all_nb_iter = oset()
      self.all_leader_acceptor = set()
      self.all_channel_size = oset()
      self.results = {}


   def add_summary(self, filename):
      fd = open(filename, 'r')

      for line in fd:
         # do not read comments
         if line.startswith("#"):
            continue

         zeline = line.split('\t')
   #TODO
         if len(zeline) != 7:
            continue

   #TODO
         nb_paxos_nodes = int(zeline[0])
         nb_clients = int(zeline[1])
         req_size = int(zeline[2])
         nb_iter = int(zeline[3])
         leader_acceptor = zeline[4]
         channel_size = int(zeline[5])
         thr = float(zeline[6])

   #TODO
         key = (nb_paxos_nodes, nb_clients, req_size, nb_iter, leader_acceptor, channel_size)
         t = self.results.get(key)
         if t == None:
            self.results[key] = [thr]
         else:
            t.append(thr)

   #TODO
         self.all_nb_paxos_nodes.add(nb_paxos_nodes)
         self.all_nb_clients.add(nb_clients)
         self.all_req_size.add(req_size)
         self.all_nb_iter.add(nb_iter)
         self.all_leader_acceptor.add(leader_acceptor)
         self.all_channel_size.add(channel_size)

      fd.close()


   def get_mean_stdev_summary(self, filename):
      fd = open(filename, 'w')

   #TODO
      fd.write("#nb_paxos_nodes\tnb_clients\treq_size\tnb_iter\tleader_acceptor\tchannel_size\tthr_mean\tthr_stddev\tthr_stddev_perc\n")
      
   #TODO
      for nb_paxos_nodes in self.all_nb_paxos_nodes:
         for nb_clients in self.all_nb_clients:
            for req_size in self.all_req_size:
               for nb_iter in self.all_nb_iter:
                  for leader_acceptor in self.all_leader_acceptor:
                     for channel_size in self.all_channel_size:

                        key = (nb_paxos_nodes, nb_clients, req_size, nb_iter, leader_acceptor, channel_size)
                        t = self.results.get(key)
                        if t == None:
                           continue

                        fd.write("%d\t%d\t%d\t%d\t%s\t%d"%(nb_paxos_nodes, nb_clients, req_size, nb_iter, leader_acceptor, channel_size))
                        a = scipy.array(t)
                        m = a.mean()
                        s = a.std()
                        p = s * 100 / m
                        fd.write("\t%.5f\t%.5f\t%.5f\n"%(m, s, p))
   
      fd.close()
################################################################################


# ENTRY POINT
if __name__ == "__main__":
   if len(sys.argv) >= 2:
      E = Experiment()
      for i in xrange(2, len(sys.argv)):
         E.add_summary(sys.argv[i])
      E.get_mean_stdev_summary(sys.argv[1])
   else:
      print("Usage: %s <out_summary_file> <in_summary_file1> [ ... <in_summary_filen>]"%(sys.argv[0])) 
