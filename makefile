all: ping new_ping watchdog

ping: ping.o
	gcc -Wall -g -o ping ping.o

ping.o: ping.c
	gcc -Wall -g -c ping.c

new_ping: new_ping.o
	gcc -Wall -g -o new_ping new_ping.o

new_ping.o: new_ping.c
	gcc -Wall -g -c new_ping.c

watchdog: watchdog.o
	gcc -Wall -g -o watchdog watchdog.o

watchdog.o: watchdog.c
	gcc -Wall -g -c watchdog.c


clean:
	rm -f *.o ping new_ping watchdog