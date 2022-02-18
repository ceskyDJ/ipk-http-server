.PHONY: all pack

all: hinfosvc

hinfosvc: hinfosvc.c hinfosvc.h
	gcc -g -pedantic -Wall -Wextra $< -o $@

pack:
	mkdir tmp
	cp *.c *.h Makefile ../Readme.md tmp
	cd tmp && tar -zcvf ../../xsmahe01.tar.gz *
	rm -rf tmp
