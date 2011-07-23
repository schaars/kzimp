#!/bin/bash

NORMAL_XPS=1
if [ $NORMAL_XPS -eq 1 ]; then

   NB_NODES_ARRAY=( 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 )
   NB_ITER=100000
   MSG_SIZE_ARRAY=( 128 512 1024 4096 10240 102400 1048576 )
   CHKPT_SIZE_ARRAY=( 128 )

   for chkpt_size in ${CHKPT_SIZE_ARRAY[@]}; do
      for nb_nodes in ${NB_NODES_ARRAY[@]}; do
         for msg_size in ${MSG_SIZE_ARRAY[@]}; do

            ./launch_ulm.sh $nb_nodes $NB_ITER $msg_size $chkpt_size 50
            ./launch_barrelfish_mp.sh $nb_nodes $NB_ITER $msg_size $chkpt_size 1000
            ./launch_kzimp.sh $nb_nodes $NB_ITER $msg_size $chkpt_size 500
            ./launch_bfish_mprotect.sh $nb_nodes $NB_ITER $msg_size $chkpt_size 1000
            ./launch_kbfish.sh $nb_nodes $NB_ITER $msg_size $chkpt_size 1000
            ./launch_inet_tcp.sh $nb_nodes $NB_ITER $msg_size $chkpt_size
            ./launch_inet_udp.sh $nb_nodes $NB_ITER $msg_size $chkpt_size
            ./launch_unix.sh $nb_nodes $NB_ITER $msg_size $chkpt_size
            ./launch_pipe.sh $nb_nodes $NB_ITER $msg_size $chkpt_size

         done
      done
   done
fi


#### UDP LIMIT THROUGHPUT ####

UDP_LIMIT_THR=0
if [ $UDP_LIMIT_THR -eq 1 ]; then

   NB_NODES_ARRAY=( 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 )
   NB_ITER=100000
   MSG_SIZE_ARRAY=( 102400 1048576 )
   CHKPT_SIZE_ARRAY=( 128 )
   LIMIT_THR_ARRAY=( 50 100 150 200 250 300 350 400 450 500 )

   for chkpt_size in ${CHKPT_SIZE_ARRAY[@]}; do
      for nb_nodes in ${NB_NODES_ARRAY[@]}; do
         for msg_size in ${MSG_SIZE_ARRAY[@]}; do
            for limit_thr in ${LIMIT_THR_ARRAY[@]}; do

               ./launch_inet_udp.sh $nb_nodes $NB_ITER $msg_size $chkpt_size $limit_thr
               if [ $? -eq 1 ]; then
                  break
               fi

            done
         done
      done
   done
fi

