#!/bin/bash


NUM_CONSUMERS=1
MSG_SIZE_ARRAY=( 64 1024 10240 102400 1048576 )

XP_DURATION=$((5*60))   # 5 minutes

NB_DGRAMS=10

for core_id in 1 3 5 9; do

cat << EOF > UNIX_SOCKETS_PROPERTIES
-DCORE_EXPERIMENT -DCORE_EXPERIMENT_CORE_ID=${core_id}
EOF

make unix_microbench

for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

	echo "===== core $core_id, $NUM_CONSUMERS consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B ====="
	./launch_unix.sh $NUM_CONSUMERS ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION} ${NB_DGRAMS}

done

mkdir microbench_unix_core${core_id}
mv microbench_unix_1consumers_* microbench_unix_core${core_id}/

done
