#!/bin/bash

pkill -f barrelfish_mp_checkpointing
pkill -f ulm_checkpointing
pkill -f kzimp_checkpointing
pkill -f bfish_mprotect_checkpointing
pkill -f kbfish_checkpointing
pkill -f inet_tcp_checkpointing
pkill -f inet_udp_checkpointing
pkill -f unix_checkpointing
pkill -f pipe_checkpointing
pkill -f ipc_msg_queue_checkpointing
pkill -f posix_msg_queue_checkpointing

#sudo needed for knem
sudo pkill -f openmpi_checkpointing
sudo pkill -f mpich2_checkpointing
sudo pkill -f mpirun

