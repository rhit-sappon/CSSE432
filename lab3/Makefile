# Bryce Bejlovec
# To compile all three programs, list all executable names here.
CC = gcc
CFLAGS = -pthread
OBJECTS = proxy.o proxy_parse.o

all: proxy proxy_cache

proxy: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o proxy

proxy.o: proxy.c
	$(CC) $(CFLAGS) -c proxy.c

proxy_parse.o: proxy_parse.c proxy_parse.h
	$(CC) $(CFLAGS) -c proxy_parse.c

proxy_cache: proxy_cache.o proxy_parse.o
	$(CC) $(CFLAGS) proxy_cache.o proxy_parse.o -o proxy_cache

proxy_cache.o: proxy_cache.c
	$(CC) $(CFLAGS) -c proxy_cache.c



# Delete object files and executatbles to start afresh.
# Be sure to delete the "palindrome" executable also.
# Delete these files BEFORE committing your work. 
clean:
	rm -f *~ *.o proxy 

