#!/bin/bash
#
# Launch a PaxosInside XP with Barrelfish MP
# Args:
#   $1: nb paxos nodes
#   $2: nb clients
#   $3: nb iter per client
#   $4: same_proc or different_proc
#   $5: message max size
#   $6: number of messages in the channel


CONFIG_FILE=config
MESSAGE_MAX_SIZE=128


if [ $# -eq 6 ]; then
   NB_PAXOS_NODES=$1
   NB_CLIENTS=$2
   NB_ITER_PER_CLIENT=$3
   LEADER_ACCEPTOR=$4
   MESSAGE_MAX_SIZE=$5
   MSG_CHANNEL=$5
 
else
   echo "Usage: ./$(basename $0) <nb_paxos_nodes> <nb_clients> <nb_iter_per_client> <same_proc|different_proc> <msg_max_size> <channel_size>"
   exit 0
fi

./stop_all.sh
rm -f /tmp/paxosInside_client_*_finished
./remove_shared_segment.pl

# create config file
./create_config.sh $NB_PAXOS_NODES $NB_CLIENTS $NB_ITER_PER_CLIENT $LEADER_ACCEPTOR > $CONFIG_FILE

# Barrelfish specific
# used by ftok
touch /tmp/barrelfish_message_passing_microbench

#set new parameters
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmall
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmmax

# compile
#echo "-DUSLEEP -DNB_MESSAGES=${MSG_CHANNEL} -DURPC_MSG_WORDS=$(( ${MESSAGE_MAX_SIZE}/8 ))" > BARRELFISH_MP_PROPERTIES
echo "-DNB_MESSAGES=${MSG_CHANNEL} -DURPC_MSG_WORDS=$(( ${MESSAGE_MAX_SIZE}/8 ))" > BARRELFISH_MP_PROPERTIES
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
./remove_shared_segment.pl
mv results.txt barrelfish_mp_${NB_PAXOS_NODES}nodes_${NB_CLIENTS}clients_${NB_ITER_PER_CLIENT}iter_${LEADER_ACCEPTOR}_${MSG_CHANNEL}channelSize.txt
