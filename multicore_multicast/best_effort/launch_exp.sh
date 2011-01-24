#!/bin/bash
# launch experiements


nbc=4

#sleeping during 10, 20, 30, 50 and 60 minutes
#DURATION[0]=10
#DURATION[1000]=20
#DURATION[10000]=30
#DURATION[100000]=50
#DURATION[1000000]=60

DURATION[0]=7
DURATION[1000]=15
DURATION[10000]=20
DURATION[100000]=30
DURATION[1000000]=60



# launch 1 experiment
function launch_unlimited {
limit=0

#echo "limit=${limit}" >> $output

for reqsize in 0 1000 10000 100000 1000000; do
   # duration is in seconds
   duration=$(( ${DURATION[$reqsize]} * 60 ))
   echo "duration=${DURATION[$reqsize]} min" >> $output
   echo "reqsize=${reqsize}" >> $output

   ./launch_replicas.sh $type $nbc $duration $limit $reqsize $nb_msg $st >> $output
   sleep 1; # not to have interweaving in output
done

#for reqsize in 100000 1000000; do
#   # duration is in seconds
#   duration=$(( ${DURATION[$reqsize]} * 60 ))
#   echo "duration=${DURATION[$reqsize]} min" >> $output
#   echo "reqsize=${reqsize}" >> $output
#
#   ./launch_replicas.sh $type $nbc $duration $limit $reqsize 100 $st >> $output
#   sleep 1; # not to have interweaving in output
#done
}


# launch 1 lm experiment
function launch_1_lm {
   duration=$(( ${DURATION[$reqsize]} * 60 ))
   #echo "duration=${DURATION[$reqsize]} min" >> $output
   echo "reqsize=${reqsize}" >> $output

   while [ $limit_down -lt $limit_up ]; do
      ./launch_replicas.sh $type $nbc $duration $limit_up $reqsize 100 >> $output
      sleep 1; # not to have interweaving in output
      limit_up=$(( $limit_up-$limit_step))
   done
}


# launch remaining lm experiments
function launch_all_lm {
reqsize=1000
limit_up=183000
limit_down=180000
limit_step=1000

launch_1_lm

reqsize=10000
limit_up=75000
limit_down=60000
limit_step=1000

launch_1_lm

reqsize=100000
limit_up=11000
limit_down=5000
limit_step=1000

launch_1_lm

reqsize=1000000
limit_up=600
limit_down=300
limit_step=50

launch_1_lm
}


########################
# unlimited throughput #
########################
#output=thr_unlimited.txt
#echo "nb cores=${nbc}" >> $output
##for type in 'inet' 'unix' 'ipc' 'lm' 'ulm'; do

#type='ulm'
#for st in 100; do
#   output="thr_unlimited_ulm_sleep${st}.txt"
#   launch_unlimited
#done

#type="lm"
#output="thr_lm.txt"
#launch_all_lm

type="ulm"
for nb_msg in 10 100; do
   for st in 0 10 100; do
      output="thr_unlimited_ulm_${nb_msg}_${st}.txt"
      launch_unlimited
   done
done
