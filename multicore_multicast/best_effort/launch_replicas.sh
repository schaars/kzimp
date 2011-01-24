#!/bin/bash
# script to launch multiple clients
# args:
#   $1 = type (unix, inet, lm, ulm (for user-level local multicast) or ipc or ulm or ulm2)
#   $2 = nb_cores
#   $3 = duration
#   $4 = limit throughput (req/s)
#   $5 = req_size
#   $6 = nb_msg (local multicast only)
#   $7 = sleep_time (usec, ulm only)

# if 0 args then print usage
if [ $# -eq 0 ]; then
  echo "Usage: $0 type nb_cores duration limit_thr req_size nb_msg sleep_time"
  echo "Type: unix, inet, ipc, lm, lmunicast or ulm (User-level Local Multicast) or ulm2 or ipc0"
  echo "nb_msg used for type=lm or type=ulm or type=ulm"
  exit 0
fi

TYPE=$1
NB_CORES=$2
DURATION=$3
LIMIT_THR=$4
REQ_SIZE=$5
NB_MSG=$6
SLEEP_TIME=$7
PROC_LM="/proc/local_multicast"

if [ -z $TYPE ]; then TYPE="inet"; fi
if [ -z $NB_CORES ]; then NB_CORES=4; fi
if [ -z $DURATION ]; then DURATIOn=60; fi
if [ -z $LIMIT_THR ]; then LIMIT_THR=0; fi
if [ -z $REQ_SIZE ]; then REQ_SIZE=0; fi
if [ -z $NB_MSG ]; then NB_MSG=10; fi
if [ -z $SLEEP_TIME ]; then SLEEP_TIME=0; fi


# kill old ones
for TT in "inet" "unix" "lm" "lmunicast" "ipc" "ulm" "ipc0"; do
  killall ${TT}_server &> /dev/null
  killall ${TT}_client &> /dev/null
done


if [ "$TYPE" == "lm" ] || [ "$TYPE" == "lmunicast" ]; then
   echo 6001 6004 $NB_MSG > $PROC_LM
else
  echo 0 0 $NB_MSG > $PROC_LM
fi


if [ "$TYPE" == "unix" ]; then
   rm /tmp/core* -f
   echo 10 > /proc/sys/net/unix/max_dgram_qlen
fi


if [ "$TYPE" == "ipc" ]; then
   msgmax=$(( $REQ_SIZE + 32 ))
   msgmnb=$(( $msgmax * 10 ))
   echo $msgmnb > /proc/sys/kernel/msgmnb
   echo $msgmax > /proc/sys/kernel/msgmax
   for i in $(ipcs -q | grep 0x | awk '{print $2}'); do ipcrm -q $i; done

   gcc -o ipc_server -Wall -Werror -g -DMESSAGE_MAX_SIZE=$msgmax ipcmsgqueue.c timer.h server.c
   gcc -o ipc_client -Wall -Werror -g -DMESSAGE_MAX_SIZE=$msgmax ipcmsgqueue.c client.c
fi


if [ "$TYPE" == "ipc0" ]; then
   msgmax=$(( $REQ_SIZE + 32 ))
   msgmnb=$(( $msgmax * 10 ))
   echo $msgmnb > /proc/sys/kernel/msgmnb
   echo $msgmax > /proc/sys/kernel/msgmax
   for i in $(ipcs -q | grep 0x | awk '{print $2}'); do ipcrm -q $i; done

   gcc -o ipc0_server -Wall -Werror -g -DMESSAGE_MAX_SIZE=$msgmax ipc0copymsgqueue.c timer.h ipc0copy_server.c
   gcc -o ipc0_client -Wall -Werror -g -DMESSAGE_MAX_SIZE=$msgmax ipc0copymsgqueue.c ipc0copy_client.c
fi


if [ "$TYPE" == "ulm" ] || [ "$TYPE" == "ulm2" ]; then
   # compile
   msgmax=$(( $REQ_SIZE + 32 ))

   for i in $(ipcs -m | grep 0x | awk '{print $2}'); do ipcrm -m $i; done
   echo 10000000000 > /proc/sys/kernel/shmall
   echo 1000000000 > /proc/sys/kernel/shmmax

   gcc -o ulm_servent -Wall -Werror -g -DMESSAGE_MAX_SIZE=$msgmax mpsoc.c atomic.h timer.h ulm_servent.c
   gcc -o ulm2_servent -Wall -Werror -g -DMESSAGE_MAX_SIZE=$msgmax mpsoc2.c atomic.h timer.h ulm2_servent.c

   # launch
   ./${TYPE}_servent -n $NB_CORES -s $REQ_SIZE -l $LIMIT_THR -t $DURATION -m $NB_MSG -d $SLEEP_TIME &

   sleep 5

   # pin to each core
   c=0
   for pid in $(ps | grep ${TYPE}_servent | awk '{print $1}'); do
      taskset -c -p $c $pid &> /dev/null
      c=$(( $c+1 ))
   done

   sleep $(( $DURATION + 5 ))

   # killall
   for pid in $(ps | grep ${TYPE}_servent | awk '{print $1}'); do
      kill -2 $pid
   done

   exit 0
fi


# launch new ones
taskset -c 0 ./${TYPE}_server -n $NB_CORES -s $REQ_SIZE -l $LIMIT_THR -t $DURATION &
for c in $( seq 1 $(( $NB_CORES-1 )) ); do
   taskset -c $c ./${TYPE}_client -c $c -s $REQ_SIZE &
done

sleep $(( $DURATION + 5 ))
# kill -2 all clients
if [ "$TYPE" == lmunicast ]; then
   for pid in $(ps -A | grep ${TYPE} | awk '{print $1}'); do
      kill -2 $pid
   done
else
   for pid in $(ps -A | grep ${TYPE}_client | awk '{print $1}'); do
      kill -2 $pid
   done
fi
