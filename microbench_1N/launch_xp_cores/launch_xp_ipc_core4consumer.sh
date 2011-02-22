#!/bin/bash


NUM_CONSUMERS=1
MSG_SIZE_ARRAY=( 64 1024 10240 102400 1048576 )

XP_DURATION=$((5*60))   # 5 minutes

num_queues=1

for core_id in 2 4 8; do

for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

	echo "===== core $core_id, $NUM_CONSUMERS consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B ====="
	

	echo "-DCORE_EXPERIMENT -DCORE_EXPERIMENT_CORE_ID=${core_id} -DONE_QUEUE -DMESSAGE_MAX_SIZE=${MSG_SIZE_ARRAY[$i]}" > IPC_MSG_QUEUE_PROPERTIES
	make ipc_msg_queue_microbench
	
	./launch_ipc_msg_queue.sh $NUM_CONSUMERS ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION} $num_queues ${MSG_SIZE_ARRAY[$i]} no_compile

done

mkdir microbench_ipc_msg_queue_core${core_id}
mv microbench_ipc_msg_queue_1consumers_* microbench_ipc_msg_queue_core${core_id}/

done
