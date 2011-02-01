#!/bin/bash
#
# script to extract the latency of all the experiments.
# Launch multiple instances of extract_latency.sh in parallel


nb_cores=$(grep cores /proc/cpuinfo | wc -l)

# construct the list of directories for each core
core=0
rm -f dir_list_core*
for dir in *consumers*B*; do
   echo $dir >> dir_list_core$core
   core=$(( ($core+1) % $nb_cores ))   
done


# launch the cores
for core in $(seq 0 $nb_cores); do
   ./extract_latencies.pl dir_list_core$core &
done


# wait for the cores to finish
nbc=1
while [ $nbc -gt 0 ]; do
   nbc=$(pgrep extract_latency.pl | wc -l)
   sleep 10
done


rm -f dir_list_core*
