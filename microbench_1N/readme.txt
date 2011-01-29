===========================================
===== Multicore replication -- readme =====
===========================================


++++++++++++++++++++++++++++++++++++++++
+++++ Machines with hyperthreading +++++

Set the value of the macro NB_THREADS_PER_CORE (file microbench.c) to the number of cores per threads in your machine.
This ensures that 2 processes of the benchmark are never located to the same core.


++++++++++++++++++++
+++++ Inet TCP +++++

You need the TCP configuration present in the file tcp_sysctl.conf.
To take these parameters into account, run
  # sysctl -p tcp_sysctl.conf

The benchmark is the following one:
  $ ./launch_xp_inet_tcp.sh 
    Usage: ./launch_xp_inet_tcp.sh <nb_consumers> <message_size_in_B> <nb_messages_warmup_phase> <nb_message_logging_phase>





%TODO% for the other communication mechanisms
