#!/bin/bash
#
# Launch a PaxosInside XP with IPC Message Queue
# Args:
#   $1: nb paxos nodes
#   $2: nb iter per client
#   $3: same_proc or different_proc
#   $4: message max size
#   $5: if given, then activate profiling


CONFIG_FILE=config
PROFDIR=../profiler


if [ $# -eq 5 ]; then
   NB_PAXOS_NODES=$1
   NB_ITER=$2
   LEADER_ACCEPTOR=$3
   MESSAGE_MAX_SIZE=$4
   PROFILER=$5
   
elif [ $# -eq 4 ]; then
   NB_PAXOS_NODES=$1
   NB_ITER=$2
   LEADER_ACCEPTOR=$3
   MESSAGE_MAX_SIZE=$4
   PROFILER=
 
else
   echo "Usage: ./$(basename $0) <nb_paxos_nodes> <nb_iter> <same_proc|different_proc> <msg_max_size> [profiling?]"
   exit 0
fi


if [ "$PROFILER" = "likwid" ]; then
   if [ -z $LIKWID_GROUP ]; then
      echo "You must give a LIKWID_GROUP when using liwkid."
      exit 0
   else
      sudo modprobe msr
      sudo chmod o+rw /dev/cpu/*/msr
      export PATH=$PATH:/home/bft/multicore_replication_microbench/likwid/installed/bin
      export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/home/bft/multicore_replication_microbench/likwid/installed/lib
      PROFILE_OUT="paxos_ipc_mq_likwid_${LIKWID_GROUP}.txt"
   fi
fi

./stop_all.sh
rm -f /tmp/paxosInside_client_*_finished
./remove_shared_segment.pl

# used by ftok
touch /tmp/ipc_msg_queue_paxosInside

# kenel parameters
sudo ./root_set_value.sh $MESSAGE_MAX_SIZE /proc/sys/kernel/msgmax
sudo ./root_set_value.sh 100000000 /proc/sys/kernel/msgmnb

# create config file
./create_config.sh $NB_PAXOS_NODES 2 $NB_ITER $LEADER_ACCEPTOR > $CONFIG_FILE

# compile
echo "-DIPC_MSG_QUEUE -DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE}" > IPC_MSG_QUEUE_PROPERTIES
make ipc_msg_queue_paxosInside


#####################################
############# Profiler  #############
if [ "$PROFILER" = "profile" ]; then
cd $PROFDIR
make
cd -
fi
#####################################


# launch
#echo ./bin/ipc_msg_queue_paxosInside $CONFIG_FILE &
#read

# launch
if [ "$PROFILER" = "likwid" ]; then
likwid-perfctr -C 0-6 -g ${LIKWID_GROUP} ./bin/ipc_msg_queue_paxosInside $CONFIG_FILE | tee ${PROFILE_OUT} &
else
./bin/ipc_msg_queue_paxosInside $CONFIG_FILE &
fi


#####################################
############# Profiler  #############
if [ "$PROFILER" = "profile" ]; then
sleep 5
sudo $PROFDIR/profiler-sampling &
fi
#####################################


# wait for the end
nbc=0
while [ $nbc -ne 1 ]; do
   echo "Waiting for the end: nbc=$nbc / 1"
   sleep 10

   nbc=0
   for i in $(seq 0 2); do
      F=/tmp/paxosInside_client_$(($i + $NB_PAXOS_NODES))_finished
      if [ -e $F ]; then
         nbc=$(($nbc+1))
      fi
   done
done


#####################################
############# Profiler  #############
if [ "$PROFILER" = "profile" ]; then
sudo pkill profiler
sudo chown bft:bft /tmp/perf.data.*

OUTPUT_DIR=ipc_mq_profiling_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize
mkdir $OUTPUT_DIR

for e in 0 1 2; do
   $PROFDIR/parser-sampling /tmp/perf.data.* -c 0 -c 1 -c 2 -c 3 -c 4 -c 5 -c 6 --base-event ${e} --app ipc_msg_queue_p > $OUTPUT_DIR/perf_everyone_event_${e}.log
done

rm /tmp/perf.data.* -f
fi

if [ "$PROFILER" = "likwid" ]; then
   killall ipc_msg_queue_paxosInside

   OUTPUT_DIR=ipc_msg_queue_likwid_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize
   mkdir -p $OUTPUT_DIR
   mv $PROFILE_OUT $OUTPUT_DIR/
fi
#####################################


# save results
./stop_all.sh
./remove_shared_segment.pl
mv results.txt ipc_mq_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}.txt
