#!/bin/bash
#
# Launch a PaxosInside XP with kbfish
# Args:
#   $1: nb paxos nodes
#   $2: nb iter per client
#   $3: same_proc or different_proc
#   $4: message max size
#   $5: number of messages in the channel
#   $6: if given, then activate profiling


CONFIG_FILE=config
PROFDIR=../profiler

KBFISH_DIR="../kbfish"

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

./stop_all.sh
rm -f /tmp/paxosInside_client_*_finished

# create config file
./create_config.sh $NB_PAXOS_NODES 2 $NB_ITER $LEADER_ACCEPTOR > $CONFIG_FILE

# compile and load module
NB_MAX_CHANNELS=8
cd $KBFISH_DIR
echo "-DMESSAGE_BYTES=${MSG_SIZE}" > KBFISH_PROPERTIES
make
./kbfish.sh unload
./kbfish.sh load nb_max_communication_channels=${NB_MAX_CHANNELS} default_channel_size=${MAX_NB_MSG} default_max_msg_size=${MSG_SIZE} 
if [ $? -eq 1 ]; then
   echo "An error has occured when loading kbfishmem. Aborting the experiment $OUTPUT_DIR"
   exit 0
fi
cd -

# compile
echo "-DMESSAGE_MAX_SIZE=${MSG_SIZE}" > KBFISH_PROPERTIES
make kbfish_paxosInside


#####################################
############# Profiler  #############
if [ ! -z $PROFILER ]; then
cd $PROFDIR
make
cd ..
fi
#####################################


# launch
./bin/kbfish_paxosInside $CONFIG_FILE &


#####################################
############# Profiler  #############
if [ ! -z $PROFILER ]; then
sleep 5
sudo $PROFDIR/profiler-sampling &
fi
#####################################


# wait for the end
nbc=0
n=0
while [ $nbc -ne 1 ]; do
  #if [ $n -eq 100 ]; then
  #    echo -e "\nkbfish_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize\n\tTAKES TOO MUCH TIME" >> results.txt
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
if [ ! -z $PROFILER ]; then
sudo pkill profiler
sudo chown bft:bft /tmp/perf.data.*

OUTPUT_DIR=kbfish_profiling_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize
mkdir $OUTPUT_DIR

for e in 0 1 2; do
   $PROFDIR/parser-sampling /tmp/perf.data.* --c 0 --c 1 --c 2 --c 3 --c 4 --c 5 --c 6 --base-event ${e} --app kbfish_mprotect > $OUTPUT_DIR/perf_everyone_event_${e}.log
done

rm /tmp/perf.data.* -f
fi
#####################################


# save results
./stop_all.sh
sleep 1
cd $KBFISH_DIR; ./kbfish.sh unload; cd -
mv results.txt kbfish_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize.txt
