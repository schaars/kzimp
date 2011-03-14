#!/bin/bash
#
# Launch a Checkpointing XP with Barrelfish MP
# Args:
#   $1: nb nodes
#   $2: nb iter
#   $3: checkpoint period (ms)
#   $4: snapshot period (ms)
#   $5: message max size
#   $6: number of messages in the channel


CONFIG_FILE=config


if [ $# -eq 6 ]; then
   NB_NODES=$1
   NB_ITER=$2
   CHECKPOINT_PERIOD=$3
   SNAPSHOT_PERIOD=$4
   MESSAGE_MAX_SIZE=$5
   MSG_CHANNEL=$6
 
else
   echo "Usage: ./$(basename $0) <nb_nodes> <nb_iter> <checkpoint_period(ms)> <snapshot_period(ms)> <msg_max_size> <channel_size>"
   exit 0
fi

./stop_all.sh
rm -f /tmp/checkpointing_node_*_finished
./remove_shared_segment.pl

# create config file
./create_config.sh $NB_NODES $NB_ITER $CHECKPOINT_PERIOD $SNAPSHOT_PERIOD > $CONFIG_FILE

# Barrelfish specific
# used by ftok
touch /tmp/checkpointing_barrelfish_mp_shmem

#set new parameters
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmall
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmmax

# compile
#echo "-DUSLEEP -DNB_MESSAGES=${MSG_CHANNEL} -DURPC_MSG_WORDS=$(( ${MESSAGE_MAX_SIZE}/8 ))" > BARRELFISH_MP_PROPERTIES
echo "-DNB_MESSAGES=${MSG_CHANNEL} -DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DURPC_MSG_WORDS=$(( ${MESSAGE_MAX_SIZE}/8 ))" > BARRELFISH_MP_PROPERTIES
make barrelfish_mp_checkpointing

# launch
./bin/barrelfish_mp_checkpointing $CONFIG_FILE &

# wait for the end
nbc=0
while [ $nbc -ne $NB_NODES ]; do
   echo "Waiting for the end: nbc=$nbc / $NB_NODES"
   sleep 10

   nbc=0
   for i in $(seq 0 $NB_NODES); do
      F=/tmp/checkpointing_node_${i}_finished
      if [ -e $F ]; then
         nbc=$(($nbc+1))
      fi
   done
done

# save results
./stop_all.sh
./remove_shared_segment.pl
mv results.txt barrelfish_mp_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHECKPOINT_PERIOD}ms_snap${SNAPSHOT_PERIOD}ms_${MESSAGE_MAX_SIZE}B_${MSG_CHANNEL}channelSize.txt
