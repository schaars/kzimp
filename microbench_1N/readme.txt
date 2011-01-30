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
  $ ./launch_inet_tcp.sh 
    Usage: ./launch_inet_tcp.sh <nb_consumers> <message_size_in_B> <duration_warmup_phase_sec> <duration_logging_phase_sec>


++++++++++++++++++++
+++++ Inet UDP +++++

The benchmark is the following one:
  $ ./launch_inet_udp.sh 
    Usage: ./launch_inet_udp.sh <nb_consumers> <message_size_in_B> <duration_warmup_phase_sec> <duration_logging_phase_sec>


+++++++++++++++++++++++++++++++++++
+++++ Inet UDP with multicast +++++

The benchmark is the following one:
  $ ./launch_inet_udp_multicast.sh 
    Usage: ./launch_inet_udp_multicast.sh <nb_consumers> <message_size_in_B> <duration_warmup_phase_sec> <duration_logging_phase_sec>


++++++++++++++++++++++++++++++
+++++ Unix domain socket +++++

%TODO%
%modify /proc/net/unix/dgram_* with a script which has ugo+s rights and is owned by root%

The benchmark is the following one:
  $ ./launch_unix.sh 
    Usage: ./launch_unix.sh <nb_consumers> <message_size_in_B> <duration_warmup_phase_sec> <duration_logging_phase_sec>


%TODO%
%other communication mechanisms%
