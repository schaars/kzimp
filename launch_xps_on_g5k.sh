#!/bin/bash

NB_RUNS=3
BASEDIR=/home/bft/multicore_replication_microbench


MICROBENCH=0
PAXOSINSIDE=0
CHECKPOINTING=0

function scp_to_rennes {
scp -i /home/bft/.ssh/id_dsa $1 paublin@frennes:
}


# Recupere un caractere unique
getc ()
{
  stty raw -echo
  tmp=`dd bs=1 count=1 2>/dev/null`
  eval $1='$tmp'
  stty cooked
}


# list of verifications
echo '===== WARNING!!! ====='
echo "Before continuing, make sure you have made the following modifications:"
echo "1) The number of nodes/receivers is correct in the scripts, i.e. if you are on the 8 cores, you cannot launch an XP with more than 8 nodes / 7 receivers)"
echo "2) The core association is correct (in particular on the bertha)"
echo "3) The macro NB_THREADS_PER_CORE has the right value (microbench_1N/src/microbench.c)"

echo "Is it ok? [Y|n]"
getc answer
if [ `echo "$answer" | grep "n"` ];then
   echo "The script is stopping."
   exit
else
   echo "Launching the script."
fi


for r in $(seq 1 $NB_RUNS); do

   if [ $MICROBENCH -eq 1 ]; then

      #microbench
      cd $BASEDIR/microbench_1N

      for c in barrelfish ul_lm pipe inet_tcp posix inet_udp ipc_msg_queue unix; do
         ./launch_xp_${c}_all.sh

         # get results
         rm -rf run$r
         mkdir run$r
         mv microbench*consumers* run$r/
         tar cfj microbench_${c}_g5k_run$r.tar.bz2 run$r
         scp_to_rennes microbench_${c}_g5k_run$r.tar.bz2
      done

   fi


   if [ $CHECKPOINTING -eq 1 ]; then

      # checkpointing
      cd $BASEDIR/checkpointing
      ./launch_xp_all.sh

      # get results
      rm -rf run$r
      mkdir run$r
      mv ulm* barrelfish* run$r/
      tar cfj chkpt_g5k_run$r.tar.bz2 run$r
      scp_to_rennes chkpt_g5k_run$r.tar.bz2

   fi


   if [ $PAXOSINSIDE -eq 1 ]; then

      # paxosInside
      cd $BASEDIR/paxosInside_distributed
      ./launch_xp_all.sh

      # get results
      rm -rf run$r
      mkdir run$r
      mv ulm* barrelfish* run$r/
      tar cfj paxosInside_g5k_run$r.tar.bz2 run$r
      scp_to_rennes paxosInside_g5k_run$r.tar.bz2

   fi

done
