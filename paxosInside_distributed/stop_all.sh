#!/bin/bash

pkill -f inet_tcp_paxosInside
pkill -f inet_udp_paxosInside
pkill -f unix_paxosInside
pkill -f pipe_paxosInside
pkill -f ipc_msg_queue_paxosInside
pkill -f posix_msg_queue_paxosInside
pkill -f barrelfish_mp_paxosInside
pkill -f ulm_paxosInside
