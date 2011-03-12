#!/bin/bash

NB_PAXOS_NODES=5
NB_ITER=10000000
LEADER_ACCEPTOR_ARRAY=( same_proc different_proc )
MSG_SIZE_ARRAY=( 64 128 512 1024 4096 10240 102400 1048576 )

for leader_acceptor in ${LEADER_ACCEPTOR_ARRAY[@]}; do
for msg_size in ${MSG_SIZE_ARRAY[@]}; do

if [ $msg_size -eq 102400 ] || [ $msg_size -eq 1048576 ]; then
  NB_ITER=10000
else
  NB_ITER=10000000
fi  
    
  ./launch_ulm.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor ${MSG_SIZE_ARRAY[$i]} 50
  ./launch_barrelfish_mp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor ${MSG_SIZE_ARRAY[$i]} 1000
  
done
done
