all: oss user

oss: oss.o
	gcc -Wall -lrt -o oss oss.o

oss.o: oss.c
	gcc -Wall -c -lrt oss.c

user: user.o
	gcc -Wall -lrt -o user user.o

user.o: user.c
	gcc -Wall -lrt -c user.c

clean:
	rm *.o user oss
