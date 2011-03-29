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


#Order of the cores for the Bertha:
# proc 0: cores 0 4 8 12
# proc 1: cores 3 7 11 15
# proc 2: cores 2 6 10 14
# proc 3: cores 1 5 9 13


core=0
for node in $(seq 0 $(( $NB_NODES - 1 ))); do
   echo $core
   core=$(( $core + $NB_THREADS_PER_CORE ))
done
