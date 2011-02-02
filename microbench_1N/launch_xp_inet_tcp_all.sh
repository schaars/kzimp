#!/bin/bash


for consumers in 1 3 5 7; do


transfert=10000000000
reqsize=64

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $transfert


transfert=10000000000
reqsize=1024

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $transfert


transfert=10000000000
reqsize=4096

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $transfert


transfert=10000000000
reqsize=10240

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $transfert



transfert=10000000000
reqsize=102400

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $transfert



transfert=10000000000
reqsize=1024000

echo "===== $consumers consumers, $reqsize B ====="
./launch_inet_tcp.sh $consumers $reqsize $transfert

done
