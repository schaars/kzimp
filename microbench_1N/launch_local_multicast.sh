#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: max size of Local Multicast ring buffer


MEMORY_DIR="memory_conso"
START_PORT=6001

# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   SIZE_BUFFER_LM=$4

else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> <local_multicast_buffer_size>"
   exit 0
fi


rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

# activate Local Multicast
END_PORT=$(( $START_PORT + $NB_CONSUMERS - 1 ))
if [ -e /proc/local_multicast ]; then
   sudo ./root_set_value.sh "$START_PORT $END_PORT $SIZE_BUFFER_LM" /proc/local_multicast
else
   echo "You need a kernel compiled with Local Multicast. Aborting."
   exit 0
fi

./get_memory_usage.sh  $MEMORY_DIR &
./bin/local_multicast_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh

# save files
OUTPUT_DIR="microbench_local_multicast_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_{SIZE_BUFFER_LM}bufferSize"
mkdir $OUTPUT_DIR
mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/