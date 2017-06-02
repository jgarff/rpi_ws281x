all: libws2811.a test

libws2811.a: rpihw.c dma.c pwm.c ws2811.c mailbox.c
	gcc -c rpihw.c dma.c pwm.c ws2811.c mailbox.c
	ar rcs libws2811.a rpihw.o dma.o pwm.o ws2811.o mailbox.o

test: libws2811.a
	gcc main.c -L. -lws2811 -o test

clean:
	rm test libws2811.a
