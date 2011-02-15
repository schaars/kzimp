#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: do you want 1 or N queues?
#  $5: message max size
#  $6: optional. Set it if you do not want the script to compile the program


MEMORY_DIR="memory_conso"
NB_THREADS_PER_CORE=2


# get arguments
if [ $# -eq 6 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   NB_QUEUES=$4
   if [ $NB_QUEUES == "1" ]; then
     ONE_QUEUE=-DONE_QUEUE
   else
     ONE_QUEUE=
   fi
   MESSAGE_MAX_SIZE=$5
   NO_COMPILE=$6

elif [ $# -eq 5 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   NB_QUEUES=$4
   if [ $NB_QUEUES == "1" ]; then
     ONE_QUEUE=-DONE_QUEUE
   else
     ONE_QUEUE=
   fi
   MESSAGE_MAX_SIZE=$5
   NO_COMPILE=
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> <nb_queues> <message_max_size>"
   exit 0
fi

OUTPUT_DIR="microbench_ipc_msg_queue_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_${NB_QUEUES}queues_msg_max_size_${MESSAGE_MAX_SIZE}B"

if [ -d $OUTPUT_DIR ]; then
   echo IPC msg queue ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B ${NB_QUEUES} queues ${MESSAGE_MAX_SIZE}B for msg max size already done
   exit 0
fi


rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

# used by ftok
touch /tmp/ipc_msg_queue_microbench

# kenel parameters
sudo ./root_set_value.sh $MESSAGE_MAX_SIZE /proc/sys/kernel/msgmax
sudo ./root_set_value.sh 100000000 /proc/sys/kernel/msgmnb

# make with the new parameters
if [ -z $NO_COMPILE ]; then
   echo "$ONE_QUEUE -DMESSAGE_MAX_SIZE=$MESSAGE_MAX_SIZE" > IPC_MSG_QUEUE_PROPERTIES
   make ipc_msg_queue_microbench
fi

./get_memory_usage.sh  $MEMORY_DIR &
./bin/ipc_msg_queue_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP &

sleep 5

sudo ./profiler/profiler-sampling &

sleep $DURATION_XP
sudo pkill profiler
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
      ./profiler/parser-sampling /tmp/perf.data.* --c ${cid} --base-event ${e} --app ipc_msg_queue_m > $OUTPUT_DIR/perf_core_${cid}_event_${e}.log
   done
done
rm /tmp/perf.data.* -f
