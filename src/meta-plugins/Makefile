# (C) 2017 by folkert van heusden, this file is released in the public domain
INSTALL_PREFIX=/usr/local

LDFLAGS=`pkg-config --libs libgps`
CXXFLAGS=-fPIC -Wall -pedantic -ggdb3 `pkg-config --cflags libgps` -std=c++11
VERSION=0.1

OBJS1=gps.o

all: gps

gps: $(OBJS1)
	ar -r gps.a $(OBJS1)
	ld -g -x -lm $(LDFLAGS) -shared -soname gps.so.1 -o gps.so.$(VERSION) --whole-archive gps.a

install: all
	cp gps.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/gps.so.*

clean:
	rm -f $(OBJS1) gps.a gps.so.* core
