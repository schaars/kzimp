#!/bin/bash

NB_PAXOS_NODES=5
NB_ITER=100000
LEADER_ACCEPTOR_ARRAY=( same_proc different_proc )
MSG_SIZE_ARRAY=( 64 128 512 1024 4096 10240 102400 1048576 )

for leader_acceptor in ${LEADER_ACCEPTOR_ARRAY[@]}; do
for msg_size in ${MSG_SIZE_ARRAY[@]}; do
    
  ./launch_ulm.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 50
  ./launch_barrelfish_mp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 1000
  
done
done
