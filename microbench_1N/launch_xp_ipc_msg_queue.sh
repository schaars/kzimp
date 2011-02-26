#!/bin/bash

#NUM_QUEUES_ARRAY=( 1 N )
NUM_QUEUES_ARRAY=( 1 )
#NUM_CONSUMERS_ARRAY=( 1 3 5 7 )
NUM_CONSUMERS_ARRAY=( 3 )
#MSG_SIZE_ARRAY=( 1 64 128 512 1024 4096 10240 102400 1048576 )
MSG_SIZE_ARRAY=( 10240 )

XP_DURATION=$((5*60))   # 5 minutes

#for MSG_MAX_SIZE in 0 1; do
for MSG_MAX_SIZE in 0; do

for num_queues in ${NUM_QUEUES_ARRAY[@]}; do

for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

	for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do
	
		if [ $MSG_MAX_SIZE -eq 0 ]; then
			msg_max_size=${MSG_SIZE_ARRAY[$i]}
		else
			msg_max_size=${MSG_SIZE_ARRAY[$(( ${#MSG_SIZE_ARRAY[@]}-1 ))]}
		fi

		echo "===== $num_consumers consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B, $num_queues queues, msg_max_size is $msg_max_size ====="
		./launch_ipc_msg_queue.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION} $num_queues $msg_max_size

	done

done

done

done
