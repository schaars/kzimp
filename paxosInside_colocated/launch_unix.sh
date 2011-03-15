#!/bin/bash
#
# Launch a PaxosInside XP with Unix domain sockets
# Args:
#   $1: nb paxos nodes
#   $2: nb clients
#   $3: nb iter per client
#   $4: same_proc or different_proc
#   $5: message max size
#   $6: max number of datagrams


CONFIG_FILE=config


if [ $# -eq 6 ]; then
   NB_PAXOS_NODES=$1
   NB_CLIENTS=$2
   NB_ITER_PER_CLIENT=$3
   LEADER_ACCEPTOR=$4
   MESSAGE_MAX_SIZE=$5
   NB_DGRAMS=$6
else
   echo "Usage: ./$(basename $0) <nb_paxos_nodes> <nb_clients> <nb_iter_per_client> <same_proc|different_proc> <msg_max_size> <nb_dgrams>"
   exit 0
fi

./stop_all.sh
rm -f /tmp/paxosInside_client_*_finished

# create config file
./create_config.sh $NB_PAXOS_NODES $NB_CLIENTS $NB_ITER_PER_CLIENT $LEADER_ACCEPTOR > $CONFIG_FILE

# Unix domain specific
rm -f /tmp/multicore_replication_paxosInside_*
sudo ./root_set_value.sh $NB_DGRAMS /proc/sys/net/unix/max_dgram_qlen

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE}" > UNIX_PROPERTIES
make unix_paxosInside

# launch
./bin/unix_paxosInside $CONFIG_FILE &

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
rm -f /tmp/multicore_replication_paxosInside_*
mv results.txt unix_${NB_PAXOS_NODES}nodes_${NB_CLIENTS}clients_${NB_ITER_PER_CLIENT}iter_${LEADER_ACCEPTOR}_${NB_DGRAMS}channelSize.txt