#!/bin/bash

NB_PAXOS_NODES=5
NB_ITER=10000000
#LEADER_ACCEPTOR_ARRAY=( same_proc different_proc )
LEADER_ACCEPTOR_ARRAY=( same_proc )
#MSG_SIZE_ARRAY=( 64 128 512 1024 4096 10240 102400 1048576 )
MSG_SIZE_ARRAY=( 64 10240 1048576 )

for leader_acceptor in ${LEADER_ACCEPTOR_ARRAY[@]}; do
for msg_size in ${MSG_SIZE_ARRAY[@]}; do

if [ $msg_size -eq 102400 ] || [ $msg_size -eq 1048576 ]; then
  NB_ITER=10000
elif [ $msg_size -eq 10240 ]; then
  NB_ITER=1000000
else
  NB_ITER=10000000
fi  
    
  ./launch_ulm.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 50 profile
  ./launch_barrelfish_mp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 500 profile
  ./launch_kzimp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 10000 profile
  
done
done
