#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: nb messages warmup phase
#  $4: nb messages logging phase


MEMORY_DIR="memory_conso"


# get arguments
if [ $# -eq 5 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   NB_MSG_WARMUP=$3
   NB_MSG_LOGGING=$4
   MESSAGE_MAX_SIZE=$5
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <nb_messages_warmup_phase> <nb_messages_logging_phase>"
   exit 0
fi


./stop_all.sh
mkdir $MEMORY_DIR


#set new parameters
sudo ./root_set_value.sh 1000000 /proc/sys/fs/mqueue/msg_max
sudo ./root_set_value.sh $MSG_SIZE /proc/sys/fs/mqueue/msgsize_max


# memory for 10 seconds
./get_memory_usage.sh  $MEMORY_DIR &
sleep 10


# launch XP
./bin/posix_msg_queue_microbench  -n $NB_CONSUMERS -m $MSG_SIZE -w $NB_MSG_WARMUP -l $NB_MSG_LOGGING


# memory for 10 seconds
sleep 10
./stop_all.sh


# save files
OUTPUT_DIR="posix_msg_queue_${NB_CONSUMERS}consumers_${MSG_SIZE}B"
mkdir $OUTPUT_DIR
mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/
mv latencies_*.log $OUTPUT_DIR/
