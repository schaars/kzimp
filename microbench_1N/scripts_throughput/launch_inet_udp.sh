#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: optional. Set multicast if you want udp multicast


NB_THREADS_PER_CORE=2


# get arguments
if [ $# -eq 3 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   MULTICAST=

elif [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   MULTICAST=multicast
   
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> [multicast]"
   exit 0
fi


if [ -z $MULTICAST ]; then
   OUTPUT_DIR="microbench_inet_udp_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B"
else
   OUTPUT_DIR="microbench_inet_udp_multicast_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B"
fi

if [ -d $OUTPUT_DIR ]; then
   echo Inet UDP ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B, multicast=$MULTICAST already done
   exit 0
fi


./stop_all.sh

# recompile with message size
if [ -z $MULTICAST ]; then
echo "-DMESSAGE_MAX_SIZE=$MSG_SIZE" > INET_UDP_PROPERTIES
else
echo "-DMESSAGE_MAX_SIZE=$MSG_SIZE -DIP_MULTICAST" > INET_UDP_PROPERTIES
fi
make inet_udp_microbench

# deactivate Local Multicast if present
if [ -e /proc/local_multicast ]; then
   sudo ./root_set_value.sh "0 0 0" /proc/local_multicast
fi

sudo sysctl -p inet_sysctl.conf

# launch XP
./bin/inet_udp_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh

# save files
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/
