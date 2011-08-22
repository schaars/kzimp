#!/bin/bash
#
# Launch a Checkpointing XP with TCP
# Args:
#   $1: nb nodes
#   $2: nb iter
#   $3: message max size
#   $4: checkpoint size


CONFIG_FILE=config

# Set it to -DIPV6 if you want to enable IPV6
IPV6=

# wait for the end of TIME_WAIT connections
function wait_for_time_wait {
nbc=1
while [ $nbc != 0 ]; do
   ./stop_all.sh
   echo "Waiting for the end of TIME_WAIT connections"
   sleep 20
   nbc=$(netstat -tn | grep TIME_WAIT | grep -v ":22 " | wc -l)
done
}


if [ $# -eq 4 ]; then
   NB_NODES=$1
   NB_ITER=$2
   MESSAGE_MAX_SIZE=$3
   CHKPT_SIZE=$4
 
else
   echo "Usage: ./$(basename $0) <nb_nodes> <nb_iter> <msg_max_size> <chkpt_size>"
   exit 0
fi

./stop_all.sh
wait_for_time_wait
rm -f /tmp/checkpointing_node_0_finished

# create config file
./create_config.sh $NB_NODES $NB_ITER > $CONFIG_FILE

# set new parameters
sudo sysctl -p ../inet_sysctl.conf

# compile
echo "-DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DMESSAGE_MAX_SIZE_CHKPT_REQ=${CHKPT_SIZE} -DTCP_NAGLE ${IPV6}" > INET_TCP_PROPERTIES
make inet_tcp_checkpointing

# launch
./bin/inet_tcp_checkpointing $CONFIG_FILE &

# wait for the end
F=/tmp/checkpointing_node_0_finished
while [ ! -e $F ]; do
   echo "Waiting for the end"
   sleep 10
done

# save results
./stop_all.sh
mv results.txt inet_tcp_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B.txt
