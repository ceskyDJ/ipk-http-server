# Makefile
#
# Author: Michal Šmahel (xsmahe01)
#
# Usage:
# make          ... build main binary
# make pack     ... create final archive
# make clean    ... remove temporary files
# make cleanall ... remove all generated files

# Used magic variables:
# $< ... first dependency
# $^ ... all dependencies
# $@ ... target

PROGRAM=hinfosvc
ARCHIVE=xsmahe01.tar.gz
MODULES=$(PROGRAM).o system-info.o http-processing.o

CC=gcc
CFLAGS=-std=gnu11 -Wall -Wextra -pedantic -g

# Get a list of source files derived from MODULES
SOURCES=$(patsubst %.o, %.c, $(MODULES))

.PHONY: all pack

all: $(PROGRAM)

# Recipe for compiling modules
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Linking all together
$(PROGRAM): $(MODULES)
	$(CC) $(CFLAGS) $^ -o $@

#######################################
# Module dependencies
dep.list: $(SOURCES)
	$(CC) -MM *.c >dep.list

-include dep.list
#######################################

pack:
	mkdir tmp
	cp *.c *.h Makefile ../Readme.md tmp
	cd tmp && tar -zcvf ../../$(ARCHIVE) *
	rm -rf tmp

clean:
	rm -f *.o

cleanall: clean
	rm -f dep.list $(PROGRAM) ../$(ARCHIVE)
