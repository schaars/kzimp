#!/bin/bash

NB_PAXOS_NODES=5
NB_ITER=100000
#LEADER_ACCEPTOR_ARRAY=( same_proc different_proc )
LEADER_ACCEPTOR_ARRAY=( same_proc )
MSG_SIZE_ARRAY=( 64 128 512 1024 4096 10240 102400 1048576 )

# set it to profile if you want to enable the profiling
PROFILING=

for leader_acceptor in ${LEADER_ACCEPTOR_ARRAY[@]}; do
for msg_size in ${MSG_SIZE_ARRAY[@]}; do

# may be needed for the profiling, if it takes too much time
#if [ $msg_size -eq 102400 ] || [ $msg_size -eq 1048576 ]; then
#  NB_ITER=10000
#elif [ $msg_size -eq 10240 ]; then
#  NB_ITER=1000000
#else
#  NB_ITER=10000000
#fi  
     
  ./launch_ulm.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 50 $PROFILING
  ./launch_barrelfish_mp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 1000 $PROFILING
  ./launch_kzimp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 500 $PROFILING
  ./launch_bfish_mprotect.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 1000 $PROFILING
  ./launch_kbfish.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 1000 $PROFILING
  ./launch_inet_tcp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size $PROFILING
  ./launch_inet_udp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size $PROFILING
  ./launch_unix.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size $PROFILING
  ./launch_pipe.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size $PROFILING

done
done
