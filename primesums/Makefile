GCC_VERSION=4.5.1
PREFIX=/opt/gcc/${GCC_VERSION}/bin
CC=${PREFIX}/gcc
CPP=${PREFIX}/g++
#LD_LIBRARY_PATH=/opt/intel/composerxe/tbb/lib/intel64/cc4.1.0_libc2.4_kernel2.6.16.21
LD_LIBRARY_PATH=/opt/intel/tbb/latest/lib/intel64/cc4.1.0_libc2.4_kernel2.6.16.21

primesums : primesums.cpp
	#g++ -g -pg -Wall -I ~/tbb30_174oss/include primesums.cpp -o primesums -ltbb -L/Users/cnauroth/tbb30_174oss/lib
	g++ -O3 -Wall -I ~/tbb30_174oss/include primesums.cpp -o primesums -ltbb -L/Users/cnauroth/tbb30_174oss/lib

clean :
	rm -f primesums
