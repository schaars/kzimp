#!/bin/bash
# script to launch multiple clients
# args:
#   $1 = type (unix, inet, lm (for local multicast) or ipc)
#   $2 = nb_cores
#   $3 = nb_req
#   $4 = req_size
#   $5 = ack_laps
#   $6 = nb_msg
#   $7 = threshold

# if 0 args then print usage
if [ $# -eq 0 ]; then
  echo "Usage: $0 type nb_cores nb_req req_size ack_laps nb_msg"
  echo "Type: unix, inet or lm (for local multicast)"
  exit 0
fi

TYPE=$1
NB_CORES=$2
NB_REQ=$3
REQ_SIZE=$4
ACK_LAPS=$5
NB_MSG=$6
PROC_LM="/proc/local_multicast"

if [ -z $TYPE ]; then TYPE="inet"; fi
if [ -z $NB_CORES ]; then NB_CORES=4; fi
if [ -z $NB_REQ ]; then NB_REQ=10000; fi
if [ -z $REQ_SIZE ]; then REQ_SIZE=0; fi
if [ -z $ACK_LAPS ]; then ACK_LAPS=1; fi
if [ -z $NB_MSG ]; then NB_MSG=10; fi

# kill old ones
for TT in "inet" "unix" "lm" "ipc"; do
  killall ${TT}_server
  killall ${TT}_client
done

if [ "$TYPE" == "lm" ]; then
   echo 6001 6004 $NB_MSG > $PROC_LM
else
  echo 0 0 $NB_MSG > $PROC_LM
fi
  
# launch new ones
if [ "$TYPE" == "ipc" ]; then
   taskset -c 0 ./${TYPE}_server -n $NB_CORES -r $NB_REQ -s $REQ_SIZE -a $ACK_LAPS &
fi

for c in $( seq 1 $(( $NB_CORES-1 )) ); do
    taskset -c $c ./${TYPE}_client -c $c -s $REQ_SIZE -a $ACK_LAPS &
done

if [ ! "$TYPE" == "ipc" ]; then
   sleep 1
   taskset -c 0 ./${TYPE}_server -n $NB_CORES -r $NB_REQ -s $REQ_SIZE -a $ACK_LAPS
fi

