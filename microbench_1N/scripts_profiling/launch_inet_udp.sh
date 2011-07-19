#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: optional. Set multicast if you want udp multicast


#MEMORY_DIR="memory_conso"
PROFDIR=../profiler


# get arguments
if [ $# -eq 3 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   MULTICAST=

elif [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   MULTICAST=multicast
   
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> [multicast]"
   exit 0
fi


if [ -z $MULTICAST ]; then
   OUTPUT_DIR="microbench_inet_udp_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B"
else
   OUTPUT_DIR="microbench_inet_udp_multicast_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B"
fi

if [ -d $OUTPUT_DIR ]; then
   echo Inet UDP ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B, multicast=$MULTICAST already done
   exit 0
fi

#rm -rf $MEMORY_DIR && mkdir $MEMORY_DIR

./stop_all.sh

# deactivate Local Multicast if present
if [ -e /proc/local_multicast ]; then
   sudo ./root_set_value.sh "0 0 0" /proc/local_multicast
fi

sudo sysctl -p ../inet_sysctl.conf

# launch XP
.#/get_memory_usage.sh  $MEMORY_DIR &
if [ -z $MULTICAST ]; then
   ./bin/inet_udp_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP &
else
   ./bin/inet_udp_multicast_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP &
fi

sleep 5

sudo $PROFDIR/profiler-sampling &

sleep $DURATION_XP
sudo pkill profiler

c=$(ps -A | grep inet_udp_microb | wc -l)
while [ $c -gt 0 ]; do
   sleep 20
   c=$(ps -A | grep inet_udp_microb | wc -l)
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
