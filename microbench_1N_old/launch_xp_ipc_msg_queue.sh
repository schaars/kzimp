#!/bin/bash


# message max size = M
for nbmqueue in 1 N; do

for consumers in 1 3 5 7; do


warmup=100000
logging=10000000
reqsize=64

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = M ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue $reqsize


warmup=100000
logging=10000000
reqsize=1024

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = M ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue $reqsize


warmup=100000
logging=10000000
reqsize=4096

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = M ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue $reqsize


warmup=100000
logging=10000000
reqsize=10240

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = M ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue $reqsize



warmup=100000
logging=2000000
reqsize=102400

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = M ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue $reqsize



warmup=10000
logging=500000
reqsize=1024000

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = M ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue $reqsize

done

done



# message max size = max(M)
for nbmqueue in 1 N; do

for consumers in 1 3 5 7; do


warmup=100000
logging=10000000
reqsize=64

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = max(M) ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue 1024000


warmup=100000
logging=10000000
reqsize=1024

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = max(M) ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue 1024000


warmup=100000
logging=10000000
reqsize=4096

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = max(M) ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue 1024000


warmup=100000
logging=10000000
reqsize=10240

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = max(M) ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue 1024000



warmup=100000
logging=2000000
reqsize=102400

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = max(M) ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue 1024000



warmup=10000
logging=500000
reqsize=1024000

echo "===== $consumers consumers, $reqsize B, nb_mqueue $nbmqueue, max msg size = max(M) ====="
./launch_ipc_msg_queue.sh $consumers $reqsize $warmup $logging $nbmqueue 1024000

done

done
