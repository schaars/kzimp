#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds


MEMORY_DIR="memory_conso"
NB_THREADS_PER_CORE=2


# get arguments
if [ $# -eq 3 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec>"
   exit 0
fi


OUTPUT_DIR="microbench_pipe_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B"

if [ -d $OUTPUT_DIR ]; then
   echo Pipes ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B already done
   exit 0
fi

rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

./get_memory_usage.sh  $MEMORY_DIR &
./bin/pipe_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP &

sleep 5

sudo ./profiler/profiler-sampling &

sleep $DURATION_XP
sudo pkill profiler

./stop_all.sh

# save files
mkdir $OUTPUT_DIR
mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/

sudo chown bft:bft /tmp/perf.data.*
for c in $(seq 0 ${NB_CONSUMERS}); do
   cid=$(( $c * $NB_THREADS_PER_CORE ))
   for e in 0 1 2; do
      ./profiler/parser-sampling /tmp/perf.data.${cid} --c ${cid} --base-event ${e} --app pipe_microbench > $OUTPUT_DIR/perf_core_${cid}_event_${e}.log
   done
done
rm /tmp/perf.data.* -f
