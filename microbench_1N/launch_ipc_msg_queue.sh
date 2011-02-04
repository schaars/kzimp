#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: do you want 1 or N queues?
#  $5: message max size
#  $6: optional. Set it if you do not want the script to compile the program


MEMORY_DIR="memory_conso"

# get arguments
if [ $# -eq 6 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   NB_QUEUES=$4
   if [ $NB_QUEUES == "1" ]; then
     ONE_QUEUE=-DONE_QUEUE
   else
     ONE_QUEUE=
   fi
   MESSAGE_MAX_SIZE=$5
   NO_COMPILE=$6

elif [ $# -eq 5 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   NB_QUEUES=$4
   if [ $NB_QUEUES == "1" ]; then
     ONE_QUEUE=-DONE_QUEUE
   else
     ONE_QUEUE=
   fi
   MESSAGE_MAX_SIZE=$5
   NO_COMPILE=
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> <nb_queues> <message_max_size>"
   exit 0
fi


rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

# used by ftok
touch /tmp/ipc_msg_queue_microbench

# kenel parameters
sudo ./root_set_value.sh $MESSAGE_MAX_SIZE /proc/sys/kernel/msgmax
sudo ./root_set_value.sh 100000000 /proc/sys/kernel/msgmnb

# make with the new parameters
if [ ! -z $NO_COMPILE ]; then
   echo "$ONE_QUEUE -DMESSAGE_MAX_SIZE=$MESSAGE_MAX_SIZE" > IPC_MSG_QUEUE_PROPERTIES
   make ipc_msg_queue_microbench
fi

./get_memory_usage.sh  $MEMORY_DIR &
./bin/ipc_msg_queue_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh

# save files
OUTPUT_DIR="microbench_ipc_msg_queue_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_${NB_QUEUES}queues_msg_max_size_${MESSAGE_MAX_SIZE}B"
mkdir $OUTPUT_DIR
mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/
