#!/bin/bash

NB_PAXOS_NODES=5
NB_ITER=1000000
#LEADER_ACCEPTOR_ARRAY=( same_proc different_proc )
LEADER_ACCEPTOR_ARRAY=( same_proc )
MSG_SIZE_ARRAY=( 64 128 512 1024 4096 10240 102400 1048576 )

# set it to profile if you want to enable the profiling using bprof
# set it to likwid if you want to profile with likwid. You also need to define
# LIKWID_GROUP to define what you'd like to profile.
PROFILING=

for leader_acceptor in ${LEADER_ACCEPTOR_ARRAY[@]}; do
for msg_size in ${MSG_SIZE_ARRAY[@]}; do

# may be needed for the profiling, if it takes too much time
#if [ $msg_size -eq 102400 ] || [ $msg_size -eq 1048576 ]; then
#  NB_ITER=100000
#elif [ $msg_size -eq 10240 ]; then
#  NB_ITER=1000000
#else
#  NB_ITER=1000000
#fi  
     
  ./launch_ulm.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 50 $PROFILING
  ./launch_barrelfish_mp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 1000 $PROFILING
  ./launch_kzimp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 500 $PROFILING
  ./launch_bfish_mprotect.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 1000 $PROFILING
  ./launch_kbfish.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 250 $PROFILING
  ./launch_inet_tcp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size $PROFILING
  ./launch_inet_udp.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size $PROFILING
  ./launch_unix.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size $PROFILING
  ./launch_pipe.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size $PROFILING
  ./launch_ipc_msg_queue.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size $PROFILING
  ./launch_posix_msg_queue.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 10 $PROFILING
  ./launch_openmpi.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 0 $PROFILING
  ./launch_openmpi.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 72 $PROFILING
  ./launch_mpich2.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 0 $PROFILING

  # likwid example:
  #LIKWID_GROUP=CACHE ./launch_bfish_mprotect.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 1000 $PROFILING
  #LIKWID_GROUP=L2CACHE ./launch_bfish_mprotect.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 1000 $PROFILING
  #LIKWID_GROUP=L3CACHE ./launch_bfish_mprotect.sh $NB_PAXOS_NODES $NB_ITER $leader_acceptor $msg_size 1000 $PROFILING


done
done
