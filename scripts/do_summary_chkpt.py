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
      regex1 = r"""^(.+)_(\d+)nodes_(\d+)iter_chkpt(\d+)_msg(\d+)B_(\d+)channelSize.txt"""
      regex2 = r"""^(.+)_(\d+)nodes_(\d+)iter_chkpt(\d+)_msg(\d+)B_(\d+)snapPerSec.txt"""
      regex3 = r"""^(.+)_(\d+)nodes_(\d+)iter_chkpt(\d+)_msg(\d+)B.txt"""
      regex4 = r"""^(.+)_(\d+)nodes_(\d+)iter_chkpt(\d+)_msg(\d+)B_knem.txt"""
      regex1_obj = re.compile(regex1)
      regex2_obj = re.compile(regex2)
      regex3_obj = re.compile(regex3)
      regex4_obj = re.compile(regex4)

      # for every file in the folder
      file_list = [x for x in os.listdir(self.indir) if x.endswith('.txt')]
      for f in file_list:
         if "channelSize" in f:
            match_obj = regex1_obj.search(f)
            chan_size=int(match_obj.group(6))
            limit_thr=0
         else:
            if "snapPerSec" in f:
               match_obj = regex2_obj.search(f)
               limit_thr=int(match_obj.group(6))
            else:
               if "knem" in f:
                  match_obj = regex4_obj.search(f)
                  limit_thr=0
               else:
                  match_obj = regex3_obj.search(f)
                  limit_thr=0
            chan_size=0

         # parse
         comm_mech=match_obj.group(1)
         nb_nodes=int(match_obj.group(2))
         nb_iter=int(match_obj.group(3))
         chkpt_size=int(match_obj.group(4))
         msg_size=int(match_obj.group(5))

         # get throughput and compute latency
         with open(self.indir + '/' + f) as myfile:
            last_line = list(myfile)[-1]
         thr = float(last_line.split(' ')[3])
         lat = 1.0/thr * 1000.0 * 1000.0

         # add tuple
         self.add_tuple(comm_mech, nb_nodes, nb_iter, chkpt_size, msg_size, chan_size, limit_thr, (thr, lat))
      # ]-- end for
   # ]-- end construct/1

   def add_tuple(self, comm_mech, nb_nodes, nb_iter, chkpt_size, msg_size, chan_size, limit_thr, T):
      self.results[nb_nodes][msg_size][chkpt_size][nb_iter][chan_size][limit_thr] = T
   # ]-- end add_tuple/9


   def dump(self, outfile):
      fd = open(outfile, 'w')

      fd.write("#nb_nodes\tmsg_size\tchkpt_size\tnb_iter\tchannel_size\tlimit_thr\tthroughput\tlatency\n")

      for nb_nodes in sorted(self.results):
         sub1 = self.results[nb_nodes]
         for msg_size in sorted(sub1):
            sub2 = sub1[msg_size]
            for chkpt_size in sorted(sub2):
               sub3 = sub2[chkpt_size]
               for nb_iter in sorted(sub3):
                  sub4 = sub3[nb_iter]
                  for chan_size in sorted(sub4):
                     sub5 = sub4[chan_size]
                     for limit_thr in sorted(sub5):
                        T = sub5[limit_thr]
                        fd.write("%i\t%i\t%i\t%i\t%i\t%i\t%.5f\t%.5f\n"%(nb_nodes, msg_size, chkpt_size, nb_iter, chan_size, limit_thr, T[0], T[1]))

      fd.close()
   # ]-- end dump/2
################################################################################


# ENTRY POINT
if __name__ == "__main__":
   if len(sys.argv) == 3:
      Summary(sys.argv[1]).dump(sys.argv[2])
   else:
      print("Usage: %s <results_directory> <output_file>"%(sys.argv[0])) 

