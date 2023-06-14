CFLAGS = -Wall -g 

# Port to connect to
PORT_SERVER = 54321

# IP address to connect to (localhost)
IP_SERVER = 127.0.0.1

ID_CLIENT = 1

all: server subscriber

server: server.cpp
	g++ $(CFLAGS) -o server server.cpp

subscriber: subscriber.cpp
	g++ $(CFLAGS) -o subscriber subscriber.cpp

clean:
	rm -f server subscriber

run_server: server
	./server $(PORT_SERVER)

run_subscriber: subscriber
	./subscriber $(ID_CLIENT) $(IP_SERVER) $(PORT_SERVER)
