INCLUDE=-Ich
WARNINGS=-Wall -Wno-return-type -Wno-implicit-int 
CFLAGS=-std=c11 $(WARNINGS) -g $(INCLUDE)
LIBS=`echo $(LD_LIBRARY_PATH) | sed "s/^/-L/" | sed "s/:/ -L/g" | sed 's/-L$$//'` -lreadline -lhistory -lm

all: aifaleene

aifaleene: main.o
	$(CC) main.o -o aifaleene $(LIBS)

main.o: main.c ch/hash.h ch/list.h ch/ibuf.h ch/buf.h ch/misc.h aifaleene.h 
	$(CC) $(CFLAGS) -c main.c -o main.o

clean:
	rm aifaleene *.o

.Phony: all clean
