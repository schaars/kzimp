#!/bin/bash

EXP="FAULT_$(hostname -s)"
r=0

while [ true ]; do

   for c in 5 11 17 23; do
      for s in 1024 4096 1048576; do

         ./launch_bfish_mprotect.sh $c $s 60 1000
         ./launch_inet_tcp.sh $c $s 60
         ./launch_inet_udp.sh $c $s 60
         ./launch_ipc_msg_queue.sh $c $s 60 N $s
         ./launch_kzimp.sh $c $s 60 500
         ./launch_pipe.sh $c $s 60
         ./launch_pipe_vmsplice.sh $c $s 60
         ./launch_posix_msg_queue.sh $c $s 60 10
         ./launch_unix.sh $c $s 60 10

      done
   done

   tar cfz ${EXP}_run$r.tar.gz microbench_*
   rm -rf microbench_*
   scp ${EXP}_run$r.tar.gz reims:
   r=$(($r+1))

done
