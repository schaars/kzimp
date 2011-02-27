#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: max size of Local Multicast ring buffer


NB_THREADS_PER_CORE=2
START_PORT=6001
if [ -z $YIELD_COUNT ]; then
   YIELD_COUNT=1
fi
if [ -z $MULTICAST_MASK ]; then
   MULTICAST_MASK="0xffffffff"
fi

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

OUTPUT_DIR="microbench_local_multicast_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_${SIZE_BUFFER_LM}messages_in_buffer"

if [ -d $OUTPUT_DIR ]; then
   echo Local Multicast ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B already done
   exit 0
fi


./stop_all.sh

# activate Local Multicast
END_PORT=$(( $START_PORT + $NB_CONSUMERS - 1 ))
if [ -e /proc/local_multicast ]; then
   sudo ./root_set_value.sh "$START_PORT $END_PORT $SIZE_BUFFER_LM $YIELD_COUNT $MULTICAST_MASK" /proc/local_multicast
else
   echo "You need a kernel compiled with Local Multicast. Aborting."
   exit 0
fi

./bin/local_multicast_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh

# save files
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/
