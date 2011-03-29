#!/bin/bash

NB_RUNS=3
BASEDIR=/home/bft/multicore_replication_microbench


MICROBENCH=0
MICROBENCH2=0
PAXOSINSIDE=1
CHECKPOINTING=0

function scp_to_proton {
  #ssh -i /home/bft/.ssh/bertha_dsa aublin@proton
  scp -i /home/bft/.ssh/bertha_dsa $1 aublin@proton:
}

#for c in barrelfish ul_lm posix inet_udp inet_tcp pipe ipc_msg_queue unix; do
#for c in barrelfish ul_lm; do

for r in $(seq 1 $NB_RUNS); do

if [ $MICROBENCH -eq 1 ]; then

#microbench
cd $BASEDIR/microbench_1N


./launch_xp_${c}_all.sh

# get results
mkdir run$r
mv microbench*consumers* run$r/
tar cfj microbench_${c}_sci50_oddCons_run$r.tar.bz2 run$r
scp_to_proton microbench_${c}_sci50_oddCons_run$r.tar.bz2


fi


if [ $MICROBENCH2 -eq 1 ]; then

for n in 2 4 6; do


#microbench
cd $BASEDIR/microbench_1N_0$n


./launch_xp_${c}_all.sh

# get results
rm -rf run$r
mkdir run$r
mv microbench*consumers* run$r/
tar cfj microbench_${c}_sci50_1cons_cores0${n}_run$r.tar.bz2 run$r
scp_to_proton microbench_${c}_sci50_1cons_cores0${n}_run$r.tar.bz2


done

fi



if [ $CHECKPOINTING -eq 1 ]; then

# checkpointing
cd $BASEDIR/checkpointing
./launch_xp_all.sh

# get results
mkdir run$r
mv ulm* barrelfish* run$r/
tar cfj chkpt_sci50_evenCons_run$r.tar.bz2 run$r
scp_to_proton chkpt_sci50_evenCons_run$r.tar.bz2

fi


if [ $PAXOSINSIDE -eq 1 ]; then

# paxosInside
cd $BASEDIR/paxosInside_distributed
./launch_xp_all.sh

# get results
rm -rf run$r
mkdir run$r
mv ulm* barrelfish* run$r/
tar cfj paxosInside_sci50_ulm25_run$r.tar.bz2 run$r
scp_to_proton paxosInside_sci50_ulm25_run$r.tar.bz2

fi

done

#done

