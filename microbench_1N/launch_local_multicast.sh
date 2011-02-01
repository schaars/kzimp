#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: nb messages
#  $4: max nb messages in Local Multicast ring buffer


START_PORT=6001

# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   NB_MSG=$3
   NB_MSG_LM=$4

else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <nb_messages> <nb_msg_local_multicast>"
   exit 0
fi


./stop_all.sh

# activate Local Multicast
END_PORT=$(( $START_PORT + $NB_CONSUMERS - 1 ))
if [ -e /proc/local_multicast ]; then
   sudo ./root_set_value.sh "$START_PORT $END_PORT $NB_MSG_LM" /proc/local_multicast
else
   echo "You need a kernel compiled with Local Multicast. Aborting."
   exit 0
fi

./bin/local_multicast_microbench -r $NB_CONSUMERS -s $MSG_SIZE -n $NB_MSG

./stop_all.sh

# save files
OUTPUT_DIR="microbench_local_multicast_${NB_CONSUMERS}consumers_${NB_MSG}messages_${MSG_SIZE}B"
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/