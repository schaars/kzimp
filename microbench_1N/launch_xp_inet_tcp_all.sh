#!/bin/bash


for consumers in 1 3 5 7; do


warmup=100000
logging=50000000
reqsize=64

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $warmup $logging


warmup=100000
logging=25000000
reqsize=1024

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $warmup $logging


warmup=100000
logging=10000000
reqsize=4096

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $warmup $logging


warmup=100000
logging=10000000
reqsize=10240

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $warmup $logging



warmup=100000
logging=2000000
reqsize=102400

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $warmup $logging



warmup=10000
logging=500000
reqsize=1024000

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $warmup $logging

done
