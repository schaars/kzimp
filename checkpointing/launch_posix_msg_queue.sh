#!/bin/bash
#
# Launch a Checkpointing XP with POSIX Message Queue
# Args:
#   $1: nb nodes
#   $2: nb iter
#   $3: message max size
#   $4: checkpoint size
#   $5: number of messages in the channel


CONFIG_FILE=config


if [ $# -eq 5 ]; then
   NB_NODES=$1
   NB_ITER=$2
   MESSAGE_MAX_SIZE=$3
   CHKPT_SIZE=$4
   MSG_CHANNEL=$5
 
else
   echo "Usage: ./$(basename $0) <nb_nodes> <nb_iter> <msg_max_size> <chkpt_size> <nb_msg_channel>"
   exit 0
fi

sudo ./stop_all.sh
sudo rm -f /tmp/checkpointing_node_0_finished
if [ ! -d /dev/mqueue ]; then
   sudo mkdir /dev/mqueue
fi
sudo mount -t mqueue none /dev/mqueue
sudo rm /dev/mqueue/posix_message_queue_checkpointing*

#set new parameters
sudo ./root_set_value.sh 48 /proc/sys/fs/mqueue/queues_max
sudo ./root_set_value.sh $MSG_CHANNEL /proc/sys/fs/mqueue/msg_max

if [ $MESSAGE_MAX_SIZE -lt 128 ]; then
   sudo ./root_set_value.sh 128 /proc/sys/fs/mqueue/msgsize_max
else
   sudo ./root_set_value.sh $MESSAGE_MAX_SIZE /proc/sys/fs/mqueue/msgsize_max
fi

# create config file
./create_config.sh $NB_NODES $NB_ITER > $CONFIG_FILE

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DMESSAGE_MAX_SIZE_CHKPT_REQ=${CHKPT_SIZE}" > POSIX_MSG_QUEUE_PROPERTIES
make posix_msg_queue_checkpointing

# launch
sudo ./bin/posix_msg_queue_checkpointing $CONFIG_FILE &

# wait for the end
F=/tmp/checkpointing_node_0_finished
n=0
while [ ! -e $F ]; do
   #if [ $n -eq 360 ]; then
   #   echo "TAKING TOO MUCH TIME: 3600 seconds" >> results.txt
   #   ./stop_all.sh
   #   exit 1
   #fi

   echo "Waiting for the end"
   sleep 10
   n=$(($n+1))
done

# save results
sudo ./stop_all.sh
sudo umount /dev/mqueue
sudo rmdir /dev/mqueue
sudo rm -f /tmp/multicore_replication_checkpointing*

sudo chown bft:bft results.txt
mv results.txt posix_msg_queue_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B.txt
