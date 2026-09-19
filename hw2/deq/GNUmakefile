# The HW1 deque and its test suite, unchanged, linked with wrapper.c,
# which routes malloc(), free() and realloc() to the Buddy allocator in
# the parent directory.  wrapper.c needs gcc: it nests a function.
prog=deq

objs=$(addprefix ../,utils.o bm.o bbm.o freelist.o balloc.o)
ccflags=-I..

include ../../GNUmakefile
