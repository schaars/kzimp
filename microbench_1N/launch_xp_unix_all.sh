#!/bin/bash


for nbdatagrams in 10 1000 100000; do

for consumers in 1 3 5 7; do


transfert=10000000000
reqsize=64

echo "===== $consumers consumers, $reqsize B ====="
./launch_unix.sh $consumers $reqsize $transfert $nbdatagrams


transfert=10000000000
reqsize=1024

echo "===== $consumers consumers, $reqsize B ====="
./launch_unix.sh $consumers $reqsize $transfert $nbdatagrams


transfert=10000000000
reqsize=4096

echo "===== $consumers consumers, $reqsize B ====="
./launch_unix.sh $consumers $reqsize $transfert $nbdatagrams


transfert=10000000000
reqsize=10240

echo "===== $consumers consumers, $reqsize B ====="
./launch_unix.sh $consumers $reqsize $transfert $nbdatagrams



transfert=10000000000
reqsize=102400

echo "===== $consumers consumers, $reqsize B ====="
./launch_unix.sh $consumers $reqsize $transfert $nbdatagrams



transfert=10000000000
reqsize=1024000

echo "===== $consumers consumers, $reqsize B ====="
./launch_unix.sh $consumers $reqsize $transfert $nbdatagrams

done

done
