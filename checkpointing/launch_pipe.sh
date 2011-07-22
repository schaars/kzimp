#!/bin/bash
#
# Launch a Checkpointing XP with Pipes
# Args:
#   $1: nb nodes
#   $2: nb iter
#   $3: message max size
#   $4: checkpoint size
#   $5: limit thr in snap/s, optionnal


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

# create config file
./create_config.sh $NB_NODES $NB_ITER > $CONFIG_FILE

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DMESSAGE_MAX_SIZE_CHKPT_REQ=${CHKPT_SIZE}" > PIPE_PROPERTIES
make pipe_checkpointing

# launch
./bin/pipe_checkpointing $CONFIG_FILE &

# wait for the end
F=/tmp/checkpointing_node_0_finished
while [ ! -e $F ]; do
   echo "Waiting for the end"
   sleep 10
done


# save results
./stop_all.sh
rm -f /tmp/checkpointing_node_0_finished

mv results.txt pipe_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B.txt
