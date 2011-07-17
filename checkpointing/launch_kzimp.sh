#!/bin/bash
#
# Launch a Checkpointing XP with kzimp
# Args:
#   $1: nb nodes
#   $2: nb iter
#   $3: message max size
#   $4: checkpoint size
#   $5: number of messages in the channel


CONFIG_FILE=config

KZIMP_DIR="../kzimp/kzimp"
#KZIMP_DIR="../kzimp/kzimp_smallbuff"
#KZIMP_DIR="../kzimp/kzimp_allMessagesArea"

# Do we compute the checksum?
COMPUTE_CHKSUM=0

# Writer's timeout
KZIMP_TIMEOUT=60000

# macro for the version with 1 channel per node i -> 0
# set it to 1 if you want 1 channel per node, 0 otherwise.
ONE_CHANNEL_PER_NODE=0


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
if [ $ONE_CHANNEL_PER_NODE -eq 1 ]; then
   NB_MAX_CHANNELS=$NB_NODES
else
   NB_MAX_CHANNELS=2
fi
cd $KZIMP_DIR
make
./kzimp.sh load nb_max_communication_channels=${NB_MAX_CHANNELS} default_channel_size=${MSG_CHANNEL} default_max_msg_size=${MESSAGE_MAX_SIZE} default_timeout_in_ms=${KZIMP_TIMEOUT} default_compute_checksum=${COMPUTE_CHKSUM}
if [ $? -eq 1 ]; then
   echo "An error has occured when loading kzimp. Aborting the experiment"
   exit 0
fi

cd -

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DMESSAGE_MAX_SIZE_CHKPT_REQ=${CHKPT_SIZE}" > KZIMP_PROPERTIES

if [ $ONE_CHANNEL_PER_NODE -eq 1 ]; then
   echo "-DONE_CHANNEL_PER_NODE" >> KZIMP_PROPERTIES
fi
make kzimp_checkpointing

# launch
./bin/kzimp_checkpointing $CONFIG_FILE &

# wait for the end
F=/tmp/checkpointing_node_0_finished
n=0
while [ ! -e $F ]; do
   if [ $n -eq 100 ]; then
      echo -e "\nkzimp_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B_${MSG_CHANNEL}channelSize\n\tTAKES TOO MUCH TIME" >> results.txt
      break
   fi

   echo "Waiting for the end"
   sleep 10
   n=$(($n+1))
done

# save results
./stop_all.sh
sleep 1 # if not present, then unloading the module fails because there is still a kzimp process
cd $KZIMP_DIR; ./kzimp.sh unload; cd -
mv results.txt kzimp_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B_${MSG_CHANNEL}channelSize.txt
