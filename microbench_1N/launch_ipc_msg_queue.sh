#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: nb messages
#  $4: do you want 1 or N queues?
#  $5: message max size


# get arguments
if [ $# -eq 6 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   NB_MSG=$3
   NB_QUEUES=$4
   if [ $NB_QUEUES -eq 1 ]; then
     ONE_QUEUE=-DONE_QUEUE
   else
     ONE_QUEUE=
   fi
   MESSAGE_MAX_SIZE=$5
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <nb_messages> <nb_queues> <message_max_size>"
   exit 0
fi


./stop_all.sh

# used by ftok
touch /tmp/ipc_msg_queue_microbench

# kenel parameters
sudo ./root_set_value.sh $MESSAGE_MAX_SIZE /proc/sys/kernel/msgmax
sudo ./root_set_value.sh 100000000 /proc/sys/kernel/msgmnb

# make with the new parameters
echo "$ONE_QUEUE -DMESSAGE_MAX_SIZE=$MESSAGE_MAX_SIZE" > IPC_MSG_QUEUE_PROPERTIES
make ipc_msg_queue_microbench

./bin/ipc_msg_queue_microbench -r $NB_CONSUMERS -s $MSG_SIZE -n $NB_MSG

./stop_all.sh

# save files
OUTPUT_DIR="microbench_ipc_msg_queue_${NB_CONSUMERS}consumers_${NB_MSG}messages_${MSG_SIZE}B_${NB_QUEUES}queues_msg_max_size_${MESSAGE_MAX_SIZE}B"
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/