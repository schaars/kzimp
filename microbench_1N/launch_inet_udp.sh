#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: nb messages warmup phase
#  $4: nb messages logging phase
#  $5: optionnal. Set multicast if you want udp multicast


MEMORY_DIR="memory_conso"


# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   NB_MSG_WARMUP=$3
   NB_MSG_LOGGING=$4
   MULTICAST=

elif [ $# -eq 5 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   NB_MSG_WARMUP=$3
   NB_MSG_LOGGING=$4
   MULTICAST=multicast
   
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <nb_messages_warmup_phase> <nb_messages_logging_phase> [multicast]"
   exit 0
fi


./stop_all.sh
mkdir $MEMORY_DIR


# deactivate Local Multicast if present
if [ -e /proc/local_multicast ]; then
   sudo ./root_set_value.sh "0 0 0" /proc/local_multicast
fi

sudo sysctl -p inet_sysctl.conf


# memory for 10 seconds
./get_memory_usage.sh  $MEMORY_DIR &
sleep 10


# launch XP
if [ -z $MULTICAST ]; then
   ./bin/inet_udp_microbench  -n $NB_CONSUMERS -m $MSG_SIZE -w $NB_MSG_WARMUP -l $NB_MSG_LOGGING
else
   ./bin/inet_udp_multicast_microbench  -n $NB_CONSUMERS -m $MSG_SIZE -w $NB_MSG_WARMUP -l $NB_MSG_LOGGING
fi

# memory for 10 seconds
sleep 10
./stop_all.sh


# save files
if [ -z $MULTICAST ]; then
   OUTPUT_DIR="inet_udp_${NB_CONSUMERS}consumers_${MSG_SIZE}B"
else
   OUTPUT_DIR="inet_udp_multicast_${NB_CONSUMERS}consumers_${MSG_SIZE}B"
fi
mkdir $OUTPUT_DIR
mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/
mv latencies_*.log $OUTPUT_DIR/
