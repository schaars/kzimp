#!/bin/bash
#
# Launch a PaxosInside XP with POSIX Message Queue
# Args:
#   $1: nb paxos nodes
#   $2: nb iter per client
#   $3: same_proc or different_proc
#   $4: message max size
#   $5: number of messages in the channel
#   $6: if given, then activate profiling


CONFIG_FILE=config
PROFDIR=../profiler


if [ $# -eq 6 ]; then
   NB_PAXOS_NODES=$1
   NB_ITER=$2
   LEADER_ACCEPTOR=$3
   MESSAGE_MAX_SIZE=$4
   MSG_CHANNEL=$5
   PROFILER=$5
   
elif [ $# -eq 5 ]; then
   NB_PAXOS_NODES=$1
   NB_ITER=$2
   LEADER_ACCEPTOR=$3
   MESSAGE_MAX_SIZE=$4
   MSG_CHANNEL=$5
   PROFILER=
 
else
   echo "Usage: ./$(basename $0) <nb_paxos_nodes> <nb_iter> <same_proc|different_proc> <msg_max_size> <nb_msg_channel> [profiling?]"
   exit 0
fi

sudo ./stop_all.sh
sudo rm -f /tmp/paxosInside_client_*_finished
if [ ! -d /dev/mqueue ]; then
   sudo mkdir /dev/mqueue
fi
sudo mount -t mqueue none /dev/mqueue
sudo rm /dev/mqueue/posix_message_queue_paxosInside*

#set new parameters
sudo ./root_set_value.sh 48 /proc/sys/fs/mqueue/queues_max
sudo ./root_set_value.sh $MSG_CHANNEL /proc/sys/fs/mqueue/msg_max

if [ $MESSAGE_MAX_SIZE -lt 128 ]; then
   sudo ./root_set_value.sh 128 /proc/sys/fs/mqueue/msgsize_max
else
   sudo ./root_set_value.sh $MESSAGE_MAX_SIZE /proc/sys/fs/mqueue/msgsize_max
fi

# create config file
./create_config.sh $NB_PAXOS_NODES 2 $NB_ITER $LEADER_ACCEPTOR > $CONFIG_FILE

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE}" > POSIX_MSG_QUEUE_PROPERTIES
make posix_msg_queue_paxosInside


#####################################
############# Profiler  #############
if [ ! -z $PROFILER ]; then
cd $PROFDIR
make
cd -
fi
#####################################


# launch
sudo ./bin/posix_msg_queue_paxosInside $CONFIG_FILE &


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

OUTPUT_DIR=posix_mq_profiling_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize
mkdir $OUTPUT_DIR

for e in 0 1 2; do
   $PROFDIR/parser-sampling /tmp/perf.data.* -c 0 -c 1 -c 2 -c 3 -c 4 -c 5 -c 6 --base-event ${e} --app posix_msg_queu > OUTPUT_DIR/perf_everyone_event_${e}.log
done

rm /tmp/perf.data.* -f
fi
#####################################


# save results
sudo ./stop_all.sh
sudo rm /dev/mqueue/posix_message_queue_paxosInside*
sudo umount /dev/mqueue
sudo rmdir /dev/mqueue

sudo chown bft:bft results.txt
mv results.txt posix_mq_${NB_PAXOS_NODES}nodes_2clients_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize.txt
