#!/bin/bash


NUM_CONSUMERS=1
#msg size = 102400 or 1048576: mq_open error: cannot allocate memory
#MSG_SIZE_ARRAY=( 64 1024 4096 10240 102400 1048576 )
MSG_SIZE_ARRAY=( 64 1024 4096 10240 )

XP_DURATION=$((5*60))   # 5 minutes

for core_id in 2 4 8; do

cat << EOF > POSIX_MSG_QUEUE_PROPERTIES
-DCORE_EXPERIMENT -DCORE_EXPERIMENT_CORE_ID=${core_id}
EOF

make posix_msg_queue_microbench

for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

	echo "===== core $core_id, $NUM_CONSUMERS consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B ====="
	./launch_posix_msg_queue.sh $NUM_CONSUMERS ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION}

done

mkdir microbench_posix_msg_queue_core${core_id}
mv microbench_posix_msg_queue_1consumers_* microbench_posix_msg_queue_core${core_id}/

done
