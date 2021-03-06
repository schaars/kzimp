#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: max size of Local Multicast ring buffer


#MEMORY_DIR="memory_conso"
START_PORT=6001
PROFDIR=../profiler

if [ -z $YIELD_COUNT ]; then
   YIELD_COUNT=1
fi
if [ -z $MULTICAST_MASK ]; then
   MULTICAST_MASK="0xffffffff"
fi

# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   SIZE_BUFFER_LM=$4

else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> <local_multicast_buffer_size>"
   exit 0
fi

OUTPUT_DIR="microbench_local_multicast_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_${SIZE_BUFFER_LM}messages_in_buffer"

if [ -d $OUTPUT_DIR ]; then
   echo Local Multicast ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B already done
   exit 0
fi


#rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

# activate Local Multicast
END_PORT=$(( $START_PORT + $NB_CONSUMERS - 1 ))
if [ -e /proc/local_multicast ]; then
   sudo ./root_set_value.sh "$START_PORT $END_PORT $SIZE_BUFFER_LM $YIELD_COUNT $MULTICAST_MASK" /proc/local_multicast
else
   echo "You need a kernel compiled with Local Multicast. Aborting."
   exit 0
fi

#./get_memory_usage.sh  $MEMORY_DIR &
./bin/local_multicast_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP &

sleep 5

sudo $PROFDIR/profiler-sampling &

sleep $DURATION_XP
sudo pkill profiler

c=$(ps -A | grep local_multicast | wc -l)
iter=0
while [ $c -gt 0 ] && [ $iter -lt 10 ]; do
   sleep 20
   c=$(ps -A | grep local_multicast | wc -l)
   iter=$(($iter+1))
   echo iteration number $iter
done

./stop_all.sh

# save files
mkdir $OUTPUT_DIR
#mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/

sudo chown bft:bft /tmp/perf.data.*

for e in 0 1 2 3; do
   $PROFDIR/parser-sampling /tmp/perf.data.* --base-event ${e} > $OUTPUT_DIR/perf_everyone_event_${e}.log
done

rm /tmp/perf.data.* -f
