#!/bin/bash

NUM_CONSUMERS_ARRAY=( 1 )
MSG_SIZE_ARRAY=( 64 1024 )
NUM_MSG_BUFFER_ARRAY=( 1000 )
XP_DURATION=$((5*60)) # 5 minutes

for num_msg_channel in ${NUM_MSG_BUFFER_ARRAY[@]}; do

	for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

		for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

			echo "===== $num_consumers consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B, $num_msg_channel messages in channel ====="
			./launch_ul_lm.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION} $num_msg_channel

		done

	done

done

NUM_CONSUMERS_ARRAY=( 1 )
MSG_SIZE_ARRAY=( 10240 )
NUM_MSG_BUFFER_ARRAY=( 100 )
XP_DURATION=$((5*60)) # 5 minutes

for num_msg_channel in ${NUM_MSG_BUFFER_ARRAY[@]}; do

	for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

		for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

			echo "===== $num_consumers consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B, $num_msg_channel messages in channel ====="
			./launch_ul_lm.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION} $num_msg_channel

		done

	done

done

NUM_CONSUMERS_ARRAY=( 1 )
MSG_SIZE_ARRAY=( 1048576 )
NUM_MSG_BUFFER_ARRAY=( 10 )
XP_DURATION=$((5*60)) # 5 minutes

for num_msg_channel in ${NUM_MSG_BUFFER_ARRAY[@]}; do

	for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

		for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

			echo "===== $num_consumers consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B, $num_msg_channel messages in channel ====="
			./launch_ul_lm.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION} $num_msg_channel

		done

	done

done



NUM_CONSUMERS_ARRAY=( 3 5 7 )
MSG_SIZE_ARRAY=( 64 1024 10240 102400 1048576 )
NUM_MSG_BUFFER_ARRAY=( 10 )
XP_DURATION=$((5*60)) # 5 minutes

for num_msg_channel in ${NUM_MSG_BUFFER_ARRAY[@]}; do

	for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

		for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

			echo "===== $num_consumers consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B, $num_msg_channel messages in channel ====="
			./launch_ul_lm.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION} $num_msg_channel

		done

	done

done
