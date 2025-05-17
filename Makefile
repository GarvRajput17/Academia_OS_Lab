CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS = -pthread

SRCS = server.c client.c admin.c student.c faculty.c syscalls.c socket_utils.c session_utils.c
OBJS = $(SRCS:.c=.o)
SERVER = server
CLIENT = client

all: $(SERVER) $(CLIENT)

$(SERVER): server.o syscalls.o socket_utils.o session_utils.o
	$(CC) $(LDFLAGS) -DSERVER_BUILD -o $@ $^

$(CLIENT): client.o admin.o student.o faculty.o syscalls.o socket_utils.o session_utils.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(SERVER) $(CLIENT) 