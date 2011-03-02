#!/bin/bash


NUM_CONSUMERS_ARRAY=( 1 3 5 7 )
MSG_SIZE_ARRAY=( 1 64 128 512 1024 4096 10240 102400 1048576 )

XP_DURATION=$((2*60))   # 2 minutes

for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

	for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

		echo "===== $(date) $num_consumers consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B ====="
		./launch_inet_udp.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION}

	done

done