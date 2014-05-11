
CC_FLAG= -g -std=c99

smtp:main.o smtp.o base64.o
	gcc ${CC_FLAG} -o smtp main.o smtp.o base64.o

main.o:main.c
	gcc ${CC_FLAG} -c main.c -o main.o

smtp.o:smtp.h smtp.c
	gcc ${CC_FLAG} -c smtp.c -o smtp.o

base64.o:lib/base64.h lib/base64.c
	gcc ${CC_FLAG} -c lib/base64.c -o base64.o
.PHONY:clean
clean: 
	rm -f *.o smtp