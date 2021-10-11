# (C) 2017-2020 by folkert van heusden, this file is released in the public domain
INSTALL_PREFIX=/usr/local

CXXFLAGS=-fPIC -Wall -ggdb3 `pkg-config --cflags opencv4` -std=c++17 -I../build
VERSION=0.1

OBJS1=demo.o
OBJS2=demoocv.o
OBJS3=demoocv2.o
OBJS4=hue-only.o
OBJS5=gpio.o
OBJS6=network.o
OBJS7=objects-test.o

all: lib1 lib2 lib3 lib4 lib5 lib6 lib7

lib1: $(OBJS1)
	ar -r demo.a $(OBJS1)
	ld -g -x -lm -shared -soname demo.so.2 -o demo.so.$(VERSION) --whole-archive demo.a

lib2: $(OBJS2)
	ar -r demoocv.a $(OBJS2)
	ld -g -x -lm `pkg-config --libs opencv4` -shared -soname demoocv.so.2 -o demoocv.so.$(VERSION) --whole-archive demoocv.a

lib3: $(OBJS3)
	ar -r demoocv2.a $(OBJS3)
	ld -g -x -lm `pkg-config --libs opencv4` -shared -soname demoocv2.so.2 -o demoocv2.so.$(VERSION) --whole-archive demoocv2.a 

lib4: $(OBJS4)
	ar -r hue-only.a $(OBJS4)
	ld -g -x -lm -shared -soname hue-only.so.2 -o hue-only.so.$(VERSION) --whole-archive hue-only.a 

lib5: $(OBJS5)
	ar -r gpio.a $(OBJS5)
	ld -g -x -lm -shared -soname gpio.so.2 -o gpio.so.$(VERSION) --whole-archive gpio.a 

lib6: $(OBJS6)
	ar -r network.a $(OBJS6)
	ld -g -x -lm -shared -soname network.so.2 -o network.so.$(VERSION) --whole-archive network.a 

lib7: $(OBJS7)
	ar -r objects-test.a $(OBJS7)
	ld -g -x -lm -shared -soname objects-test.so.2 -o objects-test.so.$(VERSION) --whole-archive objects-test.a 

install: all
	cp demo*.so.* network*.so.* gpio*.so.* objects-test*.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/demo*.so.* $(INSTALL_PREFIX)/lib/gpio*.so.* $(INSTALL_PREFIX)/lib/network*.so.* $(INSTALL_PREFIX)/lib/objects-test*.so.*

clean:
	rm -f $(OBJS1) core $(OBJS2) $(OBJS3) *.a *.so.* $(OBJS5) $(OBJS6) $(OBJS7)
