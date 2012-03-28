#!/bin/bash
#
# Launch a Checkpointing XP with mpich2
# Args:
#   $1: nb nodes
#   $2: nb iter
#   $3: message max size
#   $4: checkpoint size
#   $5: knem threshold (in bytes, 0 to deactivate knem)


CONFIG_FILE=config

if [ $# -eq 5 ]; then
   NB_NODES=$1
   NB_ITER=$2
   MESSAGE_MAX_SIZE=$3
   CHKPT_SIZE=$4
   KNEM_THRESH=$5
 
else
   echo "Usage: ./$(basename $0) <nb_nodes> <nb_iter> <msg_max_size> <chkpt_size> <knem_thresh>"
   exit 0
fi

./stop_all.sh
sudo rm -f /tmp/checkpointing_node_0_finished

# create config file
./create_config.sh $NB_NODES $NB_ITER > $CONFIG_FILE

# compile
echo "-DUSE_MPI -DMESSAGE_MAX_SIZE=${MESSAGE_MAX_SIZE} -DMESSAGE_MAX_SIZE_CHKPT_REQ=${CHKPT_SIZE}" > MPI_PROPERTIES
make mpich2_checkpointing

# launch
/home/bft/mpich2-install/bin/mpirun -np ${NB_NODES} ./bin/mpich2_checkpointing $CONFIG_FILE &

# wait for the end
F=/tmp/checkpointing_node_0_finished
n=0
while [ ! -e $F ]; do
   #if [ $n -eq 100 ]; then
   #   echo -e "\nopenmpi_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B\n\tTAKES TOO MUCH TIME" >> results.txt
   #   break
   #fi

   echo "Waiting for the end"
   sleep 10
   n=$(($n+1))
done

# save results
./stop_all.sh
OUTFILE="mpich2_${NB_NODES}nodes_${NB_ITER}iter_chkpt${CHKPT_SIZE}_msg${MESSAGE_MAX_SIZE}B.txt"

mv results.txt ${OUTFILE}
