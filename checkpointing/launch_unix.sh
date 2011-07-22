#!/bin/bash
#
# Launch a Checkpointing XP with Unix Domain sockets
# Args:
#   $1: nb nodes
#   $2: nb iter
#   $3: message max size
#   $4: checkpoint size
#   $5: limit thr in snap/s, optionnal


CONFIG_FILE=config


if [ $# -eq 5 ]; then
   NB_NODES=$1
   NB_ITER=$2
   MESSAGE_MAX_SIZE=$3
   CHKPT_SIZE=$4
   LIMIT_THR=$5

elif [ $# -eq 4 ]; then
   NB_NODES=$1
   NB_ITER=$2
   MESSAGE_MAX_SIZE=$3
   CHKPT_SIZE=$4
   LIMIT_THR=0
 
else
   echo "Usage: ./$(basename $0) <nb_nodes> <nb_iter> <msg_max_size> <chkpt_size> [limit_thr]"
   exit 0
fi

./stop_all.sh
rm -f /tmp/checkpointing_node_0_finished
rm -f /tmp/multicore_replication_checkpointing*

# create config file
./create_config.sh $NB_NODES $NB_ITER > $CONFIG_FILE

# set new parameters
sudo sysctl -p ../inet_sysctl.conf

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DMESSAGE_MAX_SIZE_CHKPT_REQ=${CHKPT_SIZE}" > unix_PROPERTIES
make unix_checkpointing

# launch
./bin/unix_checkpointing $CONFIG_FILE &

# wait for the end
F=/tmp/checkpointing_node_0_finished
n=0
while [ ! -e $F ]; do
   if [ $n -eq 360 ]; then
      echo "TAKING TOO MUCH TIME: 3600 seconds" >> results.txt
      ./stop_all.sh
      exit 1
   fi

   echo "Waiting for the end"
   sleep 10
   n=$(($n+1))
done

# save results
./stop_all.sh
rm -f /tmp/multicore_replication_checkpointing*

if [ $LIMIT_THR -gt 0 ]; then
   mv results.txt unix_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B_thr${LIMIT_THR}snapPerSec.txt
else
   mv results.txt unix_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B.txt
fi
