# (C) 2017 by folkert van heusden, this file is released in the public domain
INSTALL_PREFIX=/usr/local

CFLAGS=-fPIC -Wall -pedantic -ggdb3 `pkg-config --cflags freetype2`
CXXFLAGS=-fPIC -Wall -pedantic -ggdb3 -std=c++17 `pkg-config --cflags freetype2`
VERSION=0.1

OBJS1=demo.o
OBJS2=vnc.o
OBJS3=mqtt-images.o

all: lib1 vnc mqtt-images

lib1: $(OBJS1)
	ar -r demo.a $(OBJS1)
	ld -g -x -lm -shared -soname demo.so.2 -o demo.so.$(VERSION) --whole-archive demo.a

vnc: $(OBJS2)
	ar -r vnc.a $(OBJS2)
	ld -g -x -lm -shared -soname vnc.so.2 -o vnc.so.$(VERSION) --whole-archive vnc.a

mqtt-images: $(OBJS3)
	ar -r mqtt-images.a $(OBJS3)
	ld -g -x -lm `pkg-config --libs libmosquitto` -shared -soname mqtt-images.so.2 -o mqtt-images.so.$(VERSION) --whole-archive mqtt-images.a

install: all
	cp demo.so.* vnc.so.* mqtt-images.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/demo.so.* $(INSTALL_PREFIX)/lib/vnc.so.* $(INSTALL_PREFIX)/lib/mqtt-images.so.*

clean:
	rm -f $(OBJS1) demo.a demo.so.* core $(OBJS2) vnc.a vnc.so.* mqtt-images.a mqtt-images.so.* $(OBJS3)
