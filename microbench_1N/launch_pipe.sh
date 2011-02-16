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

c=$(ps -A | grep pipe_microbench | wc -l)
while [ $c -gt 0 ]; do
   sleep 20
   c=$(ps -A | grep pipe_microbench | wc -l)
done

./stop_all.sh

# save files
mkdir $OUTPUT_DIR
mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/

sudo chown bft:bft /tmp/perf.data.*

# for the consumers
str=""
for c in $(seq 1 ${NB_CONSUMERS}); do
   cid=$(( $c * $NB_THREADS_PER_CORE ))
   str="$str --c $cid"
done

for e in 0 1 2; do
   ./profiler/parser-sampling /tmp/perf.data.* --c 0 --base-event ${e} --app pipe_microbench > $OUTPUT_DIR/perf_producer_event_${e}.log
   ./profiler/parser-sampling /tmp/perf.data.* ${str} --base-event ${e} --app pipe_microbench > $OUTPUT_DIR/perf_consumers_event_${e}.log
done

rm /tmp/perf.data.* -f
