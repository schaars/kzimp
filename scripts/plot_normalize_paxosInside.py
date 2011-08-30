#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-

import sys
import os
from oset import oset

################################################################################
# A Summary contains, for a given mechanism, a dictionnary msg_size -> (mean_thr, stdev_thr)
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
# compute the normalized throughput of each mechanism in the list of
# summaries with respect to the main_summary
def compute_normalized_thr(main_summary, summaries):
   fd = open("plot.data", 'w')

   fd.write("#msg_size")
   for s in summaries:
      fd.write("\tthr_normalized_%s/%s\tstdev_normalized_%s/%s"%(main_summary.name, s.name, main_summary.name, s.name))
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

         thr_i = thr_s / thr_main
         stdev_i = 0

         fd.write("\t%f\t%f"%(thr_i, stdev_i))
      
      fd.write("\n")

   fd.close()

   return


# ENTRY POINT
if __name__ == "__main__":
   if len(sys.argv) >= 4:
      summaries = []
      summaries.append(Summary(sys.argv[1], sys.argv[2]))
      for i in xrange(3, len(sys.argv), 2):
         summaries.append(Summary(sys.argv[i], sys.argv[i+1]))
      compute_normalized_thr(summaries[0], summaries)
   else:
      print("Usage: %s <title_main_summary> <file_main_summary> <title_summaryA> <file_summaryA> [ ... <title_summaryN> <file_summaryN>]"%(sys.argv[0])) 
