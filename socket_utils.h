#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define PORT 8080
#define BACKLOG 10

int create_server_socket();
int create_client_socket(const char *server_ip);

#endif // SOCKET_UTILS_H 