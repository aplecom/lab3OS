lab3: main.o
	gcc main.c -o lab3

clean:
	rm -rf *.o
	rm -rf lab3
	rm -rf check.txt