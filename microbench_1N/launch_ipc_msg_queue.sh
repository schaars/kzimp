#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: nb messages warmup phase
#  $4: nb messages logging phase
#  $5: do you want 1 or N queues?
#  $6: message max size


MEMORY_DIR="memory_conso"


# get arguments
if [ $# -eq 6 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   NB_MSG_WARMUP=$3
   NB_MSG_LOGGING=$4
   NB_QUEUES=$5
   if [ $NB_QUEUES -eq 1 ]; then
     ONE_QUEUE=-DONE_QUEUE
   else
     ONE_QUEUE=
   fi
   MESSAGE_MAX_SIZE=$6
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <nb_messages_warmup_phase> <nb_messages_logging_phase> <nb_queues> <message_max_size>"
   exit 0
fi


./stop_all.sh
mkdir $MEMORY_DIR


# used by ftok
touch /tmp/ipc_msg_queue_microbench

# make with the new parameters
echo "$ONE_QUEUE -DMESSAGE_MAX_SIZE=$MESSAGE_MAX_SIZE" > IPC_MSG_QUEUE_PROPERTIES
make ipc_msg_queue_microbench


# memory for 10 seconds
./get_memory_usage.sh  $MEMORY_DIR &
sleep 10


# launch XP
./bin/ipc_msg_queue_microbench  -n $NB_CONSUMERS -m $MSG_SIZE -w $NB_MSG_WARMUP -l $NB_MSG_LOGGING


# memory for 10 seconds
sleep 10
./stop_all.sh


# pre-processing: extract latency of each message
./extract_latencies.py $NB_CONSUMERS
rm -f latencies_*.log


# save files
OUTPUT_DIR="ipc_msg_queue_${NB_CONSUMERS}consumers_${MSG_SIZE}B_${NB_QUEUES}queues_msg_max_size_${MESSAGE_MAX_SIZE}B"
mkdir $OUTPUT_DIR
mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/
mv messages_latencies.log $OUTPUT_DIR/
