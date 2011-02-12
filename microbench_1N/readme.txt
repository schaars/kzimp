===========================================
===== Multicore replication -- readme =====
===========================================


Note: not up-to-date!

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
    Usage: ./launch_inet_tcp.sh <nb_consumers> <message_size_in_B> <nb_messages>


++++++++++++++++++++
+++++ Inet UDP +++++

You need the Inet configuration present in the file inet_sysctl.conf.
To take these parameters into account, run
  # sysctl -p inet_sysctl.conf
Note that this is done in the script (using sudo)

The benchmark is the following one:
  $ ./launch_inet_udp.sh 
    Usage: ./launch_inet_udp.sh <nb_consumers> <message_size_in_B> <nb_messages> [multicast]

Give a 5th argument if you want to enable multicast


++++++++++++++++++++++++++++++
+++++ Unix domain socket +++++

The benchmark is the following one:
  $ ./launch_unix.sh 
    Usage: ./launch_unix.sh <nb_consumers> <message_size_in_B> <nb_messages> <nb_max_datagrams>

The script sets /proc/sys/net/unix/max_dgram_qlen to <nb_max_datagrams>


++++++++++++++++
+++++ Pipe +++++

The benchmark is the following one:
  $ ./launch_pipe.sh 
    Usage: ./launch_pipe.sh <nb_consumers> <message_size_in_B> <nb_messages>

You can also use pipes with vmsplice.
However, it requires that the memory area used to send the message must be left untouched until you know that the message has been handled


+++++++++++++++++++++++++++++
+++++ IPC message queue +++++

The benchmark is the following one:
  $ ./launch_ipc_msg_queue.sh 
    Usage: ./launch_ipc_msg_queue.sh <nb_consumers> <message_size_in_B> <nb_messages> <nb_queues> <message_max_size>
  
nb_queues: defines if there is 1 queue shared between the consumers or 1 queue per consumer (in this case set this value to N)
message_max_size: defines the size of the mtext field in the IPC message structure

Files /proc/sys/kernel/msgmax and /proc/sys/kernel/msgmnb are modified by the script.


+++++++++++++++++++++++++++++++
+++++ POSIX message queue +++++

The benchmark is the following one:
  $ ./launch_posix_msg_queue.sh 
    Usage: ./launch_posix_msg_queue.sh <nb_consumers> <message_size_in_B> <nb_messages>

/Files proc/sys/fs/mqueue/msg_max and /proc/sys/fs/mqueue/msgsize_max are modified by the script.


++++++++++++++++++++++++++++++++++++++
+++++ Barrelfish message passing +++++

The benchmark is the following one:
  $ ./launch_barrelfish_mp.sh 
    Usage: ./launch_posix_msg_queue.sh <nb_consumers> <message_size_in_B> <nb_messages> <nb_msg_channel>

nb_msg_channel controls the size of the channel: its value is the maximal number of messages one can find in the channel

Files /proc/sys/kernel/shmall and /proc/sys/kernel/shmmax are modified by the script.




%TODO%
%other communication mechanisms%

%Local Multicast

%User-level Local Multicast with 0-copy
