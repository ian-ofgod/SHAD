#!/bin/bash
set -e # STOP ON ERROR

FOLDER=$HOME/HWLOC_with_GMT

SHADROOT=$FOLDER/SHAD
GMTROOT=$FOLDER/gmt
GTESTROOT=$FOLDER/googletest
GPERFTOOLSROOT=$FOLDER/gperftools

module unload openmpi/4.1.2
module unload rocm/5.6.0
module unload cmake/3.27.7
module unload gcc/8.5.0
module unload python/3.7.0

module load python/3.7.0
module load cmake/3.27.7
module load gcc/8.5.0
module load openmpi/4.1.2

cd build
salloc -N 2 -p junction mpirun -np 2 -npernode 1 --bind-to socket ./examples/hwloc_simple/hwloc_simple --gmt_thread_pinning true --gmt_comm_buffer_size 4194304
#salloc -N 2 -p junction mpirun -np 2 -npernode 1 --bind-to socket ./examples/hwloc_simple/hwloc_simple --gmt_thread_pinning true  --gmt_num_workers 8 --gmt_num_helpers 1 --gmt_comm_buffer_size 4194304