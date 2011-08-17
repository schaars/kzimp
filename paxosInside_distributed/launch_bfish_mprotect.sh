#!/bin/bash
#
# Launch a PaxosInside XP with bfish + mprotect (uses kbfishmem)
# Args:
#   $1: nb paxos nodes
#   $2: nb iter per client
#   $3: same_proc or different_proc
#   $4: message max size
#   $5: number of messages in the channel
#   $6: if given, then activate profiling

COMM_MECH="bfish_mprotect"

CONFIG_FILE=config
PROFDIR=../profiler

KBFISH_MEM_DIR="../kbfishmem"

# WAIT_TYPE is either USLEEP or BUSY
WAIT_TYPE=USLEEP

if [ $# -eq 6 ]; then
   NB_PAXOS_NODES=$1
   NB_ITER=$2
   LEADER_ACCEPTOR=$3
   MESSAGE_MAX_SIZE=$4
   MSG_CHANNEL=$5
   PROFILER=$6
   
elif [ $# -eq 5 ]; then
   NB_PAXOS_NODES=$1
   NB_ITER=$2
   LEADER_ACCEPTOR=$3
   MESSAGE_MAX_SIZE=$4
   MSG_CHANNEL=$5
   PROFILER=
 
else
   echo "Usage: ./$(basename $0) <nb_paxos_nodes> <nb_iter> <same_proc|different_proc> <msg_max_size> <channel_size> [profiling?]"
   exit 0
fi

if [ "$PROFILER" = "likwid" ] && [ -z $LIKWID_GROUP ]; then
   echo "You must give a LIKWID_GROUP when using liwkid."
   exit 0
else
   sudo modprobe msr
   sudo chmod o+rw /dev/cpu/*/msr
   export PATH=$PATH:/home/bft/multicore_replication_microbench/likwid/installed/bin
   export LD_PRELOAD=${LD_PRELOAD}:/home/bft/multicore_replication_microbench/likwid/installed/lib
   PROFILE_OUT="paxos_bfish_likwid_${LIKWID_GROUP}.txt"
fi

./stop_all.sh
./remove_shared_segment.pl
rm -f /tmp/paxosInside_client_*_finished

# create config file
./create_config.sh $NB_PAXOS_NODES 2 $NB_ITER $LEADER_ACCEPTOR > $CONFIG_FILE

# Min size is the size of a uintptr_t: 8 bytes
if [ $MESSAGE_MAX_SIZE -lt 8 ]; then
   REAL_MSG_SIZE=8
else
   REAL_MSG_SIZE=$MESSAGE_MAX_SIZE
fi

NB_MAX_CHANNELS=8

echo "-DNB_MESSAGES=${MSG_CHANNEL} -DMESSAGE_MAX_SIZE=${REAL_MSG_SIZE} -DMESSAGE_BYTES=${REAL_MSG_SIZE}" > BFISH_MPROTECT_PROPERTIES
make ${COMM_MECH}_paxosInside
REAL_MSG_SIZE=$(./bin/bfishmprotect_get_struct_ump_message_size)

#compile and load module
cd $KBFISH_MEM_DIR
make
./kbfishmem.sh unload
./kbfishmem.sh load nb_max_communication_channels=${NB_MAX_CHANNELS} default_channel_size=${MSG_CHANNEL} default_max_msg_size=${REAL_MSG_SIZE} 
if [ $? -eq 1 ]; then
   echo "An error has occured when loading kbfishmem. Aborting the experiment $OUTPUT_DIR"
   exit 0
fi
cd -


#####################################
############# Profiler  #############
if [ "$PROFILER" = "profile" ]; then
cd $PROFDIR
make
cd -
fi
#####################################


# launch
if [ "$PROFILER" = "likwid" ]; then
likwid-perfctr -C 0-6 -g ${LIKWID_GROUP} ./bin/${COMM_MECH}_paxosInside $CONFIG_FILE | tee ${PROFILE_OUT} &
else
./bin/${COMM_MECH}_paxosInside $CONFIG_FILE &
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
n=0
while [ $nbc -ne 1 ]; do
  #if [ $n -eq 100 ]; then
  #    echo -e "\n${COMM_MECH}_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize\n\tTAKES TOO MUCH TIME" >> results.txt
  #    break
  #fi

   echo "Waiting for the end: nbc=$nbc / 1"
   sleep 10

   nbc=0
   for i in $(seq 0 2); do
      F=/tmp/paxosInside_client_$(($i + $NB_PAXOS_NODES))_finished
      if [ -e $F ]; then
         nbc=$(($nbc+1))
      fi
   done

   n=$(($n+1))
done


#####################################
############# Profiler  #############
if [ "$PROFILER" = "profile" ]; then
sudo pkill profiler
sudo chown bft:bft /tmp/perf.data.*

OUTPUT_DIR=${COMM_MECH}_profiling_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize
mkdir $OUTPUT_DIR

for e in 0 1 2; do
   $PROFDIR/parser-sampling /tmp/perf.data.* -c 0 -c 1 -c 2 -c 3 -c 4 -c 5 -c 6 --base-event ${e} --app ${COMM_MECH}_ > $OUTPUT_DIR/perf_everyone_event_${e}.log
done

rm /tmp/perf.data.* -f
fi

if [ "$PROFILER" = "likwid" ]; then
   killall ${COMM_MECH}_paxosInside

   OUTPUT_DIR=${COMM_MECH}_likwid_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize
   mkdir -p $OUTPUT_DIR
   mv $PROFILE_OUT $OUTPUT_DIR/
fi
#####################################

# save results
./stop_all.sh
./remove_shared_segment.pl
sleep 1
cd $KBFISH_MEM_DIR; ./kbfishmem.sh unload; cd -
mv results.txt ${COMM_MECH}_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize.txt
