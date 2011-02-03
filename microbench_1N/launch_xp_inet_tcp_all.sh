#!/bin/bash


NUM_CONSUMERS_ARRAY=( 1 3 5 7 )
TRANSFER_SIZE_ARRAY=( 1 1 1 1 1 )   # in GB
MSG_SIZE_ARRAY=( 64 1024 10240 102400 1048576 )

for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do

	for i in $(seq 0 $(( ${#TRANSFER_SIZE_ARRAY[@]}-1 )) ); do

		echo "===== $num_consumers consumers, ${TRANSFER_SIZE_ARRAY[$i]}GB transfered, msg size is ${MSG_SIZE_ARRAY[$i]}B ====="
		./launch_inet_tcp.sh $num_consumers ${MSG_SIZE_ARRAY[$i]} $(( ${TRANSFER_SIZE_ARRAY[$i]} * 1000000000 ))

	done

done