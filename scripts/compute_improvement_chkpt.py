#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-

import sys
import os
from oset import oset

################################################################################
# A Summary contains, for a given mechanism, a dictionnary key -> (mean, stdev)
class Summary:
   # Variables:
   #  -name: name of the communication mechanism to which this summary belongs to
   #  -filename: file from which to take the results
   #  -thr_or_lat: string which is either thr or lat depending on if the results are
   #               on the throughput or the latency, because the formulas to compute
   #               the improvement are not the same.
   #  -all_keys: an ordered set of all the keys
   #  -results: a dictionnary containing, for each msg_size, a tuple
   #            (mean, stdev)


   def __init__(self, thr_or_lat):
      self.name = ""
      self.filename = ""
      self.thr_or_lat = ""
      self.all_keys = oset()
      self.results = {}


   def __init__(self, name, filename, thr_or_lat = ""):
      self.name = name
      self.filename = filename
      self.thr_or_lat = thr_or_lat
      self.all_keys = oset()
      self.results = {}

      self.populate_results()


   def populate_results(self):
      fd = open(self.filename, 'r')
      
      for line in fd:
         # do not read comments
         if line.startswith("#"):
            continue

         zeline = line.split('\t')
         if len(zeline) != 3:
            continue

         k = int(zeline[0])
         mean = float(zeline[1])
         stdev = float(zeline[2])

         self.results[k] = (mean, stdev)

         self.all_keys.add(k)

      fd.close()

################################################################################


# given a main summary and a list of summary,
# compute the improvement of the main summary
# against each summary in the list
def compute_improvement(main_summary, summaries):
   fd = open("plot.data", 'w')

   fd.write("#key")
   for s in summaries:
      fd.write("\tmean_imp_%s/%s\tstdev_imp_%s/%s"%(main_summary.name, s.name, main_summary.name, s.name))
   fd.write("\n")
      
   for k in main_summary.all_keys:
      t = main_summary.results[k]
      mean_main = t[0]
      stdev_main = t[1]

      fd.write("%s\t"%(k))

      for s in summaries:
         try:
            t = s.results[k]
         except KeyError:
            continue

         mean_s = t[0]
         stdev_s = t[1]

         # We need to know if it is the throughput or the latency,
         # because the formula are not the same
         if main_summary.thr_or_lat == "thr":
            mean_i = (mean_main / mean_s - 1) * 100.0
         elif main_summary.thr_or_lat == "lat":
            mean_i = (1 - mean_main / mean_s) * 100.0
         else:
            print("%s is not valid. Aborting."%(main_summary.thr_or_lat))
            return
         stdev_i = 0

         fd.write("\t%f\t%f"%(mean_i, stdev_i))
      
      fd.write("\n")

   fd.close()

   return


# ENTRY POINT
if __name__ == "__main__":
   if len(sys.argv) >= 5:
      main_summary = Summary(sys.argv[2], sys.argv[3], sys.argv[1]) 
      summaries = []
      for i in xrange(4, len(sys.argv), 2):
         summaries.append(Summary(sys.argv[i], sys.argv[i+1])) 
      compute_improvement(main_summary, summaries)
   else:
      print("Usage: %s <thr or lat> <title_main_summary> <file_main_summary> <title_summaryA> <file_summaryA> [ ... <title_summaryN> <file_summaryN>]"%(sys.argv[0])) 
