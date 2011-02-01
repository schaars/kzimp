#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: nb messages


# get arguments
if [ $# -eq 3 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   NB_MSG=$3
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <nb_messages>"
   exit 0
fi


./stop_all.sh

sudo sysctl -p inet_sysctl.conf

# wait for TIME_WAIT connections
nbc=$(netstat -tn | grep TIME_WAIT | grep -v ":22 " | wc -l)
while [ $nbc != 0 ]; do
   ./stop_all.sh
   echo "Waiting for the end of TIME_WAIT connections"
   sleep 20
   nbc=$(netstat -tn | grep TIME_WAIT | grep -v ":22 " | wc -l)
done

# launch XP
./bin/inet_tcp_microbench -r $NB_CONSUMERS -s $MSG_SIZE -n $NB_MSG

./stop_all.sh

# save files
OUTPUT_DIR="microbench_inet_tcp_${NB_CONSUMERS}consumers_${NB_MSG}messages_${MSG_SIZE}B"
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/