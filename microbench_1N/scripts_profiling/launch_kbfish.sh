#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: max nb of messages in the circular buffer

#MEMORY_DIR="memory_conso"
PROFDIR=../profiler

KBFISH_DIR="../kbfish"


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

OUTPUT_DIR="microbench_kbfish_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_${MAX_NB_MSG}messages_in_buffer"

if [ -d $OUTPUT_DIR ]; then
   echo KZIMP ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B ${MAX_NB_MSG} msg in channel already done
   exit 0
fi

#rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

# Min size is the size of a uintptr_t: 8 bytes
if [ $MSG_SIZE -lt 8 ]; then
   REAL_MSG_SIZE=8
else
   REAL_MSG_SIZE=$MSG_SIZE
fi

#compile and load module
cd $KBFISH_DIR
echo "-DMESSAGE_BYTES=${REAL_MSG_SIZE}" > KBFISH_PROPERTIES
make
./kbfish.sh load nb_max_communication_channels=${NB_CONSUMERS} default_channel_size=${MAX_NB_MSG} default_max_msg_size=${REAL_MSG_SIZE} 
if [ $? -eq 1 ]; then
   echo "An error has occured when loading kbfishmem. Aborting the experiment $OUTPUT_DIR"
   exit 0
fi
cd -

# launch XP
#./get_memory_usage.sh  $MEMORY_DIR &
echo "" > KBFISH_PROPERTIES
make kbfish_microbench
./bin/kbfish_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP &

sleep 5

sudo $PROFDIR/profiler-sampling &

sleep $DURATION_XP
sudo pkill profiler

sleep 30

./stop_all.sh

# save files
mkdir $OUTPUT_DIR
#mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/

sudo chown bft:bft /tmp/perf.data.*

for e in 0 1 2 3; do
   $PROFDIR/parser-sampling /tmp/perf.data.* --base-event ${e} > $OUTPUT_DIR/perf_everyone_event_${e}.log
done

cd $KBFISH_DIR; ./kbfish.sh unload; cd -

rm /tmp/perf.data.* -f
