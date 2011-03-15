#!/bin/bash

NB_NODES_ARRAY=( 2 3 5 7 9 11 13 15 )
NB_ITER=10000000
MSG_SIZE_ARRAY=( 64 128 512 1024 4096 10240 102400 1048576 )

for nb_nodes in ${NB_NODES_ARRAY[@]}; do
for msg_size in ${MSG_SIZE_ARRAY[@]}; do

  if [ $msg_size -eq 102400 ] || [ $msg_size -eq 1048576 ]; then
    NB_ITER=10000
  elif [ $msg_size -eq 10240 ]; then
    NB_ITER=1000000
  else
    NB_ITER=10000000
  fi 

  ./launch_ulm.sh $nb_nodes $NB_ITER $msg_size 50
  ./launch_barrelfish_mp.sh $nb_nodes $NB_ITER $msg_size 1000
 
done
done
