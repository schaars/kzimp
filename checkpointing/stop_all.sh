#!/bin/bash

pkill -f barrelfish_mp_checkpointing
pkill -f ulm_checkpointing
pkill -f kzimp_checkpointing
pkill -f bfish_mprotect_checkpointing
pkill -f kbfish_checkpointing
pkill -f inet_tcp_checkpointing
pkill -f inet_udp_checkpointing
