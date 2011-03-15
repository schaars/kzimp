#!/bin/bash
#
# Launch a PaxosInside XP with POSIX msg queue
# Args:
#   $1: nb paxos nodes
#   $2: nb clients
#   $3: nb iter per client
#   $4: same_proc or different_proc
#   $5: message max size
#   $6: number of messages in the channel


CONFIG_FILE=config


if [ $# -eq 6 ]; then
   NB_PAXOS_NODES=$1
   NB_CLIENTS=$2
   NB_ITER_PER_CLIENT=$3
   LEADER_ACCEPTOR=$4
   MESSAGE_MAX_SIZE=$5
   MSG_CHANNEL=$6
 
else
   echo "Usage: ./$(basename $0) <nb_paxos_nodes> <nb_clients> <nb_iter_per_client> <same_proc|different_proc> <msg_max_size> <channel_size>"
   exit 0
fi

sudo ./stop_all.sh
sudo rm -f /tmp/paxosInside_client_*_finished

# create config file
./create_config.sh $NB_PAXOS_NODES $NB_CLIENTS $NB_ITER_PER_CLIENT $LEADER_ACCEPTOR > $CONFIG_FILE

# POSIX MQ specific
# rm old queues
sudo rm /dev/mqueue/posix_message_queue_paxosInside*

#set new parameters
sudo ./root_set_value.sh 32 /proc/sys/fs/mqueue/queues_max
sudo ./root_set_value.sh $MSG_CHANNEL /proc/sys/fs/mqueue/msg_max
sudo ./root_set_value.sh ${MESSAGE_MAX_SIZE} /proc/sys/fs/mqueue/msgsize_max

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE}" > POSIX_MSG_QUEUE_PROPERTIES
make posix_msg_queue_paxosInside

# launch
sudo ./bin/posix_msg_queue_paxosInside $CONFIG_FILE &

# wait for the end
nbc=0
while [ $nbc -ne $NB_CLIENTS ]; do
   echo "Waiting for the end: nbc=$nbc / $NB_CLIENTS"
   sleep 10

   nbc=0
   for i in $(seq 0 $NB_CLIENTS); do
      F=/tmp/paxosInside_client_$(($i + $NB_PAXOS_NODES))_finished
      if [ -e $F ]; then
         nbc=$(($nbc+1))
      fi
   done
done

# save results
sudo ./stop_all.sh
sudo rm -f /tmp/paxosInside_client_*_finished
sudo chown bft:bft results.txt
mv results.txt posix_msg_queue_${NB_PAXOS_NODES}nodes_${NB_CLIENTS}clients_${NB_ITER_PER_CLIENT}iter_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize.txt