#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: nb messages
#  $4: optional. Set multicast if you want udp multicast


# get arguments
if [ $# -eq 3 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   NB_MSG=$3
   MULTICAST=

elif [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   NB_MSG=$3
   MULTICAST=multicast
   
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <nb_messages> [multicast]"
   exit 0
fi


./stop_all.sh

# deactivate Local Multicast if present
if [ -e /proc/local_multicast ]; then
   sudo ./root_set_value.sh "0 0 0" /proc/local_multicast
fi

sudo sysctl -p inet_sysctl.conf

# launch XP
if [ -z $MULTICAST ]; then
   ./bin/inet_udp_microbench -r $NB_CONSUMERS -s $MSG_SIZE -n $NB_MSG
else
   ./bin/inet_udp_multicast_microbench -r $NB_CONSUMERS -s $MSG_SIZE -n $NB_MSG
fi

./stop_all.sh

# save files
if [ -z $MULTICAST ]; then
   OUTPUT_DIR="microbench_inet_udp_${NB_CONSUMERS}consumers_${NB_MSG}messages_${MSG_SIZE}B"
else
   OUTPUT_DIR="microbench_inet_udp_multicast_${NB_CONSUMERS}consumers_${NB_MSG}messages_${MSG_SIZE}B"
fi

mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/