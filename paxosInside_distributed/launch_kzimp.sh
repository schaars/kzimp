#!/bin/bash
#
# Launch a PaxosInside XP with kzimp
# Args:
#   $1: nb paxos nodes
#   $2: nb iter per client
#   $3: same_proc or different_proc
#   $4: message max size
#   $5: number of messages in the channel
#   $6: if given, then activate profiling


CONFIG_FILE=config
PROFDIR=../profiler

KZIMP_DIR="../kzimp/kzimp"
#KZIMP_DIR="../kzimp/kzimp_smallbuff"

# Do we compute the checksum?
COMPUTE_CHKSUM=0

# Writer's timeout
KZIMP_TIMEOUT=60000

# macro for the version with 1 channel per learner i -> client 0
ONE_CHANNEL_PER_LEARNER=


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
if [ -e $ONE_CHANNEL_PER_LEARNER ]; then
   NB_MAX_CHANNELS=6
else
   NB_MAX_CHANNELS=4
fi
cd $KZIMP_DIR
make
./kzimp.sh load nb_max_communication_channels=${NB_MAX_CHANNELS} default_channel_size=${MSG_CHANNEL} default_max_msg_size=${MESSAGE_MAX_SIZE} default_timeout_in_ms=${KZIMP_TIMEOUT} default_compute_checksum=${COMPUTE_CHKSUM}
cd -

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE}" > ULM_PROPERTIES
if [ -e $ONE_CHANNEL_PER_LEARNER ]; then
   echo "-DONE_CHANNEL_PER_LEARNER" >> ULM_PROPERTIES
fi
make kzimp_paxosInside


#####################################
############# Profiler  #############
if [ ! -z $PROFILER ]; then
cd $PROFDIR
make
cd ..
fi
#####################################


# launch
./bin/kzimp_paxosInside $CONFIG_FILE &


#####################################
############# Profiler  #############
if [ ! -z $PROFILER ]; then
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
if [ ! -z $PROFILER ]; then
sudo pkill profiler
sudo chown bft:bft /tmp/perf.data.*

OUTPUT_DIR=kzimp_profiling_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize
mkdir $OUTPUT_DIR

for e in 0 1 2; do
   $PROFDIR/parser-sampling /tmp/perf.data.* --c 0 --c 1 --c 2 --c 3 --c 4 --c 5 --c 6 --base-event ${e} --app kzimp_paxosInsi > $OUTPUT_DIR/perf_everyone_event_${e}.log
done

rm /tmp/perf.data.* -f
fi
#####################################


# save results
./stop_all.sh
cd $KZIMP_DIR; ./kzimp.sh unload; cd -
mv results.txt kzimp_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize.txt
