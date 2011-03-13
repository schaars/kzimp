#!/bin/bash

NB_NODES_ARRAY=( 2 3 5 7 )
NB_ITER=150
CHKPT_PERIOD=100
SNAP_PERIOD=500
MSG_SIZE_ARRAY=( 64 128 512 1024 4096 10240 102400 1048576 )

for nb_nodes in ${NB_NODES_ARRAY[@]}; do
for msg_size in ${MSG_SIZE_ARRAY[@]}; do

  ./launch_ulm.sh $NB_NODES $NB_ITER $CHKPT_PERIOD $SNAP_PERIOD $msg_size 50
  ./launch_barrelfish_mp.sh $NB_NODES $NB_ITER $CHKPT_PERIOD $SNAP_PERIOD $msg_size 1000
  
done
done
