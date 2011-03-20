#!/bin/bash
#
# Launch a PaxosInside XP with ULM
# Args:
#   $1: nb paxos nodes
#   $2: nb iter per client
#   $3: same_proc or different_proc
#   $4: message max size
#   $5: number of messages in the channel
#   $6: if given, then activate profiling


CONFIG_FILE=config


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

./stop_all.sh
rm -f /tmp/paxosInside_client_*_finished
./remove_shared_segment.pl

# create config file
./create_config.sh $NB_PAXOS_NODES 2 $NB_ITER $LEADER_ACCEPTOR > $CONFIG_FILE

# ULM specific
# used by ftok
touch /tmp/ulm_paxosInside_client_to_leader
touch /tmp/ulm_paxosInside_leader_to_acceptor
touch /tmp/ulm_paxosInside_acceptor_multicast
for i in $(seq 0 $(( $NB_PAXOS_NODES-2 ))); do
  touch /tmp/ulm_paxosInside_learner${i}_to_client
done


#set new parameters
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmall
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmmax

# compile
echo "-DULM -DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DNB_MESSAGES=${MSG_CHANNEL}" > ULM_PROPERTIES
#echo "-DUSLEEP -DULM -DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DNB_MESSAGES=${MSG_CHANNEL}" > ULM_PROPERTIES
make ulm_paxosInside


#####################################
############# Profiler  #############
if [ ! -z $PROFILER ]; then
cd profiler
make
cd ..
fi
#####################################


# launch
./bin/ulm_paxosInside $CONFIG_FILE &


#####################################
############# Profiler  #############
if [ ! -z $PROFILER ]; then
sleep 5
sudo ./profiler/profiler-sampling &
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
if [ ! -z $PROFILER ]; then
sudo pkill profiler
sudo chown bft:bft /tmp/perf.data.*

#TODO: save results
OUTPUT_DIR=ulm_profiling_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize
mkdir $OUTPUT_DIR

for e in 0 1 2; do
   ./profiler/parser-sampling /tmp/perf.data.* --c 0 --c 1 --c 2 --c 3 --c 4 --c 5 --c 6 --base-event ${e} --app ulm_paxosInside > $OUTPUT_DIR/perf_everyone_event_${e}.log
done

rm /tmp/perf.data.* -f
fi
#####################################


# save results
./stop_all.sh
./remove_shared_segment.pl
mv results.txt ulm_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize.txt
