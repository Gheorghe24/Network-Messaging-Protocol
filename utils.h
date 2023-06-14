#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

#ifndef _HELPERS_H
#define _HELPERS_H 1

#define MAX_TCP_CMD_LEN	65
using namespace std;

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

struct UDP_FORMAT {
	char topic[50];
	uint8_t type;
	char payload[1500];
};

struct TCP_message {
	struct UDP_FORMAT message;
	struct in_addr source_ip;
	in_port_t source_port;
};

#endif