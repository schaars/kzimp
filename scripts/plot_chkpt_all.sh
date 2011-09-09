#!/bin/bash

THROUGHPUT=0
IMPROVEMENT=0
NORMALIZE=1

_ADD_TITLE=0

#NB_NODES_ARRAY=( 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 )
NB_NODES_ARRAY=( )
#MSG_SIZE_ARRAY=( 128 512 1024 4096 10240 102400 1048576 )
MSG_SIZE_ARRAY=( 4096 )

# all the summaries
KZIMP_NO_CSUM=../kzimp/no_csum/summary_mean_stdev.txt
BARRELFISH_MP=../bfish/summary_mean_stdev.txt
IPC_MQ=../ipc_mq/summary_mean_stdev.txt
POSIX_MQ=../posix_mq/summary_mean_stdev.txt
TCP=../ipv4/tcp/summary_mean_stdev.txt
UDP=../ipv4/udp/summary_mean_stdev.txt
UNIX=../unix/summary_mean_stdev.txt
PIPES=../pipes/summary_mean_stdev.txt
PIPES_VMSPLICE=../pipes_vmsplice/summary_mean_stdev.txt

# scripts
PLOT_THR_MSG_SIZE=../../../scripts/plot_chkpt_given_msg_size.sh 
PLOT_THR_NB_NODES=../../../scripts/plot_chkpt_given_nb_nodes.sh 
PLOT_IMP_MSG_SIZE=../../../scripts/plot_improvement_chkpt_given_msg_size.sh 
PLOT_IMP_NB_NODES=../../../scripts/plot_improvement_chkpt_given_nb_nodes.sh 
PLOT_NOR_MSG_SIZE=../../../scripts/plot_normalize_chkpt_given_msg_size.sh 
PLOT_NOR_NB_NODES=../../../scripts/plot_normalize_chkpt_given_nb_nodes.sh 


echo "=== THROUGHPUT ==="
if [ $THROUGHPUT -eq 1 ]; then

   echo -n "nb nodes:"
   for n in ${NB_NODES_ARRAY[@]}; do
      echo -n " $n"

      LOG_SCALE=1 ADD_TITLE=${_ADD_TITLE} ${PLOT_THR_NB_NODES} \
         chkpt_kzimp_lat_${n}nodes_log lat $n \
         "EICoM no checksum" $KZIMP_NO_CSUM \
         "Unix domain" $UNIX \
         TCP $TCP \
         UDP $UDP \
         Pipes $PIPES \
         "Pipes + vmsplice()" $PIPES_VMSPLICE \
         "IPC MQ" $IPC_MQ \
         "POSIX MQ" $POSIX_MQ \
         "Barrelfish MP" $BARRELFISH_MP

   done
   echo ""

   echo -n "msg size:"
   for s in ${MSG_SIZE_ARRAY[@]}; do
      echo -n " $s"

      ADD_TITLE=${_ADD_TITLE} ${PLOT_THR_MSG_SIZE} \
         chkpt_kzimp_lat_${s}B lat $s \
         "EICoM no checksum" $KZIMP_NO_CSUM \
         "Unix domain" $UNIX \
         TCP $TCP \
         UDP $UDP \
         Pipes $PIPES \
         "Pipes + vmsplice()" $PIPES_VMSPLICE \
         "IPC MQ" $IPC_MQ \
         "POSIX MQ" $POSIX_MQ \
         "Barrelfish MP" $BARRELFISH_MP
   done
   echo ""

fi

echo "=== IMPROVEMENT ==="
if [ $IMPROVEMENT -eq 1 ]; then

   echo -n "nb nodes:"
   for n in ${NB_NODES_ARRAY[@]}; do
      echo -n " $n"

      ADD_TITLE=${_ADD_TITLE} ${PLOT_IMP_NB_NODES} \
         chkpt_kzimp_imp_lat_${n}nodes lat $n \
         "EICoM no checksum" $KZIMP_NO_CSUM \
         "Unix domain" $UNIX \
         TCP $TCP \
         UDP $UDP \
         Pipes $PIPES \
         "Pipes + vmsplice()" $PIPES_VMSPLICE \
         "IPC MQ" $IPC_MQ \
         "POSIX MQ" $POSIX_MQ \
         "Barrelfish MP" $BARRELFISH_MP
   done
   echo ""

   echo -n "msg size:"
   for s in ${MSG_SIZE_ARRAY[@]}; do
      echo -n " $s"

      ADD_TITLE=${_ADD_TITLE} ${PLOT_IMP_MSG_SIZE} \
         chkpt_kzimp_imp_lat_${s}B lat $s \
         "EICoM no checksum" $KZIMP_NO_CSUM \
         "Unix domain" $UNIX \
         TCP $TCP \
         UDP $UDP \
         Pipes $PIPES \
         "Pipes + vmsplice()" $PIPES_VMSPLICE \
         "IPC MQ" $IPC_MQ \
         "POSIX MQ" $POSIX_MQ \
         "Barrelfish MP" $BARRELFISH_MP

   done
   echo ""

fi


echo "=== NORMALIZE ==="
if [ $NORMALIZE -eq 1 ]; then

   echo -n "nb nodes:"
   for n in ${NB_NODES_ARRAY[@]}; do
      echo -n " $n"

      ADD_TITLE=${_ADD_TITLE} ${PLOT_NOR_NB_NODES} \
         chkpt_kzimp_imp_normalize_${n}nodes lat $n \
         "EICoM no checksum" $KZIMP_NO_CSUM \
         "Unix domain" $UNIX \
         TCP $TCP \
         UDP $UDP \
         Pipes $PIPES \
         "Pipes + vmsplice()" $PIPES_VMSPLICE \
         "IPC MQ" $IPC_MQ \
         "POSIX MQ" $POSIX_MQ \
         "Barrelfish MP" $BARRELFISH_MP

   done
   echo ""

   echo -n "msg size:"
   for s in ${MSG_SIZE_ARRAY[@]}; do
      echo -n " $s"

      ADD_TITLE=${_ADD_TITLE} ${PLOT_NOR_MSG_SIZE} \
         chkpt_kzimp_normalize_lat_${s}B lat $s \
         "EICoM no checksum" $KZIMP_NO_CSUM \
         "Unix domain" $UNIX \
         TCP $TCP \
         UDP $UDP \
         Pipes $PIPES \
         "Pipes + vmsplice()" $PIPES_VMSPLICE \
         "IPC MQ" $IPC_MQ \
         "POSIX MQ" $POSIX_MQ \
         "Barrelfish MP" $BARRELFISH_MP

   done
   echo ""

fi




# ++===========++
# || OLD LINES ||
# ++===========++
#
#
#=========================
#=== all but TCP + UDP ===
#=========================
#
#pdftk $(for i in {2..24}; do echo -n "chkpt_kzimp_lat_${i}nodes_log.pdf "; done) cat output chkpt_kzimp_lat_all_nodes_log.pdf
#
#pdftk chkpt_kzimp_lat_128B.pdf chkpt_kzimp_lat_512B.pdf chkpt_kzimp_lat_1024B.pdf chkpt_kzimp_lat_4096B.pdf chkpt_kzimp_lat_10240B.pdf chkpt_kzimp_lat_102400B.pdf chkpt_kzimp_lat_1048576B.pdf cat output chkpt_kzimp_lat_all_msg_size.pdf    
#
#
#=================
#=== TCP + UDP ===
#=================
#
#pdftk $(for i in {2..24}; do echo -n "chkpt_kzimp_hdr_csum_lat_${i}nodes_log.pdf "; done) cat output chkpt_kzimp_hdr_csum_lat_all_nodes_log.pdf
#
#pdftk chkpt_kzimp_hdr_csum_lat_128B.pdf chkpt_kzimp_hdr_csum_lat_512B.pdf chkpt_kzimp_hdr_csum_lat_1024B.pdf chkpt_kzimp_hdr_csum_lat_4096B.pdf chkpt_kzimp_hdr_csum_lat_10240B.pdf chkpt_kzimp_hdr_csum_lat_102400B.pdf chkpt_kzimp_hdr_csum_lat_1048576B.pdf cat output chkpt_kzimp_hdr_csum_lat_all_msg_size.pdf
#
#
#======================================
#=== improvement, all but TCP + UDP ===
#======================================
#
#pdftk $(for i in {2..24}; do echo -n "chkpt_kzimp_imp_lat_${i}nodes.pdf "; done) cat output chkpt_kzimp_imp_lat_all_nodes.pdf
#
#pdftk chkpt_kzimp_imp_lat_128B.pdf chkpt_kzimp_imp_lat_512B.pdf chkpt_kzimp_imp_lat_1024B.pdf chkpt_kzimp_imp_lat_4096B.pdf chkpt_kzimp_imp_lat_10240B.pdf chkpt_kzimp_imp_lat_102400B.pdf chkpt_kzimp_imp_lat_1048576B.pdf cat output chkpt_kzimp_imp_lat_all_msg_size.pdf    
#
#
#==============================
#=== improvement, TCP + UDP ===
#==============================
#
#pdftk $(for i in {2..24}; do echo -n "chkpt_kzimp_hdr_csum_imp_lat_${i}nodes.pdf "; done) cat output chkpt_kzimp_hdr_csum_imp_lat_all_nodes.pdf
#
#pdftk chkpt_kzimp_hdr_csum_imp_lat_128B.pdf chkpt_kzimp_hdr_csum_imp_lat_512B.pdf chkpt_kzimp_hdr_csum_imp_lat_1024B.pdf chkpt_kzimp_hdr_csum_imp_lat_4096B.pdf chkpt_kzimp_hdr_csum_imp_lat_10240B.pdf chkpt_kzimp_hdr_csum_imp_lat_102400B.pdf chkpt_kzimp_hdr_csum_imp_lat_1048576B.pdf cat output chkpt_kzimp_hdr_csum_imp_lat_all_msg_size.pdf

