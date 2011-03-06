#!/bin/bash
#
# Launch a PaxosInside XP with Barrelfish MP
# Args:
#   $1: nb paxos nodes
#   $2: nb clients
#   $3: nb iter per client
#   $4: same_proc or different_proc
#   $5: number of messages in the channel


CONFIG_FILE=config


if [ $# -eq 5 ]; then
   NB_PAXOS_NODES=$1
   NB_CLIENTS=$2
   NB_ITER_PER_CLIENT=$3
   LEADER_ACCEPTOR=$4
   MSG_CHANNEL=$5
 
else
   echo "Usage: ./$(basename $0) <nb_paxos_nodes> <nb_clients> <nb_iter_per_client> <same_proc|different_proc> <channel_size>"
   exit 0
fi

./stop_all.sh
rm -f /tmp/paxosInside_client_*_finished

# create config file
./create_config.sh $NB_PAXOS_NODES $NB_CLIENTS $NB_ITER_PER_CLIENT $LEADER_ACCEPTOR > $CONFIG_FILE

# Barrelfish specific
# used by ftok
touch /tmp/barrelfish_message_passing_microbench

#set new parameters
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmall
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmmax

# compile
echo "-DNB_MESSAGES=${MSG_CHANNEL} -DURPC_MSG_WORDS=16" > BARRELFISH_MP_PROPERTIES
make barrelfish_mp_paxosInside

# launch
./bin/barrelfish_mp_paxosInside $CONFIG_FILE &

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
./stop_all.sh
mv results.txt barrelfish_mp_${NB_PAXOS_NODES}nodes_${NB_CLIENTS}clients_${NB_ITER_PER_CLIENT}iter_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize.txt
