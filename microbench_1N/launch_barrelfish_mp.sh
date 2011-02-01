#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: nb messages
#  $4: max nb messages in the channel


# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   NB_MSG=$3
   NB_MSG_CHANNEL=$4
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <nb_messages> <nb_msg_channel>"
   exit 0
fi


./stop_all.sh

# modify shared mem parameters
sudo ./root_set_value.sh 10000000000 /proc/sys/kernel/shmall
sudo ./root_set_value.sh 10000000000 /proc/sys/kernel/shmmax

# recompile with message size
echo "-DNB_MESSAGES=$NB_MSG_CHANNEL -DURPC_MSG_WORDS=$(($MSG_SIZE/8))" > BARRELFISH_MESSAGE_PASSING_PROPERTIES
make barrelfish_message_passing

# launch XP
./bin/barrelfish_message_passing_microbench -r $NB_CONSUMERS -s $MSG_SIZE -n $NB_MSG

./stop_all.sh

# save files
OUTPUT_DIR="microbench_barrelfish_message_passing_${NB_CONSUMERS}consumers_${NB_MSG}messages_${MSG_SIZE}B"
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/