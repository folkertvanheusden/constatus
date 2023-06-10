# (C) 2017-2023 by Folkert van Heusden, released under MIT license

NAME="constatus"
PREFIX=/usr
VERSION="5.0"

constatus:
	mkdir -p build && cd build && cmake .. && make

install: constatus
	install -Dm755 constatus ${DESTDIR}${PREFIX}/bin/constatus
	install -Dm755 motion-to-constatus.py ${DESTDIR}${PREFIX}/share/doc/constatus/motion-to-constatus.py
	mkdir -p ${DESTDIR}${PREFIX}/share/doc/constatus
	install -Dm644 constatus.cfg ${DESTDIR}${PREFIX}/share/doc/constatus/example.cfg
	install -Dm644 man/constatus.1 ${DESTDIR}${PREFIX}/share/man/man1/constatus.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/constatus

clean:
	rm -rf build

package: clean
	mkdir constatus-$(VERSION)
	cp -a man/ src examples README.md constatus.cfg debian favicon.ico stylesheet.css LICENSE CMakeLists.txt config.h.in motion-to-constatus.py README.rest constatus-$(VERSION)
	tar czf ../constatus_$(VERSION).orig.tar.gz constatus-$(VERSION)
	rm -rf constatus-$(VERSION)
