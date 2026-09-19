# The course makefile's rules, inlined so this directory is self
# contained.  deq/GNUmakefile includes this file for the same rules.
# prog and objs use ?= and += so a subdirectory can set them first.

prog?=balloc
objs+=$(addsuffix .o,$(basename $(wildcard *.c *.cc)))

defines+=-D_GNU_SOURCE
ccflags+=-g -Wall -MMD $(defines)
ldflags+=-g

.SUFFIXES:

%.o: %.c  ; gcc -o $@ -c $< $(ccflags)
%.i: %.c  ; gcc -o $@ -E $< $(defines)
%.s: %.c  ; gcc -o $@ -S $< $(defines)
%.o: %.cc ; g++ -o $@ -c $< $(ccflags)
%.i: %.cc ; g++ -o $@ -E $< $(defines)
%.s: %.cc ; g++ -o $@ -S $< $(defines)

ld?=gcc

$(prog): $(objs) ; $(ld) -o $@ $^ $(ldflags)

.PHONY: clean run valgrind

clean:: ; rm -f $(prog) *.o *.d *.i try

run:      $(prog) ; ./$< $(args)
valgrind: $(prog) ; $@ --leak-check=full --show-leak-kinds=all ./$< $(args)

sinclude *.d
