Grosu Gheorghe
324 CD

SERVER
--------------

The entire homework is created from a set of functions that implement a client-server communication protocol using TCP and UDP sockets. 

The server can handle multiple clients concurrently, and clients can subscribe to topics and receive messages published by other clients.
The clients can send messages over UDP, and the server forwards those messages to the subscribed TCP clients based on the topics they are interested in. 
The server keeps track of the connected clients, the topics they have subscribed to, and the messages they have sent. It provides a user interface to exit the server.

It has three main maps to keep track of the clients:

map<string, vector<string>> topics - a map of topics (strings) to the clients subscribed to them (represented by a vector of strings).
map<string, ClientData*> clients - a map of client IDs (strings) to their ClientData struct (which contains their socket, subscription topics, and active status).
map<int, string> socket_ID - a map of sockets (represented by their file descriptors) to their client IDs (strings).

The program uses poll to listen for incoming messages from both TCP and UDP sockets. 
When a new message arrives, the program parses it, handles it based on its type and content, and sends an appropriate response back to the client.
The program also has some helper functions to add a socket to the polling set, receive a UDP message, and handle a new connection request from a TCP client.

'ClientData structure'
---------------------
Defines a struct that holds information about the clients, including:

socket: the file descriptor of the TCP socket connected to the client
active: a flag indicating if the client is currently connected
topics: a vector of strings containing the topics the client has subscribed to
ID: a character array containing the ID of the client (a string with up to 9 characters)

handleUserInput
--------------------
The function handleUserInput reads input from the user via scanf, checks if the input is the string "exit" and if so, 
closes the TCP and UDP sockets passed as arguments and returns 1. 
If the input is not "exit", it calls the DIE function with an error message and returns 0.

add_to_polling_set function
---------------------------
This function adds a socket to the polling set. It takes as input parameters the socket to be added, the polling set (struct pollfd*), and the number of sockets in the polling set (int*).

receive_UDP_message function
--------------------------------
The function takes a single argument, the file descriptor of the UDP socket to receive the message from. 
It initializes a buffer with the same size as the UDP message format and sets its values to zero using memset. It then initializes a struct sockaddr_in object to hold the address of the client that sent the message, and sets the length of the client address using socklen_t.

The function then calls recvfrom, which receives a UDP message from the specified socket and stores it in the buffer. It also stores the address of the client that sent the message in the sockaddr_in object cli_addr. If recvfrom returns a negative value, the function returns nullopt to indicate an error occurred during the receiving process.

If recvfrom is successful, the function casts the buffer to a pointer of type UDP_FORMAT, and returns an optional object containing the dereferenced value of this pointer.

handle_received_UDP function
--------------------------------
This function handles a received UDP message by creating a corresponding TCP message and sending it to clients subscribed to the message's topic.

The function first calls the receive_UDP_message() function to receive the UDP message from the given UDP socket. If an error occurs during the reception of the message, the function calls the DIE() macro with an error message and exits.

Next, the function creates a TCP_message struct to represent the corresponding TCP message for the received UDP message. The TCP_message struct contains the source IP address and port of the UDP message sender, and the message payload.

The function then checks if any client has subscribed to the received message topic by looking up the topic in the given topics map. If no client is found, the function returns.

If clients are found for the received message topic, the function iterates over each client and checks if they are active. If a client is active, the function sends the corresponding TCP message to that client using the send() function. If a client is not active or is not found in the clients map, the function continues to the next client.

handle_client_reconnect function
-----------------------------------
This function handles a reconnection request from a client and adds it to the polling set. 

handle_new_TCP_client: 
-------------------------
Handles a new TCP client connection request. If the client is new, it creates a new client object, 
adds it to the clients map, and adds its socket to the polling set. 
If the client is already registered, it calls handle_client_reconnect function.

handle_client_reconnect: 
------------------------
Handles the case when a client tries to connect with another client's ID. 
It closes the socket and prints a message.

subscribe_to_topic:
-------------------
Handles the subscribe command received from a client. It extracts the topic and SF option, and adds the client to the corresponding topic's subscribers list. If the topic does not exist, it creates it.

remove_subscriber_from_topic: 
-----------------------------
Removes a client from a topic's subscribers list. It searches for the topic by name and removes the client from the corresponding subscribers vector.

SUBSCRIBER
--------------
This program implements a subscriber for a message broker service.
The subscriber connects to the server via a TCP connection, 
sends commands to subscribe or unsubscribe to a topic, 
and receives messages from the broker when they are published to the subscribed topics.
The purpose of the program is to connect to a server using TCP/IP and to subscribe or unsubscribe to a topic. 
The program then receives messages from the server and prints them to the console. 

Here is an explanation of the functions used in this program:

handle_stdin(int sockfd):
--------------------------- 
reads input from the standard input and sends the command to the broker server via the sockfd socket. 
It handles two types of commands: subscribe and unsubscribe, 
and exits the program if the command is exit.

print_INT_TYPE(char *payload): 
-------------------------------------
This function is used to print messages of type INT. 
The payload is a character array that includes an integer value represented in a specific format. 
The function first prints the type of the message as "INT" and then checks the first byte of the payload to determine if the integer is negative or positive. 
If the first byte is equal to 1, the integer is negative, and the function prints a "-" sign before the absolute value of the integer.
The absolute value of the integer is obtained by converting the next 4 bytes of the payload to an unsigned integer using network byte order (ntohl) and printing the result using printf with %d format.

print_SHORT_REAL_TYPE(char *payload): 
--------------------------------------
This function is used to print messages of type SHORT_REAL.
The payload is a character array that includes a short real value represented in a specific format. 
The function first prints the type of the message as "SHORT_REAL" and then converts the first two bytes of the payload to an unsigned integer using host byte order (ntohs).
The obtained integer represents the absolute value of the short real value multiplied by 100. 
The function then prints the obtained value as a floating-point number with two decimal places.

print_FLOAT_TYPE: 
---------------------
This function takes a payload of type char* as input and prints a message of type FLOAT. 
It first checks the sign bit of the payload and prints "-" if it is 1. 
Then it converts the payload to a uint32_t type, applies network byte order to it using ntohl(), and stores the result in nr_host. 
It then extracts the exponent from the payload, calculates 10 raised to the power of exponent using the pow() function, and stores the result in power10. 
Finally, it prints nr_host / power10 using printf().

receive_message:
-------------------
This function takes a socket file descriptor and a pointer to a TCP_message struct as input.
It receives a message from the socket and stores it in a buffer.
It returns -1 if no message is received and returns 0 otherwise. 
If a message is received, it copies the contents of the buffer to the TCP_message struct pointed to by message.

print_received_payload: 
-------------------------
This function takes a socket file descriptor as input. 
It first calls receive_message to receive a message from the server. 
If no message is received, it returns -1. 
Otherwise, it prints the source IP address, source port, and topic of the message. 
It then checks the type of the message and calls the appropriate print function (print_INT_TYPE, print_SHORT_REAL_TYPE, print_FLOAT_TYPE, or std::cout << "STRING - " << message.message.payload << std::endl;) to print the payload.

Main
---------
Main function first checks if enough arguments have been provided, then creates a socket and connects to a server using the provided arguments. It then sends an ID to the server using the created socket.

The function then sets up two file descriptors, one for the socket and one for standard input, and enters a loop using the poll function to wait for incoming messages. 
If a message is received from the server, it is printed using the print_received_payload function. If a message is received from standard input, it is handled using the handle_stdin function.

The loop continues until an error occurs or the connection is closed, at which point the socket is closed and the function returns 0.