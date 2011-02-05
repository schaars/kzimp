#!/bin/bash


NUM_CONSUMERS=1
MSG_SIZE_ARRAY=( 64 1024 10240 102400 1048576 )

XP_DURATION=$((5*60))   # 5 minutes

for core_id in 1 2 3 4 8 9; do

cat << EOF > INET_UDP_PROPERTIES
-DCORE_EXPERIMENT -DCORE_EXPERIMENT_CORE_ID=${core_id}
EOF

make inet_udp_microbench

for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

	echo "===== core $core_id, $NUM_CONSUMERS consumers, ${XP_DURATION} secondes, msg size is ${MSG_SIZE_ARRAY[$i]}B ====="
	./launch_inet_udp.sh $NUM_CONSUMERS ${MSG_SIZE_ARRAY[$i]} ${XP_DURATION}

done

mkdir microbench_inet_udp_core${core_id}
mv microbench_inet_udp_1consumers_* microbench_inet_udp_core${core_id}/

done
