#!/bin/bash

#XXX: do not run it
exit 0


NUM_CONSUMERS_ARRAY=( 1 3 5 7 )
MSG_SIZE_ARRAY=( 1 64 128 512 1024 4096 10240 102400 1048576 )
NUM_MSG_BUFFER_ARRAY=( 10 )
XP_DURATION=$((2*60)) # 2 minutes

for num_msg_channel in ${NUM_MSG_BUFFER_ARRAY[@]}; do

	for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

		for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

			echo "===== $num_consumers consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B, $num_msg_channel messages in channel ====="
			./launch_local_multicast.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION} $num_msg_channel

		done

	done

done
