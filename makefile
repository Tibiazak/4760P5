all: oss user

oss: oss.o
	gcc -Wall -lpthread -lrt -o oss oss.o

oss.o: oss.c
	gcc -Wall -c -lpthread -lrt oss.c

user: user.o
	gcc -Wall -lpthread -lrt -o user user.o

user.o: user.c
	gcc -Wall -lpthread -lrt -c user.c

clean:
	rm *.o user oss
