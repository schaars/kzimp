#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds


# wait for the end of TIME_WAIT connections
function wait_for_time_wait {
nbc=1
while [ $nbc != 0 ]; do
   ./stop_all.sh
   echo "Waiting for the end of TIME_WAIT connections"
   sleep 20
   nbc=$(netstat -tn | grep TIME_WAIT | grep -v ":22 " | wc -l)
done
}


# get arguments
if [ $# -eq 3 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec>"
   exit 0
fi

OUTPUT_DIR="microbench_inet_tcp_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B"

if [ -d $OUTPUT_DIR ]; then
   echo Inet TCP ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B already done
   exit 0
fi

./stop_all.sh
wait_for_time_wait

sudo sysctl -p ../inet_sysctl.conf

# wait for TIME_WAIT connections
nbc=$(netstat -tn | grep TIME_WAIT | grep -v ":22 " | wc -l)
while [ $nbc != 0 ]; do
   ./stop_all.sh
   echo "Waiting for the end of TIME_WAIT connections"
   sleep 20
   nbc=$(netstat -tn | grep TIME_WAIT | grep -v ":22 " | wc -l)
done

# launch XP
timelimit -p -s 9 -t $((${DURATION_XP}+30)) ./bin/inet_tcp_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh

# save files
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/
