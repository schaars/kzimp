#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: max size of Local Multicast ring buffer


MEMORY_DIR="memory_conso"
NB_THREADS_PER_CORE=2
START_PORT=6001

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


rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

# activate Local Multicast
END_PORT=$(( $START_PORT + $NB_CONSUMERS - 1 ))
if [ -e /proc/local_multicast ]; then
   sudo ./root_set_value.sh "$START_PORT $END_PORT $SIZE_BUFFER_LM" /proc/local_multicast
else
   echo "You need a kernel compiled with Local Multicast. Aborting."
   exit 0
fi

./get_memory_usage.sh  $MEMORY_DIR &
./bin/local_multicast_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP &

sleep 5

sudo ./profiler/profiler-sampling &

sleep $DURATION_XP
sudo pkill profiler

c=$(ps -A | grep local_multicast | wc -l)
while [ $c -gt 0 ]; do
   sleep 20
   c=$(ps -A | grep local_multicast | wc -l)
done
sleep 15

./stop_all.sh

# save files
mkdir $OUTPUT_DIR
mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/

sudo chown bft:bft /tmp/perf.data.*
for c in $(seq 0 ${NB_CONSUMERS}); do
   cid=$(( $c * $NB_THREADS_PER_CORE ))
   for e in 0 1 2; do
      ./profiler/parser-sampling /tmp/perf.data.* --c ${cid} --base-event ${e} --app local_multicast > $OUTPUT_DIR/perf_core_${cid}_event_${e}.log
   done
done
rm /tmp/perf.data.* -f
