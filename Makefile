main: main.c
	gcc `pkg-config --cflags gtk4` main.c -o main `pkg-config --libs gtk4`