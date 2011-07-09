#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: max nb of messages in the circular buffer


KZIMP_DIR="/home/bft/zimp_module"
#KZIMP_DIR="/home/bft/zimp_module/kzimp_smallBuffer4smallMessages"

# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   MAX_NB_MSG=$4
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> <max_nb_messages_in_circular_buffer>"
   exit 0
fi

OUTPUT_DIR="microbench_kzimp_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_${MAX_NB_MSG}messages_in_buffer"

if [ -d $OUTPUT_DIR ]; then
   echo KZIMP ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B ${MAX_NB_MSG} msg in channel already done
   exit 0
fi

#rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

#compile and load module
cd $KZIMP_DIR; make; ./kzimp.sh load nb_max_communication_channels=1 default_channel_size=${MAX_NB_MSG} default_max_msg_size=${MSG_SIZE} default_timeout_in_ms=60000 default_compute_checksum=0; cd -

# launch XP
#./get_memory_usage.sh  $MEMORY_DIR &
make kzimp_microbench
timelimit -p -s 9 -t $((${DURATION_XP}+30)) ./bin/kzimp_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh
cd $KZIMP_DIR; ./kzimp.sh unload; cd -

# save files
mkdir $OUTPUT_DIR
#mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/
