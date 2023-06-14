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

/*
 * @brief Se printeaza un mesaj de tip INT
*/
void print_INT_TYPE(char *payload)
{
    std::cout << "INT - ";
    if ((uint8_t)payload[0] == 1)
    {
        std::cout << "-";
    }
    uint32_t *nr = (uint32_t *)(payload + 1);
    printf("%d\n", ntohl(*nr));
}

/*
 * @brief se printeaza un mesaj de tip SHORT_REAL
*/
void print_SHORT_REAL_TYPE(char *payload)
{
    std::cout << "SHORT_REAL - ";
    uint16_t *nr = (uint16_t *)payload;
    uint16_t nr_host = ntohs(*nr);

    uint16_t first_dec = (nr_host % 100) / 10;
    uint16_t second_dec = nr_host % 10;
    std::cout << nr_host / 100 << "." << first_dec << second_dec << std::endl;
}

/*
 * @brief primeste un mesaj de la tastatura si il trimite la server
    * inchide conexiunea daca se primeste comanda exit
    * inchide conexiunea daca se primeste o comanda necunoscuta
    * trimite comanda subscribe/unsubscribe la server urmata de topic si sf
*/
int handle_stdin(int sockfd)
{
    char buffer[MAX_TCP_CMD_LEN];
    memset(buffer, 0, MAX_TCP_CMD_LEN);
    fgets(buffer, MAX_TCP_CMD_LEN - 1, stdin);

    if (buffer[strlen(buffer) - 1] == '\n')
    {
        buffer[strlen(buffer) - 1] = '\0';
    }

    if (strncmp(buffer, "exit", 4) == 0)
    {
        return -1;
    }

    if (strncmp(buffer, "subscribe", 9) == 0)
    {
        int s_rp = send(sockfd, buffer, strlen(buffer), 0);
        DIE(s_rp < 0, "send subscribe");
        std::cout << "Subscribed to topic.\n";
    }
    else if (strncmp(buffer, "unsubscribe", 11) == 0)
    {
        int u_rp = send(sockfd, buffer, strlen(buffer), 0);
        DIE(u_rp < 0, "send unsubscribe");
        std::cout << "Unsubscribed from topic.\n";
    }
    else
    {
        DIE(1, "Unknown command");
        return -1;
    }

    return 0;
}

/*
 * @brief se printeaza un mesaj de tip FLOAT
*/
void print_FLOAT_TYPE(char *payload)
{
    std::cout << "FLOAT - ";
    if (uint8_t(payload[0]) == 1)
    {
        std::cout << "-";
    }
    uint32_t *nr = (uint32_t *)(payload + 1);
    uint32_t nr_host = ntohl(*nr);

    uint8_t *exp = (uint8_t *)(payload + 5);

    float power10 = pow(10, *exp);

    printf("%f\n", nr_host / power10);
}

/*
 * @brief primeste un mesaj de la server
 * inchide conexiunea daca nu se primeste nimic
*/
int receive_message(int sockfd, 
                    struct TCP_message *message)
{
    char buffer[sizeof(struct TCP_message)];

    int n = recv(sockfd, buffer, sizeof(struct TCP_message), 0);
    DIE(n < 0, "recv");

    if (n == 0)
    {
        return -1;
    }

    *message = *((struct TCP_message *)buffer);

    return 0;
}

/*
 * @brief primeste un mesaj de la server si il afiseaza
    * inchide conexiunea daca nu se primeste nimic
    * se printeaza prima parte a mesajului
    * se printeaza payload-ul in functie de tipul mesajului
    * se printeaza mesajul complet
*/
int print_received_payload(int sockfd)
{
    struct TCP_message message;

    int result = receive_message(sockfd, &message);

    if (result == -1)
    {
        return -1;
    }

    printf("%s:%d - %s - ",
           inet_ntoa(message.source_ip),
           message.source_port,
           message.message.topic);

    switch (message.message.type)
    {
        case 0:
            print_INT_TYPE(message.message.payload);
            break;
        case 1:
            print_SHORT_REAL_TYPE(message.message.payload);
            break;
        case 2:
            print_FLOAT_TYPE(message.message.payload);
            break;
        case 3:
            std::cout << "STRING - " << message.message.payload << std::endl;
            break;
        default:
            DIE(1, "Unknown type");
    }

    return 0;
}


void disable_nagle(int sockfd)
{
    int aux = 1;
    int deactivate = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&aux, sizeof(int));
    DIE(deactivate < 0, "Error deactivating Nagle's algorithm");
}

int create_socket_and_connect(char *server_ip, int server_port)
{
    int sockfd, ret;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        close(sockfd);
        return -1;
    }

    disable_nagle(sockfd);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    ret = inet_aton(server_ip, &serv_addr.sin_addr);
    if (ret == 0)
    {
        close(sockfd);
        DIE(ret <= 0, "inet_aton");
        return -1;
    }

    ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret < 0)
    {
        close(sockfd);
        DIE(ret < 0, "connect");
        return -1;
    }

    return sockfd;
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc < 4, "Not enough arguments provided");

    int sockfd = create_socket_and_connect(argv[2], atoi(argv[3]));
    // se trimite ID-ul acestui client
    int ret = send(sockfd, argv[1], strlen(argv[1]), 0);
    DIE(ret < 0, "send");

    struct pollfd fds[2];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    while (1)
    {
        ret = poll(fds, 2, -1);
        DIE(ret < 0, "poll");

        // daca se primeste mesaj de la server
        if (fds[0].revents & POLLIN)
        {
            ret = print_received_payload(sockfd);
            if (ret < 0)
            {
                break;
            }
        }

        // daca se primeste mesaj de la tastatura
        if (fds[1].revents & POLLIN)
        {
            ret = handle_stdin(sockfd);
            if (ret < 0)
            {
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
