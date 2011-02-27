#!/bin/bash


NUM_CONSUMERS_ARRAY=( 1 3 5 7 )
MSG_SIZE_ARRAY=( 1 64 128 512 1024 4096 10240 102400 1048576 )
XP_DURATION=$((5*60)) # 5 minutes

for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

   for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

      if [ $num_consumers -eq 1 ]; then
         if [ ${MSG_SIZE_ARRAY[$i]} -le 4096 ]; then
            num_msg_channel=1000
         elif [ ${MSG_SIZE_ARRAY[$i]} -eq 10240 ]; then
            num_msg_channel=100
         else
            num_msg_channel=10
         fi
      else
         num_msg_channel=10
      fi

      echo "===== $num_consumers consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B, $num_msg_channel messages in channel ====="
      ./launch_ul_lm.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION} $num_msg_channel

   done

done
