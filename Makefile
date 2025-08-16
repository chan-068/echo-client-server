CC = g++
CFLAGS = -Wall -O2

TARGETS = echo-server echo-client
OBJS_SERVER = echo-server.o
OBJS_CLIENT = echo-client.o

all: $(TARGETS)

echo-server: $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o $@ $^

echo-client: $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) *.o
