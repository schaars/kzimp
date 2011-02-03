#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: max duration of the experiment in seconds in the channel


MEMORY_DIR="memory_conso"

# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   DURATION_XP_CHANNEL=$4
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> <DURATION_XP_channel>"
   exit 0
fi


rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

# modify shared mem parameters
sudo ./root_set_value.sh 10000000000 /proc/sys/kernel/shmall
sudo ./root_set_value.sh 10000000000 /proc/sys/kernel/shmmax

# recompile with message size
echo "-DNB_MESSAGES=$DURATION_XP_CHANNEL -DURPC_MSG_WORDS=$(($MSG_SIZE/8))" > BARRELFISH_MESSAGE_PASSING_PROPERTIES
make barrelfish_message_passing

# launch XP
./get_memory_usage.sh  $MEMORY_DIR &
./bin/barrelfish_message_passing_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh

# save files
OUTPUT_DIR="microbench_barrelfish_message_passing_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B"
mkdir $OUTPUT_DIR
mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/