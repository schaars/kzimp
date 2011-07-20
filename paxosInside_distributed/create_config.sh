#!/bin/bash
#
# create the configuration file
# output on stdout
# Args:
#   $1: nb paxos nodes
#   $2: nb clients
#   $3: nb iter per client
#   $4: same_proc or different_proc

# In case of you have activated the hyperthreading
NB_THREADS_PER_CORE=1

if [ $# -eq 4 ]; then
   NB_PAXOS_NODES=$1
   NB_CLIENTS=$2
   NB_ITER_PER_CLIENT=$3
   LEADER_ACCEPTOR=$4
else
   echo "Usage: ./$(basename $0) <nb_paxos_nodes> <nb_clients> <nb_iter_per_client> <same_proc|different_proc>"
   exit 0
fi

echo $NB_PAXOS_NODES
echo $NB_CLIENTS
echo $NB_ITER_PER_CLIENT


#Order of the cores for the Bertha:
# proc 0: cores 0 4 8 12
# proc 1: cores 3 7 11 15
# proc 2: cores 2 6 10 14
# proc 3: cores 1 5 9 13


if [ $LEADER_ACCEPTOR == "same_proc" ]; then
   leader_core=0
   acceptor_core=$(( 1 * $NB_THREADS_PER_CORE ))
else
   leader_core=$(( 4 * $NB_THREADS_PER_CORE ))
   acceptor_core=0
fi


echo $leader_core
echo $acceptor_core

core=0
for node in $(seq 2 $(( $NB_PAXOS_NODES + $NB_CLIENTS - 1 ))); do
   while [ true ]; do
      if [ $core -eq $leader_core ] || [ $core -eq $acceptor_core ]; then
         core=$(( $core + $NB_THREADS_PER_CORE ))
      else
         break
      fi
   done

   echo $core
   core=$(( $core + $NB_THREADS_PER_CORE ))
done
