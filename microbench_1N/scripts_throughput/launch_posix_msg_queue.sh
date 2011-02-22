#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: number of messages in the channel


NB_THREADS_PER_CORE=2


# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   MSG_CHANNEL=$4
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> <nb_msg_channel>"
   exit 0
fi

OUTPUT_DIR="microbench_posix_msg_queue_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_${MSG_CHANNEL}msg_in_channel"

if [ -d $OUTPUT_DIR ]; then
   echo POSIX msg queue ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B ${MSG_CHANNEL} msg in channel already done
   exit 0
fi

./stop_all.sh

#set new parameters
sudo ./root_set_value.sh 32 /proc/sys/fs/mqueue/queues_max
sudo ./root_set_value.sh $MSG_CHANNEL /proc/sys/fs/mqueue/msg_max

if [ $MSG_SIZE -lt 128 ]; then
   sudo ./root_set_value.sh 128 /proc/sys/fs/mqueue/msgsize_max
else
   sudo ./root_set_value.sh $MSG_SIZE /proc/sys/fs/mqueue/msgsize_max
fi

sudo ./bin/posix_msg_queue_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh

# save files
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/
