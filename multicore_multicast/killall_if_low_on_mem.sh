#!/bin/bash
# script to kill all instances (replicas + coordinator if more than 800MB is used by the system

while [ true ]; do

   freemem=$(free | grep 'Mem:' | awk '{print $3}')
   while [ $freemem -lt 800000 ]; do
      sleep 5s
      freemem=$(free | grep 'Mem:' | awk '{print $3}')
   done

   # kill remaining processes
   for TT in "inet" "unix" "lm" "ipc"; do
      killall ${TT}_server
      killall ${TT}_client
   done

   echo '6001 6004 10' > /proc/local_multicast
   echo -n '--- Killed all instances at '
   date
done

