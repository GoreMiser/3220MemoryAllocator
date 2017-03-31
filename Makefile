CC=clang
CFLAGS=-Wall -g

BINS=libmyalloc.so


all: $(BINS)

libmyalloc.so:  allocator.c 
	$(CC) $(CFLAGS) -O2 -DNDEBUG -fPIC -shared -o libmyalloc.so allocator.c -ldl

clean:
	rm $(BINS) 

