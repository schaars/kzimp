#!/bin/bash
#
# Launch a Checkpointing XP with Barrelfish MP
# Args:
#   $1: nb nodes
#   $2: nb iter
#   $3: message max size
#   $4: number of messages in the channel


CONFIG_FILE=config


if [ $# -eq 4 ]; then
   NB_NODES=$1
   NB_ITER=$2
   MESSAGE_MAX_SIZE=$3
   MSG_CHANNEL=$4
 
else
   echo "Usage: ./$(basename $0) <nb_nodes> <nb_iter> <msg_max_size> <channel_size>"
   exit 0
fi

./stop_all.sh
rm -f /tmp/checkpointing_node_*_finished
./remove_shared_segment.pl

# create config file
./create_config.sh $NB_NODES $NB_ITER > $CONFIG_FILE

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
F=/tmp/checkpointing_node_0_finished
while [ ! -e $F ]; do
   echo "Waiting for the end"
   sleep 10
done

# save results
./stop_all.sh
./remove_shared_segment.pl
mv results.txt barrelfish_mp_${NB_NODES}nodes_${NB_ITER}iter_${MESSAGE_MAX_SIZE}B_${MSG_CHANNEL}channelSize.txt
