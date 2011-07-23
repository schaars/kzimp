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
nbc=0
while [ $nbc -ne 1 ]; do
   echo "Waiting for the end: nbc=$nbc / 1"
   sleep 10

   nbc=0
   for i in $(seq 0 2); do
      F=/tmp/paxosInside_client_$(($i + $NB_PAXOS_NODES))_finished
      if [ -e $F ]; then
         nbc=$(($nbc+1))
      fi
   done
done


# save results
./stop_all.sh
rm -f /tmp/checkpointing_node_0_finished
./remove_shared_segment.pl
mv results.txt ipc_msg_queue_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B.txt
