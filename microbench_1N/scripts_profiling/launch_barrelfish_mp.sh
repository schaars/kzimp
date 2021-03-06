#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: max nb of messages in the channel


#MEMORY_DIR="memory_conso"
PROFDIR=../profiler


# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   MAX_MSG_CHANNEL=$4
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> <max_nb_messages_in_channel>"
   exit 0
fi

OUTPUT_DIR="microbench_barrelfish_message_passing_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_${MAX_MSG_CHANNEL}nb_messages_channel"

if [ -d $OUTPUT_DIR ]; then
   echo Barrelfish ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B, ${MAX_MSG_CHANNEL} msg in channel already done
   exit 0
fi

#rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh
./remove_shared_segment.pl

# used by ftok
touch /tmp/barrelfish_message_passing_microbench

# modify shared mem parameters
sudo ./root_set_value.sh 10000000000 /proc/sys/kernel/shmall
sudo ./root_set_value.sh 10000000000 /proc/sys/kernel/shmmax

# recompile with message size
if [ $MSG_SIZE -lt 64 ]; then
   echo "-DNB_MESSAGES=$MAX_MSG_CHANNEL -DURPC_MSG_WORDS=$((64/8))" > BARRELFISH_MESSAGE_PASSING_PROPERTIES
else
   echo "-DNB_MESSAGES=$MAX_MSG_CHANNEL -DURPC_MSG_WORDS=$(($MSG_SIZE/8))" > BARRELFISH_MESSAGE_PASSING_PROPERTIES
fi
make barrelfish_message_passing

# launch XP
#./get_memory_usage.sh  $MEMORY_DIR &
./bin/barrelfish_message_passing -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP &

sleep 5

sudo $PROFDIR/profiler-sampling &

sleep $DURATION_XP
sudo pkill profiler

c=$(ps -A | grep barrelfish_mess | wc -l)
while [ $c -gt 0 ]; do
   sleep 20
   c=$(ps -A | grep barrelfish_mess | wc -l)
done

./stop_all.sh
./remove_shared_segment.pl

# save files
mkdir $OUTPUT_DIR
#mv $MEMORY_DIR $OUTPUT_DIR/
mv statistics*.log $OUTPUT_DIR/

sudo chown bft:bft /tmp/perf.data.*

for e in 0 1 2 3; do
   $PROFDIR/parser-sampling /tmp/perf.data.* --base-event ${e} > $OUTPUT_DIR/perf_everyone_event_${e}.log
done

rm /tmp/perf.data.* -f
