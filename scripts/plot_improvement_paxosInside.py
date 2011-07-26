#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
from oset import oset

################################################################################
# A summary ontains, for a given mechanism, a dictionnary msg_size -> (mean_thr, stdev_thr)
class Summary:
   # Variables:
   #  -name: name of the communication mechanism to which this summary belongs to
   #  -filename: file from which to take the results
   #  -all_msg_size: an ordered set of all the checkpoint response sizes
   #  -results: a dictionnary containing, for each msg_size, a tuple
   #            (mean_thr, stdev_thr)



   def __init__(self):
      self.name = ""
      self.filename = ""
      self.all_msg_size = oset()
      self.results = {}


   def __init__(self, name, filename):
      self.name = name
      self.filename = filename
      self.all_msg_size = oset()
      self.results = {}

      self.populate_results()


   def populate_results(self):
      fd = open(self.filename, 'r')
      
      for line in fd:
         # do not read comments
         if line.startswith("#"):
            continue

         zeline = line.split('\t')
         if len(zeline) != 9:
            continue

         msg_size = int(zeline[2])
         thr = float(zeline[6])
         stdev = float(zeline[7])

         self.results[msg_size] = (thr, stdev)

         self.all_msg_size.add(msg_size)

      fd.close()

################################################################################


# given a main summary and a list of summary,
# compute the improvement of the main summary
# against each summary in the list
def compute_improvement(main_summary, summaries):
   fd = open("plot.data", 'w')

   fd.write("#msg_size")
   for s in summaries:
      fd.write("\tthr_imp_%s/%s\tstdev_imp_%s/%s"%(main_summary.name, s.name, main_summary.name, s.name))
   fd.write("\n")
      
   for msg_size in main_summary.all_msg_size:
      t = main_summary.results[msg_size]
      thr_main = t[0]
      stdev_main = t[1]

      fd.write("%s\t"%(msg_size))

      for s in summaries:
         t = s.results[msg_size]
         thr_s = t[0]
         stdev_s = t[1]


         # Is this code the right thing to do?
         ## compute the min improvement
         ## thr_main - stdev_main against thr_s + stdev_s
         #m = thr_main - stdev_main
         #o = thr_s + stdev_s
         #imp_min = m * 100.0 / o - 100.0 
         #
         ## compute the max improvement
         ## thr_main + stdev_main against thr_s - stdev_s
         #m = thr_main + stdev_main
         #o = thr_s - stdev_s
         #imp_max = m * 100.0 / o - 100.0 
         #
         ## compute the mean and the stdev
         #thr_i = (imp_max + imp_min) / 2.0
         #stdev_i = imp_max - thr_i


         thr_i = thr_main * 100.0 / thr_s - 100.0
         stdev_i = 0

         fd.write("\t%f\t%f"%(thr_i, stdev_i))
      
      fd.write("\n")

   fd.close()

   return


# ENTRY POINT
if __name__ == "__main__":
   if len(sys.argv) >= 4:
      main_summary = Summary(sys.argv[1], sys.argv[2]) 
      summaries = []
      for i in xrange(3, len(sys.argv), 2):
         summaries.append(Summary(sys.argv[i], sys.argv[i+1])) 
      compute_improvement(main_summary, summaries)
   else:
      print("Usage: %s <title_main_summary> <file_main_summary> <title_summaryA> <file_summaryA> [ ... <title_summaryN> <file_summaryN>]"%(sys.argv[0])) 
