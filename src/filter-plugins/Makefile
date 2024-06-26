# (C) 2017-2024 by folkert van heusden, this file is released in the public domain
INSTALL_PREFIX=/usr/local

CFLAGS=-fPIC -Wall -pedantic -ggdb3 `pkg-config --cflags opencv libmosquitto cairo`
CXXFLAGS=-fPIC -Wall -pedantic -ggdb3 `pkg-config --cflags opencv libmosquitto cairo` -std=c++17
LDFLAGS=-fPIC `pkg-config --cflags opencv libmosquitto cairo`
VERSION=0.2

OBJSKA=keep-alive.o cairo.o
OBJS1=demo.o
OBJS2=demo2.o
OBJS3=demoocv.o
OBJS4=demoocv2.o
OBJS5=mqtt_text_scroll.o cairo.o
OBJS6=mqtt_trigg_overlap.o

all: keep-alive lib1 lib2 lib3 lib4 lib5 lib6

keep-alive: $(OBJSKA)
	ar -r keep-alive.a $(OBJSKA)
	ld -g -x -lm -shared -soname keep-alive.so.2 -o keep-alive.so.$(VERSION) --whole-archive keep-alive.a $(LDFLAGS)

lib1: $(OBJS1)
	ar -r demo.a $(OBJS1)
	ld -g -x -lm -shared -soname demo.so.2 -o demo.so.$(VERSION) --whole-archive demo.a

lib2: $(OBJS2)
	ar -r demo2.a $(OBJS2)
	ld -g -x -lm -shared -soname demo2.so.2 -o demo2.so.$(VERSION) --whole-archive demo2.a

lib3: $(OBJS3)
	ar -r demoocv.a $(OBJS3)
	ld -g -x -lm `pkg-config --libs opencv` -shared -soname demoocv.so.2 -o demoocv.so.$(VERSION) --whole-archive demoocv.a

lib4: $(OBJS4)
	ar -r demoocv2.a $(OBJS4)
	ld -g -x -lm `pkg-config --libs opencv` -shared -soname demoocv2.so.2 -o demoocv2.so.$(VERSION) --whole-archive demoocv2.a 

lib5: $(OBJS5)
	ar -r mqtt_text_scroll.a $(OBJS5)
	ld -g -x -lm `pkg-config --libs libmosquitto` -shared -soname mqtt.so.2 -o mqtt_text_scroll.so.$(VERSION) --whole-archive mqtt_text_scroll.a 

lib6: $(OBJS6)
	ar -r mqtt_trigg_overlap.a $(OBJS6)
	ld -g -x -lm `pkg-config --libs libmosquitto` -shared -soname mqtt_trigg_overlap.so.2 -o mqtt_trigg_overlap.so.$(VERSION) --whole-archive mqtt_trigg_overlap.a 

install: all
	cp demo.so.* demo2.so.* demoocv.so.* mqtt_text_scroll.so.* mqtt_trigg_overlap.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/demo.so.* $(INSTALL_PREFIX)/lib/demo2.so.* $(INSTALL_PREFIX)/lib/demoocv.so.* $(INSTALL_PREFIX)/lib/mqtt_text_scroll.so.* $(INSTALL_PREFIX)/lib/mqtt_trigg_overlap.so.*

clean:
	rm -f $(OBJS1) $(OBJS2) demo.a demo.so.* core demo2.a demo2.so.* $(OBJS3) demoocv.so.* demoocv.a $(OBJS4) mqtt_text_scroll.so.* mqtt_text_scroll.a $(OBJS5) mqtt_trigg_overlap.so.* mqtt_trigg_overlap.a $(OBJS6) keep-alive.so.* keep-alive.a $(OBJSKA)
