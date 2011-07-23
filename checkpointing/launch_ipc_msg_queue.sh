#!/bin/bash
#
# Launch a Checkpointing XP with IPC Message Queue
# Args:
#   $1: nb paxos nodes
#   $2: nb iter per client
#   $3: same_proc or different_proc
#   $4: if given, then activate profiling


CONFIG_FILE=config


if [ $# -eq 4 ]; then
   NB_NODES=$1
   NB_ITER=$2
   MESSAGE_MAX_SIZE=$3
   CHKPT_SIZE=$4
 
else
   echo "Usage: ./$(basename $0) <nb_nodes> <nb_iter> <msg_max_size> <chkpt_size>"
   exit 0
fi

./stop_all.sh
rm -f /tmp/checkpointing_node_0_finished
./remove_shared_segment.pl

# used by ftok
touch /tmp/ipc_msg_queue_checkpointing

# kenel parameters
sudo ./root_set_value.sh $MESSAGE_MAX_SIZE /proc/sys/kernel/msgmax
sudo ./root_set_value.sh 100000000 /proc/sys/kernel/msgmnb

# create config file
./create_config.sh $NB_NODES $NB_ITER > $CONFIG_FILE

# compile
echo "-DIPC_MSG_QUEUE -DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DMESSAGE_MAX_SIZE_CHKPT_REQ=${CHKPT_SIZE}" > POSIX_MSG_QUEUE_PROPERTIES
make ipc_msg_queue_checkpointing


# launch
./bin/ipc_msg_queue_checkpointing $CONFIG_FILE &


# wait for the end
F=/tmp/checkpointing_node_0_finished
n=0
while [ ! -e $F ]; do
   #if [ $n -eq 360 ]; then
   #   echo "TAKING TOO MUCH TIME: 3600 seconds" >> results.txt
   #   ./stop_all.sh
   #   exit 1
   #fi

   echo "Waiting for the end"
   sleep 10
   n=$(($n+1))
done


# save results
./stop_all.sh
rm -f /tmp/checkpointing_node_0_finished
./remove_shared_segment.pl
mv results.txt ipc_msg_queue_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B.txt
