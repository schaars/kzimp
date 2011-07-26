#!/bin/bash
#
# Launch a Checkpointing XP with bfish_mprotect
# Args:
#   $1: nb nodes
#   $2: nb iter
#   $3: message max size
#   $4: checkpoint size
#   $5: number of messages in the channel


CONFIG_FILE=config

KBFISH_MEM_DIR="../kbfishmem"

if [ $# -eq 5 ]; then
   NB_NODES=$1
   NB_ITER=$2
   MESSAGE_MAX_SIZE=$3
   CHKPT_SIZE=$4
   MSG_CHANNEL=$5
 
else
   echo "Usage: ./$(basename $0) <nb_nodes> <nb_iter> <msg_max_size> <chkpt_size> <channel_size>"
   exit 0
fi

./stop_all.sh
./remove_shared_segment.pl
rm -f /tmp/checkpointing_node_0_finished

# create config file
./create_config.sh $NB_NODES $NB_ITER > $CONFIG_FILE

# Min size is the size of a uintptr_t: 8 bytes
if [ $MESSAGE_MAX_SIZE -lt 8 ]; then
   REAL_MSG_SIZE=8
else
   REAL_MSG_SIZE=$MESSAGE_MAX_SIZE
fi

echo "-DNB_MESSAGES=${MSG_CHANNEL} -DMESSAGE_MAX_SIZE=${REAL_MSG_SIZE} -DMESSAGE_MAX_SIZE_CHKPT_REQ=${CHKPT_SIZE} -DMESSAGE_BYTES=${REAL_MSG_SIZE}" > BFISH_MPROTECT_PROPERTIES
make bfish_mprotect_checkpointing
REAL_MSG_SIZE=$(./bin/bfishmprotect_get_struct_ump_message_size)

#compile and load module
cd $KBFISH_MEM_DIR
make
./kbfishmem.sh unload
./kbfishmem.sh load nb_max_communication_channels=${NB_NODES} default_channel_size=${MSG_CHANNEL} default_max_msg_size=${REAL_MSG_SIZE} 
if [ $? -eq 1 ]; then
   echo "An error has occured when loading kbfishmem. Aborting the experiment $OUTPUT_DIR"
   exit 0
fi
cd -

# launch
./bin/bfish_mprotect_checkpointing $CONFIG_FILE &


# wait for the end
F=/tmp/checkpointing_node_0_finished
n=0
while [ ! -e $F ]; do
   #if [ $n -eq 100 ]; then
   #   echo -e "\nbfish_mprotect_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B_${MSG_CHANNEL}channelSize\n\tTAKES TOO MUCH TIME" >> results.txt
   #   break
   #fi

   echo "Waiting for the end"
   sleep 10
   n=$(($n+1))
done

# save results
./stop_all.sh
./remove_shared_segment.pl
sleep 1
cd $KBFISH_MEM_DIR; ./kbfishmem.sh unload; cd -
mv results.txt bfish_mprotect_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B_${MSG_CHANNEL}channelSize.txt
