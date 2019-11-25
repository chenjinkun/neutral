ARCH=$1

if [ $ARCH == "kp920" ]
then
    make KERNELS=omp3 COMPILER=GCCKP920 MPI=no ARCH_COMPILER_CC=gcc verbose=1
fi

mv neutral.omp3 neutral.omp3.$ARCH