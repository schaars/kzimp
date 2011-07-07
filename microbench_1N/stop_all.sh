#!/bin/bash

pkill -f inet_tcp_microbench
pkill -f inet_udp_microbench
pkill -f inet_udp_multicast_microbench
pkill -f unix_microbench
pkill -f pipe_microbench
pkill -f pipe_vmsplice_microbench
pkill -f ipc_msg_queue_microbench
pkill -f barrelfish_message_passing
pkill -f local_multicast_microbench
pkill -f ul_lm_0copy_microbench
pkill -f kzimp_microbench

pkill -f get_memory_usage.sh
