#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-
#
# Make a summary of the experiments.
#  argv[1]: directory where to find the results
#  argv[2]: output file
#
# This script is mostly result filename agnostic.
# However, you cannot mix different communication mechanism: this is not taken into account


import sys
import os
import re



################################################################################
# Nested dictionnary
# Source: http://ohuiginn.net/mt/2010/07/nested_dictionaries_in_python.html
class NestedDict(dict):
   def __getitem__(self, key):
      if key in self: return self.get(key)
      return self.setdefault(key, NestedDict())
################################################################################


################################################################################
# A Summary contains the results (thr + lat) of the different experiments
class Summary:
   # Variables:
   #  -results: a (nested) dictionnary containing the results
   #  -indir: the directory where to find the results

   def __init__(self, indir):
      self.indir = indir
      self.results = NestedDict()

      self.construct()
   # ]-- end __init__/2


   # construct the summary
   def construct(self):
      # the regex
      regex = r"""^(.+)_(\d+)clients_(\d+)B"""
      regex_obj = re.compile(regex)

      # for every directory in the folder
      dir_list = [x for x in os.listdir(self.indir) if "clients_" in x]
      for d in dir_list:
         match_obj = regex_obj.search(d)
         comm_mech = match_obj.group(1)
         nb_clients = int(match_obj.group(2))
         msg_size = int(match_obj.group(3))
         
         with open(self.indir + '/' + d + '/manager.log') as myfile:
            last_line = list(myfile)[-1]
         elts = last_line.split('\t')
         
         mean_thr = float(elts[1])
         stdev_thr = float(elts[2])
         mean_lat = float(elts[3])
         stdev_lat = float(elts[4])

         self.add_tuple(comm_mech, nb_clients, msg_size, (mean_thr, stdev_thr, mean_lat, stdev_lat))
      # ]-- end for
   # ]-- end construct/1

   def add_tuple(self, comm_mech, nb_clients, msg_size, T):
      self.results[msg_size][nb_clients] = T
   # ]-- end add_tuple/9


   def dump(self, outfile):


      for msg_size in sorted(self.results):
         fd = open(outfile + str(msg_size) + "B.txt", 'w')
         fd.write("#msg_size=%iB\n"%(msg_size))
         fd.write("#nb_clients\tmean_thr\tstdev_thr\tmean_lat\tstdev_lat\n")
         
         sub1 = self.results[msg_size]
         for nb_clients in sorted(sub1):
            T = sub1[nb_clients]
            fd.write("%i\t%.5f\t%.5f\t%.5f\t%.5f\n"%(nb_clients, T[0], T[1], T[2], T[3]))

         fd.close()
   # ]-- end dump/2
################################################################################


# ENTRY POINT
if __name__ == "__main__":
   if len(sys.argv) == 3:
      Summary(sys.argv[1]).dump(sys.argv[2])
   else:
      print("Usage: %s <results_directory> <output_file>"%(sys.argv[0])) 

