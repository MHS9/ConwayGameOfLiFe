CC=cc
CFLAGS=-g
LIBS = -lpthread

all: life lifeMT

life: life.o
	$(CC) -o $@ $^ $(LIBS)

lifeMT: lifeMT.o
	$(CC) -o $@ $^ $(LIBS)

%.o : %.c
	$(CC) $(CFLAGS) -c $< 

clean:
	rm -rf *.o life lifeMT

