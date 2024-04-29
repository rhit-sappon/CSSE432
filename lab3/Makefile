# Bryce Bejlovec
# To compile all three programs, list all executable names here.
CC = gcc
CFLAGS =
all: server proxy_parse

server: server.c
	$(CC) $(CFLAGS) -o proxy_parse.o -c proxy_parse.c
	$(CC) $(CFLAGS) -o server.o -c server.c
	$(CC) $(CFLAGS) -o server proxy_parse.o server.o

# Delete object files and executatbles to start afresh.
# Be sure to delete the "palindrome" executable also.
# Delete these files BEFORE committing your work. 
clean:
	rm -f *.o server proxy_parse

