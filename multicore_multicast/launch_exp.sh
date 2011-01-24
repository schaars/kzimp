#!/bin/bash
# script to launch all experiments

OUTFILE="/tmp/res.txt"
MEMOUTFILE="/tmp/memUsage.txt"
PROC_LM="/proc/local_multicast"

#sleeping during 10, 20, 30, 50 and 60 minutes
duration[0]=10
duration[1000]=20
duration[10000]=30
duration[100000]=50
duration[1000000]=60

#number of messages, for each acklaps value
nb_msg[0]=10
nb_msg[1]=10
nb_msg[10]=11
nb_msg[100]=101
nb_msg[1000]=1001


########## uncomment the following part for memory consumption experiments ##########
acklaps=1
reqsize=1000000
#for comm in "inet" "unix" "lm" "ipc"; do
comm=$1
if [ -z $comm ]; then
   echo "Need comm means as first arg. Aborting"
   exit 1
fi

#msgmax=$(( $reqsize + 32 ))
#msgmnb=$(( $msgmax * ${nb_msg[$acklaps]}  ))
#echo $msgmax > /proc/sys/kernel/msgmax
#echo $msgmnb > /proc/sys/kernel/msgmax

#gcc -o ipc_client -Wall -Werror -g -DMESSAGE_MAX_SIZE=$msgmax ipcmsgqueue.c ipc_client.c
#gcc -o ipc_server -Wall -Werror -g -DMESSAGE_MAX_SIZE=$msgmax ipcmsgqueue.c list.c timer.h ipc_server.c -lm


# launch memory script
./get_mem_usage.sh 1 m $MEMOUTFILE &

# sleep 5 minutes
sleep 5m

# launch replicas
echo '#Launching replicas' >> $MEMOUTFILE
./launch_replicas.sh $comm 4 10000 $reqsize $acklaps ${nb_msg[$acklaps]} 0 &
sleep ${duration[$reqsize]}m

# kill replicas
for TT in "inet" "unix" "lm" "ipc"; do
   killall ${TT}_server
   killall ${TT}_client
done
echo 6001 6004 ${nb_msg[$acklaps]} > $PROC_LM
echo '#Replicas have been killed' >> $MEMOUTFILE

# sleep 5 minutes
sleep 5m

# mv memory results
killall get_mem_usage.sh
mv $MEMOUTFILE resultsMemConso/xp${comm}_${reqsize}_${acklaps}.txt

# not this time because I want to retrieve results before
halt
#####################################################################################



########## uncomment the following part for 'do we have to use indexes or loop?' experiments ##########
#for acklaps in 0 1 10 100 1000; do
#for acklaps in 100; do
#   #for reqsize in 0 1000 10000 100000 1000000; do
#   for reqsize in 1000000; do
#
#      comm="lm"
#      #for nb_msg in 10 20 50 100 200 500 1000; do
#      for nb_msg in 1000; do
#         threshold=$((nb_msg - 5))
#         ./launch_replicas.sh $comm 4 10000 $reqsize $acklaps $nb_msg $threshold &
#         sleep ${duration[$reqsize]}m
#         mv $OUTFILE resultsIndexesListNeeded/xp${comm}_${reqsize}_${acklaps}_${nb_msg}_indexes.txt
#
#         threshold=$((nb_msg + 5))
#         ./launch_replicas.sh $comm 4 10000 $reqsize $acklaps $nb_msg $threshold &
#         sleep ${duration[$reqsize]}m
#         mv $OUTFILE resultsIndexesListNeeded/xp${comm}_${reqsize}_${acklaps}_${nb_msg}_loop.txt
#      done
#   done
#done
#######################################################################################################


########## uncomment the following part for throughput experiments ##########
#./killall_if_low_on_mem.sh &
#
#for acklaps in 0 1 10 100 1000; do
#for acklaps in 1000; do
#   #for reqsize in 0 1000 10000 100000 1000000; do
#   for reqsize in 10000 100000 1000000; do
##
##      #for comm in "inet" "unix"; do
##      #      ./launch_replicas.sh $comm 4 10000 $reqsize $acklaps &
##      #      sleep ${duration[$reqsize]}m
##      #      mv $OUTFILE results/xp${comm}_${reqsize}_${acklaps}.txt
##      #done
##
#      comm="lm"
#      if [[ $acklaps = 1000 && $reqsize = 1000000 ]]; then
#         echo "No exp for lm with acklaps=$acklaps and reqsize=$reqsize"
#      else
#         ./launch_replicas.sh $comm 4 10000 $reqsize $acklaps ${nb_msg[$acklaps]} 0 &
#         sleep ${duration[$reqsize]}m
#         mv $OUTFILE resultsThrLMListIndexes/xp${comm}_${reqsize}_${acklaps}.txt
#      fi
#
##      comm="ipc"
##      msgmax=$(( $reqsize + 32 ))
##      msgmnb=$(( $msgmax * ${nb_msg[$acklaps]}  ))
##      echo $msgmax > /proc/sys/kernel/msgmax
##      echo $msgmnb > /proc/sys/kernel/msgmax
##
##      gcc -o ipc_client -Wall -Werror -g -DMESSAGE_MAX_SIZE=$msgmax ipcmsgqueue.c ipc_client.c
##      gcc -o ipc_server -Wall -Werror -g -DMESSAGE_MAX_SIZE=$msgmax ipcmsgqueue.c list.c timer.h ipc_server.c -lm
##
##      ./launch_replicas.sh $comm 4 10000 $reqsize $acklaps ${nb_msg[$acklaps]} &
##      sleep ${duration[$reqsize]}m
##      mv $OUTFILE results/xp${comm}_${reqsize}_${acklaps}.txt
##
##      for TT in "inet" "unix" "lm" "ipc"; do
##         killall ${TT}_server
##         killall ${TT}_client
##      done
##      killall launch_replicas.sh
##      for i in $(ipcs -q | grep 0x | awk '{print $2}'); do
##         ipcrm -q $i;
##      done; 
##   
#   done
#done
#
#killall killall_if_low_on_mem.sh
#######################################################################################################


# kill remaining processes
for TT in "inet" "unix" "lm" "ipc"; do
   killall ${TT}_server
   killall ${TT}_client
done

