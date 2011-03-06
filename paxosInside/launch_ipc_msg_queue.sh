#!/bin/bash
#
# Launch a PaxosInside XP with Unix domain sockets
# Args:
#   $1: nb paxos nodes
#   $2: nb clients
#   $3: nb iter per client
#   $4: same_proc or different_proc
#   $5: do you want 1 or N queues?
#   $6: message max size


CONFIG_FILE=config


if [ $# -eq 6 ]; then
   NB_PAXOS_NODES=$1
   NB_CLIENTS=$2
   NB_ITER_PER_CLIENT=$3
   LEADER_ACCEPTOR=$4
   NB_QUEUES=$5
   if [ $NB_QUEUES == "1" ]; then
     ONE_QUEUE=-DONE_QUEUE
   else
     ONE_QUEUE=
   fi
   MESSAGE_MAX_SIZE=$6
 
else
   echo "Usage: ./$(basename $0) <nb_paxos_nodes> <nb_clients> <nb_iter_per_client> <same_proc|different_proc> <nb_queues> <msg_max_size>"
   exit 0
fi

./stop_all.sh
rm -f /tmp/paxosInside_client_*_finished
./remove_shared_segment.pl

# create config file
./create_config.sh $NB_PAXOS_NODES $NB_CLIENTS $NB_ITER_PER_CLIENT $LEADER_ACCEPTOR > $CONFIG_FILE

# IPC MQ specific
# used by ftok
touch /tmp/ipc_msg_queue_paxosInside

# kenel parameters
sudo ./root_set_value.sh $MESSAGE_MAX_SIZE /proc/sys/kernel/msgmax
sudo ./root_set_value.sh 100000000 /proc/sys/kernel/msgmnb

# compile
echo "-DIPC_MSG_QUEUE $ONE_QUEUE -DMESSAGE_MAX_SIZE=$MESSAGE_MAX_SIZE" > IPC_MSG_QUEUE_PROPERTIES
make ipc_msg_queue_paxosInside

# launch
./bin/ipc_msg_queue_paxosInside $CONFIG_FILE &

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
mv results.txt ipc_msg_queue_${NB_PAXOS_NODES}nodes_${NB_CLIENTS}clients_${NB_ITER_PER_CLIENT}iter_${LEADER_ACCEPTOR}_${NB_QUEUES}queues_${MESSAGE_MAX_SIZE}BmsgMaxSize.txt
