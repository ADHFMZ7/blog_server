#include <arpa/inet.h>

#ifndef CLIENT_H
#define CLIENT_H

#define FAIL 0
#define NONEXISTENT_FILE 1
#define SUCCESS 2


typedef struct {
  int id;
  int socket_fd;
  struct sockaddr_in address;
} Client;

Client *client_new( int sock_fd, struct sockaddr_in *addr);

// closes socket also
void client_free(Client* cl);

int client_socket(Client* cl);
struct sockaddr_in client_address(Client* cl);

int client_write_buffer(Client* cl, char* buffer, int buffer_len);
int client_write_string(Client* cl, char* buffer);

int client_id(Client* cl);

#endif

