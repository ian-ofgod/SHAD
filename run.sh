#!/bin/bash
set -e # STOP ON ERROR

cd build
salloc --cpu-freq=Performance -N 1 -p junction mpirun -np 1 -npernode 1 --bind-to socket ./examples/hwloc_amd/amd_info --gmt_num_workers 1 --gmt_num_helpers 1 --gmt_comm_buffer_size 4194304