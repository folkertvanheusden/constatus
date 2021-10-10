#! /usr/bin/python3

# this code uses the 'lcdproc-python3' library:
# https://github.com/jinglemansweep/lcdproc

# for more details about lcdproc, see:
# http://lcdproc.org/

from lcdproc.server import Server
import time
import urllib.request

def main():
    # 'localhost' must be replaced by the network address on which the
    # constatus server runs
    lcd = Server('localhost', debug=True)

    lcd.start_session()

    # you can have multiple screens. constatus will switch between them
    # every 4 seconds
    screen1 = lcd.add_screen('Screen1')

    # a screen can have multiple widgets
    screen1.add_string_widget('strwidget1', text='Bitcoin rate:', x=1, y=1)  # widget1
    widget2 = screen1.add_string_widget('strwidget2', text='', x=1, y=2)

    while True:
        # retrieve bitcoin rate (don't depend on this: this service is often behind
        # for weeks)
        h = urllib.request.urlopen('https://vps001.vanheusden.com/btc/latest.txt')

        widget2.set_text(h.read().decode('utf-8'))

        time.sleep(30)

if __name__ == '__main__':
    main()
