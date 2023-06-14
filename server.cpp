#include <iostream>
#include <poll.h>
#include "utils.h"
#include <vector>
#include <map>
#include <bits/stdc++.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define MAX_CONNECTIONS 100

typedef struct ClientData
{
    int socket; 
    bool active;
    vector<string> topics;
    char ID[10];
} ClientData;

using namespace std;

/**
 * @brief Helper function to add a socket to the polling set.
 * 
 * @param socket The socket to be added.
 * @param fds The polling set.
 * @param nfds The number of sockets in the polling set.
 */
void add_to_polling_set(int socket, 
                        struct pollfd *fds, 
                        int *nfds)
{
    fds[*nfds].fd = socket;
    fds[*nfds].events = POLLIN;
    (*nfds)++;
}

/**
 * @brief Helper function to receive an UDP message.
 * 
 * @param sockfd The file descriptor of the UDP socket to receive the message from.
 * @return An optional that contains the received UDP message, or nullopt if an error occurs.
 */
std::optional<UDP_FORMAT> receive_UDP_message(int sockfd)
{
    char buffer[sizeof(UDP_FORMAT)];
    memset(buffer, 0, sizeof(UDP_FORMAT));
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr); // set the length of client address

    int rs = recvfrom(sockfd, buffer, sizeof(UDP_FORMAT), 0,
                      (struct sockaddr *)&cli_addr, &cli_len);
    if (rs < 0) {
        return std::nullopt;
    }

    UDP_FORMAT* message = (UDP_FORMAT*)buffer;
    return *message;
}

/**
 * @brief Handle the received UDP message.
 * 
 * @param UDP_socket The file descriptor of the UDP socket.
 * @param topics A map of topics and the clients subscribed to them.
 * @param clients Database of clients.
 */
void handle_received_UDP(const map<string, vector<string>>& topics,     
                        int UDP_socket, 
                        map<string, ClientData*>& clients) 
{
    auto udp_message = receive_UDP_message(UDP_socket);
    if (!udp_message) {
        DIE(1, "Error receiving UDP message");
    }

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    getpeername(UDP_socket, (struct sockaddr*)&cli_addr, &cli_len);

    TCP_message tcp_message;
    tcp_message.source_ip = cli_addr.sin_addr;
    tcp_message.source_port = cli_addr.sin_port;
    tcp_message.message = *udp_message;

    auto topic_it = topics.find(string(udp_message->topic));
    if (topic_it == topics.end()) {
        return;
    }

    for (const string& client_id : topic_it->second) {
        auto client_it = clients.find(client_id);
        if (client_it == clients.end()) {
            continue;
        }

        ClientData* client = client_it->second;

        if (client->active) {
            send(client->socket, (char*)&tcp_message, sizeof(tcp_message), 0);
        }
    }
}

/*
 * @brief Used to handle the user input and close the sockets if the user types "exit".
*/
int handleUserInput(int TCP_socket, 
                    int UDP_socket)
{
    char exit_command[5];
    memset(exit_command, 0, 5);
    scanf("%s", exit_command);

    if (strncmp(exit_command, "exit", 4) == 0)
    {
        close(TCP_socket);
        close(UDP_socket);
        return 1;
    }
    else
    {
        DIE(1, "Invalid command");
        return 0;
    }

    return -1;
}

void handle_client_reconnect(ClientData *client, 
                            int clientfd,   
                            char *ID,
                            struct sockaddr_in cli_addr,    
                            struct pollfd *fds,     
                            int *nfds,
                            map<int, string> &socket_ID)
{
    if (client->active)
    {
        close(clientfd);
        printf("Client %s already connected.\n", ID);
        return;
    }

    client->active = true;
    client->socket = clientfd;
    strcpy(client->ID, ID);

    printf("New client %s connected from %s:%d.\n", ID,
           inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

    socket_ID[clientfd] = string(ID);

    add_to_polling_set(clientfd, fds, nfds);
}

/**
 * @brief Handle a new connection request from a TCP client and add it to the polling set.
*/
void handle_new_TCP_client(map<string, ClientData *> &clients, 
                        int TCP_socket, struct pollfd *fds,     
                        int *nfds,
                        map<int, string> &socket_ID)
{
    int clientfd;
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    clientfd = accept(TCP_socket, (struct sockaddr *)&cli_addr, &cli_len);
    DIE(clientfd < 0, "accept");

    char ID[10];
    int n = recv(clientfd, ID, sizeof(ID), 0);
    DIE(n < 0, "recv");

    ID[n] = '\0';
    string ID_string = string(ID);

    auto ID_client = clients.find(string(ID));

    if (ID_client == clients.end())
    {
        printf("New client %s connected from %s:%d.\n", ID,
               inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        ClientData *client = (ClientData *)malloc(sizeof(ClientData));
        DIE(client == NULL, "malloc");

        client->active = true;
        client->socket = clientfd;
        strcpy(client->ID, ID);

        clients[ID_string] = client;
        socket_ID[clientfd] = ID_string;

        add_to_polling_set(clientfd, fds, nfds);
    }
    else
    {

        ClientData *client = ID_client->second;
        handle_client_reconnect(client, clientfd, ID, cli_addr, fds, nfds, socket_ID);
    }
}

/**
 * @brief Handles the exit command received from a client.
*/
void remove_client(int socket, map<int, string> &socket_ID, map<string, ClientData *> &clients,
                   struct pollfd *fds, int *nfds)
{
    char ID[10];
    memcpy(ID, socket_ID[socket].c_str(), 10);
    string ID_string = socket_ID[socket];

    socket_ID.erase(socket);
    fds->fd = -1;
    fds->events = 0;

    auto it = clients.find(ID_string);
    if (it != clients.end())
    {
        clients[ID_string]->active = false;
        printf("Client %s disconnected.\n", ID);
    }
}

/**
 * @brief Handles the subscribe command received from a client.
*/
void subscribe_to_topic(char *buf,  
                        map<string, vector<string>> &topics, 
                        string ID_string)
{
    // extract the topic and SF option
    char *topic = strtok(buf + 10, " ");
    char *sf_str = strtok(NULL, " ");
    int SF = atoi(sf_str);
    DIE(SF < 0, "atoi");

    string topic_string = string(topic);

    auto it = topics.find(topic_string);
    if (it == topics.end())
    {   
        vector<string> v = {ID_string};
        topics[topic_string] = v;
    }
    else
    {
        bool found_sub = false;
        for (string ID_client : topics[topic_string])
        {
            if (ID_client == ID_string)
            {
                found_sub = true;
                break;
            }
        }

        if (!found_sub)
        {
            topics[topic_string].push_back(ID_string);
        }
    }
}

/**
 * @brief Remove the client with the given ID from the subscribers of the topic with the given name.
 * 
 * @param topic_name The name of the topic.
 * @param ID_string The ID of the client to be removed.
 * @param topics The map of topics and their subscribers.
 */
void remove_subscriber_from_topic(const string& topic_name, 
                                  const string& ID_string,
                                  map<string, vector<string>>& topics) {
    // find the topic and remove the client from the vector of subscribers
    auto it = topics.find(topic_name);
    if (it != topics.end()) {
        auto& subscribers = it->second;
        auto itr = std::find(subscribers.begin(), subscribers.end(), ID_string);
        if (itr != subscribers.end()) {
            subscribers.erase(itr);
        }
    }
}

/**
 * @brief Unsubscribe the client with the given ID from the topic specified in the given message.
 * 
 * @param message The message containing the topic to unsubscribe from.
 * @param topics The map of topics and their subscribers.
 * @param ID_string The ID of the client to unsubscribe.
 */
void unsubscribe_from_topic(char* message, 
                            map<string, vector<string>>& topics, 
                            const string& ID_string) {
    char* topic = strtok(message + 12, " ");
    string topic_string = string(topic);

    remove_subscriber_from_topic(topic_string, ID_string, topics);
}

/**
 * @brief Receive a message from the subscriber
 * 
 * @param socket The socket to receive the message from.
 * @param socket_ID The map of sockets and their associated IDs.
 * @param clients The map of clients and their data.
 * @param fds The array of pollfd structs.
 * @param nfds The number of file descriptors in the array.
 * @param topics The map of topics and their subscribers.
 */

void receive_tcp_message(map<string, ClientData *> &clients,    
                        int socket, 
                         map<int, string> &socket_ID,
                         struct pollfd *fds, 
                         int *nfds, 
                         map<string, vector<string>> &topics)
{

    char buf[MAX_TCP_CMD_LEN];
    memset(buf, 0, MAX_TCP_CMD_LEN);

    int n = recv(socket, buf, MAX_TCP_CMD_LEN, 0);
    DIE(n < 0, "recv");

    char ID[10];
    memcpy(ID, socket_ID[socket].c_str(), 10);
    string ID_string = socket_ID[socket];

    if (n == 0)
    {
        remove_client(socket, socket_ID, clients, fds, nfds);
        return;
    }

    if (strncmp(buf, "subscribe", 9) == 0)
    {
        subscribe_to_topic(buf, topics, ID_string);
    }

    if (strncmp(buf, "unsubscribe", 11) == 0)
    {
        unsubscribe_from_topic(buf, topics, ID_string);
    }
}

int open_socket(char *port, 
                int type)
{
    int sockfd = socket(AF_INET, type, 0);

    if (sockfd < 0)
    {
        close(sockfd);
        DIE(sockfd < 0, "Error creating socket");
    }

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in servaddr;
    memset((char *)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(atoi(port));

    int bind_ret = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    if (bind_ret < 0)
    {
        close(sockfd);
        DIE(bind_ret < 0, "Error binding");
    }

    return sockfd;
}

void disable_nagle(int sockfd)
{
    int aux = 1;
    int deactivate = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&aux, sizeof(int));
    DIE(deactivate < 0, "Error deactivating Nagle's algorithm");
}

int open_TCP(char *port)
{
    int rsp = open_socket(port, SOCK_STREAM);

    disable_nagle(rsp);

    int listen_ret = listen(rsp, 1);

    DIE(listen_ret < 0, "Error listening");

    return rsp;
}

int open_UDP(char *port)
{
    return open_socket(port, SOCK_DGRAM);
}

void delete_clients(map<string, ClientData *> &clients)
{
    for (auto it = clients.begin(); it != clients.end(); ++it)
    {
        it->second->topics.clear();
        delete it->second;
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc < 2, "Not enough arguments");
    map<string, ClientData *> clients_Database = map<string, ClientData *>();
    map<int, string> active_Clients_ID = map<int, string>();

    struct pollfd fds[MAX_CONNECTIONS];
    int nfds = 3; 

    int udp_fd = open_UDP(argv[1]);
    DIE(udp_fd < 0, "socket");
    fds[0].fd = udp_fd;
    fds[0].events = POLLIN;

    int tcp_fd = open_TCP(argv[1]);
    DIE(tcp_fd < 0, "socket");
    fds[1].fd = tcp_fd;
    fds[1].events = POLLIN;

    fds[2].fd = STDIN_FILENO;
    fds[2].events = POLLIN;

    map<string, vector<string>> topics_of_Clients = map<string, vector<string>>();

    while (true)
    {
        int ret = poll(fds, nfds, -1);
        DIE(ret < 0, "poll");

        for (int i = 0; i < nfds; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                if (fds[i].fd == udp_fd)
                {
                    handle_received_UDP(topics_of_Clients, udp_fd, clients_Database);
                }
                else if (fds[i].fd == tcp_fd)
                {
                    handle_new_TCP_client(clients_Database, tcp_fd, fds, &nfds, active_Clients_ID);
                }
                else if (fds[i].fd == STDIN_FILENO)
                {
                    int exit = handleUserInput(tcp_fd, udp_fd);
                    if (exit == 1)
                    {
                        delete_clients(clients_Database);
                        return 0;
                    }
                }
                else
                {
                    receive_tcp_message(clients_Database, fds[i].fd, active_Clients_ID, fds, &nfds, topics_of_Clients);
                }
            }
        }
    }

    delete_clients(clients_Database);

    return 0;
}
