#!/bin/bash
#
# Plot all figures about chkpt

#NB_NODES_ARRAY=( 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 )
NB_NODES_ARRAY=( 2 6 12 18 24 )
MSG_SIZE_ARRAY=( 128 512 1024 4096 10240 102400 1048576 )
MSG_SIZE_STR_ARRAY=( 128B 512B 1kB 4kB 10kB 100kB 1MB )

OUTPUT_DIR=all_plots
GIVEN_NB_NODES=1
GIVEN_MSG_SIZE=1

if [ $GIVEN_NB_NODES -eq 1 ]; then

   # Given a number of nodes
   for nb_nodes in ${NB_NODES_ARRAY[@]}; do
      for thr_or_lat in thr lat; do

         # header checksum
         DIR=${OUTPUT_DIR}/given_nb_nodes/hdr_chksum
         if [ ! -d DIR ]; then mkdir -p $DIR; fi

         echo "${thr_or_lat} with header chksum and ${nb_nodes} nodes"
         $(dirname $0)/plot_chkpt_given_nb_nodes.sh \
            $DIR/chkpt_${thr_or_lat}_${nb_nodes}nodes \
            $thr_or_lat $nb_nodes \
            kZIMP_hdr_chksum kzimp_hdr_chksum/summary_mean_stdev.txt \
            TCP   tcp/summary_mean_stdev.txt \
            UDP   udp/summary_mean_stdev.txt


         # no checksum
         DIR=${OUTPUT_DIR}/given_nb_nodes/no_chksum
         if [ ! -d DIR ]; then mkdir -p $DIR; fi

         echo "${thr_or_lat} without chksum and ${nb_nodes} nodes"
         $(dirname $0)/plot_chkpt_given_nb_nodes.sh \
            $DIR/chkpt_${thr_or_lat}_${nb_nodes}nodes \
            $thr_or_lat $nb_nodes \
            kZIMP_no_chksum kzimp_no_chksum/summary_mean_stdev.txt \
            Unix_sockets   unix/summary_mean_stdev.txt \
            Pipes   pipe/summary_mean_stdev.txt \
            IPC_MQ   ipc_mq/summary_mean_stdev.txt \
            POSIX_MQ   posix_mq/summary_mean_stdev.txt \
            BFish   bfish_mprotect/summary_mean_stdev.txt

      done
   done

fi


if [ $GIVEN_MSG_SIZE -eq 1 ]; then

   # Given a message size
   for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do
      msg_size=${MSG_SIZE_ARRAY[$i]}
      msg_size_str=${MSG_SIZE_STR_ARRAY[$i]}

      for thr_or_lat in thr lat; do

         # header checksum
         DIR=${OUTPUT_DIR}/given_msg_size/hdr_chksum
         if [ ! -d DIR ]; then mkdir -p $DIR; fi

         echo "${thr_or_lat} with header chksum and checkpoints of ${msg_size_str}"
         $(dirname $0)/plot_chkpt_given_msg_size.sh \
            $DIR/chkpt_${thr_or_lat}_chkpt${msg_size_str} \
            $thr_or_lat $msg_size \
            kZIMP_hdr_chksum kzimp_hdr_chksum/summary_mean_stdev.txt \
            TCP   tcp/summary_mean_stdev.txt \
            UDP   udp/summary_mean_stdev.txt


         # no checksum
         DIR=${OUTPUT_DIR}/given_msg_size/no_chksum
         if [ ! -d DIR ]; then mkdir -p $DIR; fi

         echo "${thr_or_lat} without chksum and checkpoints of ${msg_size_str}"
         $(dirname $0)/plot_chkpt_given_msg_size.sh \
            $DIR/chkpt_${thr_or_lat}_chkpt${msg_size_str} \
            $thr_or_lat $msg_size \
            kZIMP_no_chksum kzimp_no_chksum/summary_mean_stdev.txt \
            Unix_sockets   unix/summary_mean_stdev.txt \
            Pipes   pipe/summary_mean_stdev.txt \
            IPC_MQ   ipc_mq/summary_mean_stdev.txt \
            POSIX_MQ   posix_mq/summary_mean_stdev.txt \
            BFish   bfish_mprotect/summary_mean_stdev.txt

      done
   done

fi

