#!/bin/bash
#
# create the configuration file
# output on stdout
# Args:
#   $1: nb nodes
#   $2: nb iter 

if [ $(hostname) == "sci73" ] || [ $(hostname) == "sci74" ] || [ $(hostname) == "sci75" ] || [ $(hostname) == "sci76" ] || [ $(hostname) == "sci77" ]; then
  NB_THREADS_PER_CORE=2
else
  NB_THREADS_PER_CORE=1
fi

if [ $# -eq 2 ]; then
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
