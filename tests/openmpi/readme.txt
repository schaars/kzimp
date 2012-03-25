load knem:
# modprobe knem

get stats about knem:
$ cat /dev/knem

disable knem at runtime:
$ mpirun --mca btl_sm_use_knem 0   <program> <args>

set min size (in bytes) to use knem:
$ mpirun --mca btl_sm_eager_limit 32768   <program> <args>

use shared memory
$ mpirun --mca btl sm   <program> <args>
