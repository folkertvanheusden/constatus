# (C) 2017 by folkert van heusden, this file is released in the public domain
INSTALL_PREFIX=/usr/local

CFLAGS=-fPIC -Wall -pedantic -ggdb3
CXXFLAGS=-fPIC -Wall -pedantic -ggdb3 -std=c++11
VERSION=0.1

OBJS1=demo.o
OBJS2=vnc.o

all: lib1 vnc

lib1: $(OBJS1)
	ar -r demo.a $(OBJS1)
	ld -g -x -lm -shared -soname demo.so.2 -o demo.so.$(VERSION) --whole-archive demo.a

vnc: $(OBJS2)
	ar -r vnc.a $(OBJS2)
	ld -g -x -lm -shared -soname vnc.so.2 -o vnc.so.$(VERSION) --whole-archive vnc.a

install: all
	cp demo.so.* vnc.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/demo.so.* $(INSTALL_PREFIX)/lib/vnc.so.*

clean:
	rm -f $(OBJS1) demo.a demo.so.* core $(OBJS2) vnc.a vnc.so.*
