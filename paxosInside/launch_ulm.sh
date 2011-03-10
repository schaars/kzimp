#!/bin/bash
#
# Launch a PaxosInside XP with ULM
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

./stop_all.sh
rm -f /tmp/paxosInside_client_*_finished
./remove_shared_segment.pl

# create config file
./create_config.sh $NB_PAXOS_NODES $NB_CLIENTS $NB_ITER_PER_CLIENT $LEADER_ACCEPTOR > $CONFIG_FILE

# ULM specific
# used by ftok
touch /tmp/ulm_paxosInside_acceptor_multicast
touch /tmp/ulm_paxosInside_leader_to_acceptor
touch /tmp/ulm_paxosInside_clients_to_leader
for n in $(seq 0 $(( $NB_CLIENTS - 1 ))); do
   touch /tmp/ulm_paxosInside_leader_to_client_${n}
done

#set new parameters
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmall
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmmax

# compile
echo "-DULM -DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DNB_MESSAGES=${MSG_CHANNEL}" > ULM_PROPERTIES
#echo "-DUSLEEP -DULM -DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DNB_MESSAGES=${MSG_CHANNEL}" > ULM_PROPERTIES
make ulm_paxosInside

# launch
./bin/ulm_paxosInside $CONFIG_FILE &

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
./remove_shared_segment.pl
mv results.txt ulm_${NB_PAXOS_NODES}nodes_${NB_CLIENTS}clients_${NB_ITER_PER_CLIENT}iter_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize.txt
