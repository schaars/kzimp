#!/bin/bash
#
# Launch a Checkpointing XP with kbfish
# Args:
#   $1: nb nodes
#   $2: nb iter
#   $3: message max size
#   $4: checkpoint size
#   $5: number of messages in the channel


CONFIG_FILE=config

KBFISH_DIR="../kbfish"

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
rm -f /tmp/checkpointing_node_0_finished

# create config file
./create_config.sh $NB_NODES $NB_ITER > $CONFIG_FILE

# compile and load module
NB_MAX_CHANNELS=$(($NB_NODES-1))
cd $KBFISH_DIR
echo "-DMESSAGE_BYTES=${MESSAGE_MAX_SIZE}" > KBFISH_PROPERTIES
make
./kbfish.sh unload
./kbfish.sh load nb_max_communication_channels=${NB_MAX_CHANNELS} default_channel_size=${MSG_CHANNEL} default_max_msg_size=${MESSAGE_MAX_SIZE}
if [ $? -eq 1 ]; then
   echo "An error has occured when loading kzimp. Aborting the experiment"
   exit 0
fi

cd -

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DMESSAGE_MAX_SIZE_CHKPT_REQ=${CHKPT_SIZE}" > KBFISH_PROPERTIES
make kbfish_checkpointing

# launch
./bin/kbfish_checkpointing $CONFIG_FILE &

# wait for the end
F=/tmp/checkpointing_node_0_finished
n=0
while [ ! -e $F ]; do
   #if [ $n -eq 100 ]; then
   #   echo -e "\nkbfish_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B_${MSG_CHANNEL}channelSize\n\tTAKES TOO MUCH TIME" >> results.txt
   #   break
   #fi

   echo "Waiting for the end"
   sleep 10
   n=$(($n+1))
done

# save results
./stop_all.sh
sleep 1
cd $KBFISH_DIR; ./kbfish.sh unload; cd -
mv results.txt kbfish_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B_${MSG_CHANNEL}channelSize.txt
