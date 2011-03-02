#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: max nb datagrams


NB_THREADS_PER_CORE=2


# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   NB_DATAGRAMS=$4
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> <nb_datagrams>"
   exit 0
fi

OUTPUT_DIR="microbench_unix_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_${NB_DATAGRAMS}dgrams"

if [ -d $OUTPUT_DIR ]; then
   echo Unix sockets ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B already done
   exit 0
fi

./stop_all.sh

# recompile with message size
echo "-DMESSAGE_MAX_SIZE=$MSG_SIZE" > UNIX_SOCKETS_PROPERTIES
make unix_microbench

# modify the max number of datagrams
sudo ./root_set_value.sh $NB_DATAGRAMS /proc/sys/net/unix/max_dgram_qlen

./bin/unix_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh

# save files
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/
