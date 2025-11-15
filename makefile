build:
	rm -f ./a.out
	gcc -std=c11 -g -Wall -pedantic -Werror main.c execute.c ram.o nupython.o -lm -Wno-psabi -Wno-unused-variable -Wno-unused-function 

run:
	./a.out

valgrind:
	rm -f ./a.out
	gcc -std=c11 -g -Wall -pedantic -Werror main.c execute.c ram.o nupython.o -lm -Wno-psabi -Wno-unused-variable -Wno-unused-function
	valgrind --tool=memcheck --leak-check=no --track-origins=yes ./a.out "$(file)"

submit:
	/gradescope/gs submit 1130317 7167063 main.c execute.c

commit:
	git add .
	git commit -m "$(msg)"

push:
	git push origin main

