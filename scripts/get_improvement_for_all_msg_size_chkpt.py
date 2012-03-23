#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-
#

import sys

################################################################################
# Nested dictionnary
# Source: http://ohuiginn.net/mt/2010/07/nested_dictionaries_in_python.html
class NestedDict(dict):
   def __getitem__(self, key):
      if key in self: return self.get(key)
      return self.setdefault(key, NestedDict())
################################################################################


D = NestedDict()

def f(filename):
   global D

   fd = open(filename, 'r')

   for line in fd:
      zeline = line.split()
      if len(zeline) != 3:
         continue

      nbNodes = int(zeline[0])
      msgSize = int(zeline[1])

      vmin = float(zeline[2])
      vmax = float(zeline[2])
      vminp = float(zeline[2])
      vmaxp = float(zeline[2])

      for v in zeline[3:]:
         v = float(v)
         if v < vmin:
            vmin = v
         if v > vmax:
            vmax = v

         if v > 0 and v < vminp:
            vminp = v
         if v > 0 and v > vmaxp:
            vmaxp = v

      D[msgSize][nbNodes] = (vmin, vmax, vminp, vmaxp)
      print (vmin, vmax, vminp, vmaxp)

   fd.close()

   for s in D:
      print "size =", s, "bytes"
      
      vmin = D[s][2][0]
      vmax = D[s][2][1]
      vminp = D[s][3][2]
      vmaxp = D[s][2][3]

      for n in D[s]:
         T = D[s][n]
         if T[0] < vmin:
            vmin = T[0]
         if T[1] > vmax:
            vmax = T[1]

         if T[2] > 0 and T[2] < vminp:
            vminp = T[2]
         if T[3] > 0 and T[3] > vmaxp:
            vmaxp = T[3]

      print vmin, vmax, vminp, vmaxp


# ENTRY POINT
if __name__ == "__main__":
   f(sys.argv[1])


