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

#KZIMP_DIR="../kzimp/kzimp_allMessagesArea"
#KZIMP_DIR="../kzimp/kzimp_splice"
KZIMP_DIR="../kzimp/kzimp_reader_splice"

# Do we compute the checksum?
COMPUTE_CHKSUM=2

# Writer's timeout
KZIMP_TIMEOUT=60000

# macro for the version with 1 channel per learner i -> client 0
# set it to 1 if you want 1 channel per learner, 0 otherwise.
ONE_CHANNEL_PER_LEARNER=0


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

if [ "$PROFILER" = "likwid" ]; then
   if [ -z $LIKWID_GROUP ]; then
      echo "You must give a LIKWID_GROUP when using liwkid."
      exit 0
   else
      sudo modprobe msr
      sudo chmod o+rw /dev/cpu/*/msr
      export PATH=$PATH:/home/bft/multicore_replication_microbench/likwid/installed/bin
      export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/home/bft/multicore_replication_microbench/likwid/installed/lib
      PROFILE_OUT="paxos_kzimp_likwid_${LIKWID_GROUP}.txt"
   fi
fi


./stop_all.sh
rm -f /tmp/paxosInside_client_*_finished

# create config file
./create_config.sh $NB_PAXOS_NODES 2 $NB_ITER $LEADER_ACCEPTOR > $CONFIG_FILE

# compile and load module
if [ $ONE_CHANNEL_PER_LEARNER -eq 1 ]; then
   NB_MAX_CHANNELS=6
else
   NB_MAX_CHANNELS=4
fi
cd $KZIMP_DIR
make
./kzimp.sh unload
./kzimp.sh load nb_max_communication_channels=${NB_MAX_CHANNELS} default_channel_size=${MSG_CHANNEL} default_max_msg_size=${MESSAGE_MAX_SIZE} default_timeout_in_ms=${KZIMP_TIMEOUT} default_compute_checksum=${COMPUTE_CHKSUM}
if [ $? -eq 1 ]; then
   echo "An error has occured when loading kzimp. Aborting the experiment"
   exit 0
fi

cd -

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE}" > KZIMP_PROPERTIES
if [ $ONE_CHANNEL_PER_LEARNER -eq 1 ]; then
   echo "-DONE_CHANNEL_PER_LEARNER" >> KZIMP_PROPERTIES
fi
if [ $KZIMP_DIR = "../kzimp/kzimp_splice" ]; then
   echo "-DKZIMP_SPLICE -DCHANNEL_SIZE=$((${MSG_CHANNEL}+1))" >> KZIMP_PROPERTIES
fi
if [ $KZIMP_DIR = "../kzimp/kzimp_reader_splice" ]; then
   echo "-DKZIMP_READ_SPLICE -DCHANNEL_SIZE=${MSG_CHANNEL}" >> KZIMP_PROPERTIES
fi
make kzimp_paxosInside

#####################################
############# Profiler  #############
if [ "$PROFILER" = "profile" ]; then
cd $PROFDIR
make
cd -
fi
#####################################


# launch
#./bin/kzimp_paxosInside $CONFIG_FILE &

# launch
if [ "$PROFILER" = "likwid" ]; then
likwid-perfctr -C 0-6 -g ${LIKWID_GROUP} ./bin/kzimp_paxosInside $CONFIG_FILE | tee ${PROFILE_OUT} &
else
./bin/kzimp_paxosInside $CONFIG_FILE &
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
  #    echo -e "\nkzimp_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize\n\tTAKES TOO MUCH TIME" >> results.txt
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

OUTPUT_DIR=kzimp_profiling_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize
mkdir $OUTPUT_DIR

for e in 0 1 2; do
   $PROFDIR/parser-sampling /tmp/perf.data.* -c 0 -c 1 -c 2 -c 3 -c 4 -c 5 -c 6 --base-event ${e} --app kzimp_paxosInsi > $OUTPUT_DIR/perf_everyone_event_${e}.log
done

rm /tmp/perf.data.* -f
fi

if [ "$PROFILER" = "likwid" ]; then
   killall kzimp_paxosInside

   OUTPUT_DIR=kzimp_likwid_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize
   mkdir -p $OUTPUT_DIR
   mv $PROFILE_OUT $OUTPUT_DIR/
fi
#####################################


# save results
./stop_all.sh
sleep 1  # needed otherwise there is still a kzimp process and the module cannot be unloaded
cd $KZIMP_DIR; ./kzimp.sh unload; cd -
mv results.txt kzimp_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize.txt
