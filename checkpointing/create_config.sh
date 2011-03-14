#!/bin/bash
#
# create the configuration file
# output on stdout
# Args:
#   $1: nb nodes
#   $2: nb iter 

NB_THREADS_PER_CORE=2

if [ $# -eq 4 ]; then
   NB_NODES=$1
   NB_ITER=$2
else
   echo "Usage: ./$(basename $0) <nb_nodes> <nb_iter>"
   exit 0
fi

echo $NB_NODES
echo $NB_ITER

core=0
for node in $(seq 0 $(( $NB_NODES - 1 ))); do
   echo $core
   core=$(( $core + $NB_THREADS_PER_CORE ))
done
