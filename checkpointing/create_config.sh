#!/bin/bash
#
# create the configuration file
# output on stdout
# Args:
#   $1: nb nodes
#   $2: nb iter 
#   $3: checkpoint period (ms)
#   $4: snapshot period (ms)

NB_THREADS_PER_CORE=2

if [ $# -eq 4 ]; then
   NB_NODES=$1
   NB_ITER=$2
   CHECKPOINT_PERIOD=$3
   SNAPSHOT_PERIOD=$4
else
   echo "Usage: ./$(basename $0) <nb_nodes> <nb_iter> <checkpoint_period(ms)> <snapshot_period(ms)>"
   exit 0
fi

echo $NB_NODES
echo $NB_ITER
echo $CHECKPOINT_PERIOD
echo $SNAPSHOT_PERIOD

core=0
for node in $(seq 0 $(( $NB_NODES - 1 ))); do
   echo $core
   core=$(( $core + $NB_THREADS_PER_CORE ))
done
