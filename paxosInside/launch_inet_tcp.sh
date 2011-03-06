#!/bin/bash
#
# Launch a PaxosInside XP with TCP
# Args:
#   $1: nb paxos nodes
#   $2: nb clients
#   $3: nb iter per client
#   $4: same_proc or different_proc


CONFIG_FILE=config


if [ $# -eq 4 ]; then
   NB_PAXOS_NODES=$1
   NB_CLIENTS=$2
   NB_ITER_PER_CLIENT=$3
   LEADER_ACCEPTOR=$4
else
   echo "Usage: ./$(basename $0) <nb_paxos_nodes> <nb_clients> <nb_iter_per_client> <same_proc|different_proc>"
   exit 0
fi

./stop_all.sh
rm -f /tmp/paxosInside_client_*_finished

# create config file
./create_config.sh $NB_PAXOS_NODES $NB_CLIENTS $NB_ITER_PER_CLIENT $LEADER_ACCEPTOR > $CONFIG_FILE

# Inet specific
sudo sysctl -p inet_sysctl.conf

# wait for TIME_WAIT connections
nbc=$(netstat -tn | grep TIME_WAIT | grep -v ":22 " | wc -l)
while [ $nbc != 0 ]; do
   ./stop_all.sh
   echo "Waiting for the end of TIME_WAIT connections"
   sleep 20
   nbc=$(netstat -tn | grep TIME_WAIT | grep -v ":22 " | wc -l)
done

# launch
./bin/inet_tcp_paxosInside $CONFIG_FILE &

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
mv results.txt inet_tcp_${NB_PAXOS_NODES}nodes_${NB_CLIENTS}clients_${NB_ITER_PER_CLIENT}iter_${LEADER_ACCEPTOR}.txt
