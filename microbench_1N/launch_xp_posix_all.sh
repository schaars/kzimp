#!/bin/bash


NUM_CONSUMERS_ARRAY=( 1 3 5 7 )
#msg size = 102400 or 1048576: mq_open error: cannot allocate memory
#MSG_SIZE_ARRAY=( 64 1024 4096 10240 102400 1048576 )
MSG_SIZE_ARRAY=( 64 1024 4096 10240 )

XP_DURATION=$((5*60))   # 5 minutes

for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

	for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

		echo "===== $num_consumers consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B ====="
		./launch_posix_msg_queue.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION}

	done

done
