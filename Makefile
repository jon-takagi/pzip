pzip:
	gcc -o pzip.bin pzip.c -Wall -Werror -pthread -O3
clean:
	rm *.bin
