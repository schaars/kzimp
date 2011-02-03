#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds


MEMORY_DIR="memory_conso"

# get arguments
if [ $# -eq 3 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec>"
   exit 0
fi


rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

#set new parameters
sudo ./root_set_value.sh 32 /proc/sys/fs/mqueue/queues_max
sudo ./root_set_value.sh 10 /proc/sys/fs/mqueue/msg_max

if [ $MSG_SIZE -lt 128 ]; then
   sudo ./root_set_value.sh 128 /proc/sys/fs/mqueue/msgsize_max
else
   sudo ./root_set_value.sh $MSG_SIZE /proc/sys/fs/mqueue/msgsize_max
fi

./get_memory_usage.sh  $MEMORY_DIR &
./bin/posix_msg_queue_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh

# save files
OUTPUT_DIR="microbench_posix_msg_queue_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B"
mkdir $OUTPUT_DIR
mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/
