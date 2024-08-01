main: main.c
	gcc `pkg-config --cflags gtk4` main.c -o main `pkg-config --libs gtk4`
	./main

test: TestCarousel.c
	gcc `pkg-config --cflags gtk4` TestCarousel.c -o testca `pkg-config --libs gtk4`
	./testca