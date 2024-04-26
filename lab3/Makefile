# Bryce Bejlovec
# To compile all three programs, list all executable names here.
CC = gcc
CFLAGS = -pthread
all: client/client server/server

%: %.o
	$(CC) $(CFLAGS) $< -o $@

%.o: %.c
	$(CC) $< -c $(CFLAGS) -o $@

# Delete object files and executatbles to start afresh.
# Be sure to delete the "palindrome" executable also.
# Delete these files BEFORE committing your work. 
clean:
	rm -f *.o client/client server/server
