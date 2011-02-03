#!/bin/bash


NUM_CONSUMERS_ARRAY=( 1 3 5 7 )
MSG_SIZE_ARRAY=( 64 1024 10240 102400 1048576 )
NUM_DGRAMS_ARRAY=( 10 100 1000 )

for TRANSFER_SIZE in 1 2 3; do

for num_dgrams in ${NUM_DGRAMS_ARRAY[@]}; do

	for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

		for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

			echo "===== $num_consumers consumers, ${TRANSFER_SIZE}GB transfered, msg size is ${MSG_SIZE_ARRAY[$i]}B, $num_dgrams dgrams ====="
			./launch_unix.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} $(( ${TRANSFER_SIZE} * 1000000000 )) $num_dgrams

		done

	done

done

done