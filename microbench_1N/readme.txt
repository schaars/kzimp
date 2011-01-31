===========================================
===== Multicore replication -- readme =====
===========================================


++++++++++++++++++++++++++++++++++++++++
+++++ Machines with hyperthreading +++++

Set the value of the macro NB_THREADS_PER_CORE (file microbench.c) to the number of cores per threads in your machine.
This ensures that 2 processes of the benchmark are never located on the same core.


++++++++++++++++++++
+++++ Inet TCP +++++

You need the Inet configuration present in the file inet_sysctl.conf.
To take these parameters into account, run
  # sysctl -p inet_sysctl.conf
Note that this is done in the script (using sudo)

The benchmark is the following one:
  $ ./launch_inet_tcp.sh 
    Usage: ./launch_inet_tcp.sh <nb_consumers> <message_size_in_B> <duration_warmup_phase_sec> <duration_logging_phase_sec>


++++++++++++++++++++
+++++ Inet UDP +++++

You need the Inet configuration present in the file inet_sysctl.conf.
To take these parameters into account, run
  # sysctl -p inet_sysctl.conf
Note that this is done in the script (using sudo)

The benchmark is the following one:
  $ ./launch_inet_udp.sh 
    Usage: ./launch_inet_udp.sh <nb_consumers> <message_size_in_B> <duration_warmup_phase_sec> <duration_logging_phase_sec> [multicast]

Give a 5th argument if you want to enable multicast


++++++++++++++++++++++++++++++
+++++ Unix domain socket +++++

The benchmark is the following one:
  $ ./launch_unix.sh 
    Usage: ./launch_unix.sh <nb_consumers> <message_size_in_B> <duration_warmup_phase_sec> <duration_logging_phase_sec> <nb_max_datagrams>

The script sets /proc/sys/net/unix/max_dgram_qlen to <nb_max_datagrams>


++++++++++++++++
+++++ Pipe +++++

The benchmark is the following one:
  $ ./launch_pipe.sh 
    Usage: ./launch_pipe.sh <nb_consumers> <message_size_in_B> <duration_warmup_phase_sec> <duration_logging_phase_sec>


%TODO% pipe with vmsplice: there is a bug


+++++++++++++++++++++++++++++
+++++ IPC message queue +++++

The benchmark is the following one:
  $ ./launch_ipc_msg_queue.sh 
    Usage: ./launch_ipc_msg_queue.sh <nb_consumers> <message_size_in_B> <nb_messages_warmup_phase> <nb_messages_logging_phase> <nb_queues> <message_max_size>
  
nb_queues: defines if there is 1 queue shared between the consumers or 1 queue per consumer (in this case set this value to N)
message_max_size: defines the size of the mtext field in the IPC message structure

%TODO modify files in /proc


+++++++++++++++++++++++++++++++
+++++ POSIX message queue +++++

The benchmark is the following one:
  $ ./launch_posix_msg_queue.sh 
    Usage: ./launch_posix_msg_queue.sh <nb_consumers> <message_size_in_B> <nb_messages_warmup_phase> <nb_messages_logging_phase>

%TODO modify files in /proc




++++++++++++++++++++++++++++++++++++++
+++++ Barrelfish message-passing +++++

The benchmark is the following one:
  $ ./launch_barrelfish.sh 
    Usage: ./launch_barrelfish.sh <nb_consumers> <message_size_in_B> <nb_messages_warmup_phase> <nb_messages_logging_phase> <nb_msg_channel>


%TODO%
%other communication mechanisms%


%Local Multicast

%User-level Local Multicast with 0-copy
