#!/bin/bash
set -e # STOP ON ERROR

SHADROOT=$HOME/SHAD
GMTROOT=$HOME/gmt
GTESTROOT=$HOME/googletest
GPERFTOOLSROOT=$HOME/gperftools
ROCMROOT=/opt/rocm-5.6.0


mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$SHADROOT           \
    -DCMAKE_BUILD_TYPE=Release                      \
    -DSHAD_RUNTIME_SYSTEM=GMT                       \
    -DGMT_ROOT=$GMTROOT                             \
    -DGTEST_ROOT=$GTESTROOT                         \
    -DROCM_ROOT=$ROCMROOT                           \
    -DGPERFTOOLS_ROOT=$GPERFTOOLSROOT

    
make -j 8 && make install