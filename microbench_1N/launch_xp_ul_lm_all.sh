#!/bin/bash


NUM_CONSUMERS_ARRAY=( 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 )
MSG_SIZE_ARRAY=( 1 64 128 512 1024 4096 10240 102400 1048576 )
NUM_MSG_CHANNEL_ARRAY=( 50 )
XP_DURATION=$((2*60)) # 2 minutes

for num_msg_channel in ${NUM_MSG_CHANNEL_ARRAY[@]}; do

   for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

      for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

         echo "===== $(date) $num_consumers consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B, $num_msg_channel messages in channel ====="
         ./launch_ul_lm.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION} $num_msg_channel

      done

   done

done
