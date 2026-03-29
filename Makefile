build:
	gcc server.c -o server.o
run:
	./server.o
reload:
	gcc server.c -o server.o
	./server.o
