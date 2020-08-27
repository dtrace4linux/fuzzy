#CC=gcc -g -O2
CC=clang -fsanitize=address -g -O2
fuzzy: fuzzy.c
	$(CC) -DCR_LINUX_X86_64 -I$(HOME)/crisp/include -I$(HOME)/crisp/foxlib -o fuzzy fuzzy.c \
	  $(HOME)/crisp/bin/foxlib.a -ldl
