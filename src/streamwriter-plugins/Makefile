# (C) 2017 by folkert van heusden, this file is released in the public domain
INSTALL_PREFIX=/usr/local

CFLAGS=-fPIC -Wall -pedantic -ggdb3
CXXFLAGS=-fPIC -Wall -pedantic -ggdb3
VERSION=0.1

OBJS1=demo.o

all: lib1

lib1: $(OBJS1)
	ar -r demo.a $(OBJS1)
	ld -g -x -lm -shared -soname demo.so.2 -o demo.so.$(VERSION) --whole-archive demo.a

install: all
	cp demo.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/demo.so.*

clean:
	rm -f $(OBJS1) demo.a demo.so.* core
