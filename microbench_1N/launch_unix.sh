#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: amount to transfert
#  $4: max nb datagrams


# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   transfert_SIZE=$3
   NB_DATAGRAMS=$4
else
   echo "Usage: ./$(basename $0) <nb_consumers> <amount_to_transfert_in_B> <amount_to_transfert_in_B> <nb_datagrams>"
   exit 0
fi


./stop_all.sh

# modify the max number of datagrams
sudo ./root_set_value.sh $NB_DATAGRAMS /proc/sys/net/unix/max_dgram_qlen

./bin/unix_microbench -r $NB_CONSUMERS -s $MSG_SIZE -n $transfert_SIZE

./stop_all.sh

# save files
OUTPUT_DIR="microbench_unix_${NB_CONSUMERS}consumers_${transfert_SIZE}Btransferted_${MSG_SIZE}B_${NB_DATAGRAMS}dgrams"
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/