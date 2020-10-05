pzip:
	gcc -o pzip.bin pzip.c -Wall -Werror -pthread -O3 --std=c99
clean:
	rm *.bin
