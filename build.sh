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


mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$SHADROOT           \
    -DCMAKE_BUILD_TYPE=Debug                      \
    -DSHAD_RUNTIME_SYSTEM=GMT                       \
    -DGMT_ROOT=$GMTROOT                             \
    -DGTEST_ROOT=$GTESTROOT                         \
    -DGPERFTOOLS_ROOT=$GPERFTOOLSROOT

    
make -j 8 && make install