LIKWID_LIB = -L../ -llikwid 
INCLUDES = -I../src/includes -I../ -I../GCC

all:  test-marker 

fortran: chaos.F90
	ifort $(INCLUDES)  -O3  -o $@ chaos.F90 $(LIKWID_LIB) -lpthread

stream-gcc:
	gcc -O3 -fopenmp -o $@  stream.c


stream-icc:
	icc -O3 -openmp -o $@  stream.c

dumper:
	gcc -O1 -g $(INCLUDES) -pthread  -o $@ cpuid_dump.c $(LIKWID_LIB)

test:
	icc -O3 -o $@ dofp.c 

test-marker:
	gcc -O1 -g $(INCLUDES) -pthread -DPERFMON  -o $@ dofp.c $(LIKWID_LIB)

test-msr:
	gcc -O2 -o $@ msrClient.c 

test-mpi:
	mpicc -O2 -openmp -I../src/includes  -I../GCC -D_GNU_SOURCE  -o $@ MPI_pin_test.c $(LIKWID_LIB)

.PHONY: clean

clean: 
	rm -f stream-* test test-marker test-msr test-mpi fortran


