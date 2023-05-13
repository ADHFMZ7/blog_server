#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Client.h"
#include "blog.h"

int debug = 1;
DBConnection db;

#define LISTEN_PORT 8888
#define PENDING_CONNECTIONS_QUEUE_LENGTH 3
#define MAX_MESSAGE_LENGTH (10 * 1024 * 1024)
#define MAX_GENERATED_LENGTH 1024
#define MAX_FILESIZE 30 * 1024 * 1024
#define DB_NAME "starter.db"

// Thread payload
typedef struct {
  Client *client;
} Thread_data;

// Takes a Thread_data*.
void *single_client_handler_threadfunc(void *);

// forward decls
//! All return FAIL (0). Anything else is successey
int establish_listening_socket(int port_to_listen);
int handle_new_client_wrapper(Client *cl);
int handle_new_client_guts(Client *cl);
int accept_a_client(int listen_socket, Client **new_client_ptr);
int close_down_listening(int listening_socket);
int read_http_request(int socket_fd, char **request_ptr);
int respond_to_http_request(Client *cl, char *request, char *requestBody);
int send_http_response(Client *cl, char *body);
int handle_static_request(Client *cl, char *request);
int handle_publish_request(Client *cl, char *request);
int handle_post_request(Client *cl, char *request);
int handle_post_index_request(Client *cl, char *request);
void generate_blog_index(DBConnection *db);
// this returns FAIL (system error - close connection), SUCCESS,
// or NONEXISTENT_FILE
int read_file_contents(const char *file_path, char **buf, int *file_sz);

int main(int argc, char *argv[]) {

  if (open_db_connection(&db, DB_NAME) != 0) {
    fprintf(stderr, "Error opening database!\n");
    exit(EXIT_FAILURE);
  }

  if (create_blog_table(&db) != 0) {
    fprintf(stderr, "Error creating table!\n");
    close_db_connection(&db);
    exit(EXIT_FAILURE);
  }

  int port = LISTEN_PORT;
  if (argc > 1)
    port = atoi(argv[1]);

  int our_socket_fd = establish_listening_socket(port);
  if (our_socket_fd == FAIL) {
    puts("exiting.");
    exit(1);
  }

  if (debug)
    puts("Ready for incoming connections...");

  int keep_going = SUCCESS;
  while (keep_going != FAIL) {
    Client *new_client;
    keep_going = accept_a_client(our_socket_fd, &new_client);

    if (keep_going != FAIL) {
      keep_going = handle_new_client_wrapper(new_client);
    }
  }

  close_down_listening(our_socket_fd);

  return 0;
}

// returns FAIL for failure, otherwise the fd to accept on
int establish_listening_socket(int port_to_listen) {
  int new_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (new_socket_fd == -1) {
    perror("Could not create socket");
    return FAIL;
  }
  if (debug)
    fprintf(stderr, "accept socket fd is %d\n", new_socket_fd);

  // We are going to listen on any address, the specified port
  struct sockaddr_in our_address;
  our_address.sin_family = AF_INET;
  our_address.sin_addr.s_addr = INADDR_ANY;
  our_address.sin_port = htons(port_to_listen);

  // Bind our socket to the given address
  if (bind(new_socket_fd, (struct sockaddr *)&our_address,
           sizeof(our_address)) < 0) {
    perror("bind failed");
    close(new_socket_fd);
    return FAIL;
  }
  if (debug)
    fprintf(stderr, "bind done on port %d\n", port_to_listen);

  // establish that we are expecting incoming connections
  int result = listen(new_socket_fd, PENDING_CONNECTIONS_QUEUE_LENGTH);
  if (result == -1) {
    perror("listen failed");
    return FAIL;
  }

  return new_socket_fd;
}

int accept_a_client(int listen_socket, Client **new_client_ptr) {
  struct sockaddr_in client_addr;
  // we must use a variable because accept() writes to it
  socklen_t sock_len = sizeof(client_addr);

  if (debug)
    fprintf(stderr, "accepting a connection on fd %d\n", listen_socket);

  int new_socket_fd =
      accept(listen_socket, (struct sockaddr *)&client_addr, &sock_len);
  if (new_socket_fd < 0) {
    perror("accept failed");
    return FAIL;
  }
  if (debug)
    fprintf(stderr, "Connection accepted. client fd is %d\n", new_socket_fd);

  Client *cl = client_new(new_socket_fd, &client_addr);
  *new_client_ptr = cl;
  return SUCCESS;
}

int close_down_listening(int listening_socket) {
  if (debug)
    fprintf(stderr, "closing socket fd %d\n", listening_socket);

  close(listening_socket);

  return SUCCESS;
}

// returns FAIL for error, 1 for success
//! Currently no "time to quit" handling
int handle_new_client_wrapper(Client *cl) {
  pthread_t client_handler_thread;

  // we must allocate this because we (probably) return before thread executes
  Thread_data *client_info = malloc(sizeof(Thread_data));

  client_info->client = cl;

  int result =
      pthread_create(&client_handler_thread,
                     NULL, // Use default thread attributes
                     single_client_handler_threadfunc, (void *)client_info);
  if (result < 0) {
    perror("pthread_create");
    return FAIL;
  }

  if (debug)
    fprintf(stderr, "Client handling thread is %lu\n", client_handler_thread);

  return SUCCESS;
}

// Payload ptr will be freed in this handler
void *single_client_handler_threadfunc(void *payload_ptr) {
  Client *client = ((Thread_data *)payload_ptr)->client;
  free(payload_ptr);

  int client_index = client_id(client);
  int result = handle_new_client_guts(client);

  if (debug)
    fprintf(stderr, "handle_new_client_guts (id %d) returned %d\n",
            client_index, result);

  return NULL;
}

int handle_new_client_guts(Client *client) {
  while (1) {
    char *request;
    int result = read_http_request(client_socket(client), &request);

    if (result == FAIL) {
      fprintf(stderr, "client %d read failed - closing, returning",
              client_id(client));
      client_free(client);
      return FAIL;
    }

    if (strlen(request) == 0) {
      fprintf(stderr, "client %d closed socket - closing, returning\n",
              client_id(client));
      client_free(client);
      free(request);
      return SUCCESS;
    }

    if (debug)
      fprintf(stderr,
              "client sent request (%d bytes): \n"
              "---\n"
              "%s\n"
              "---\n",
              result, request);

    char *requestBody = request;
    while (requestBody[0] && strncmp(requestBody, "\r\n\r\n", 4)) {
      requestBody++;
    }
    if (requestBody[0])
      requestBody += strlen("\r\n\r\n");

    if (debug)
      fprintf(stderr, "Request body is: '%s'\n", requestBody);

    result = respond_to_http_request(client, request, requestBody);
    free(request);
    if (result == FAIL) {
      fprintf(stderr, "client %d response failed - closing, returning",
              client_id(client));
      client_free(client);
      return FAIL;
    }
  }
}

// technically, we're just reading whatever they send us.
//! Note, this fails for > MAX bytes
// This is quasi-intentional because using blocking reads
// and reading in chunks is more complicated than you
// might think.
int read_http_request(int socket_fd, char **request_ptr) {
  *request_ptr = malloc(MAX_MESSAGE_LENGTH + 1);

  int amount_read = read(socket_fd, *request_ptr, MAX_MESSAGE_LENGTH);

  if (amount_read < 0) {
    perror("read_http_request");
    free(*request_ptr);
    *request_ptr = NULL;
    return FAIL;
  }

  (*request_ptr)[amount_read] = '\0';
  *request_ptr = realloc(*request_ptr, amount_read + 1);

  if (amount_read == 0) {
    // client side closed connection
    if (debug)
      fputs("Client closed connection\n", stderr);

    return SUCCESS;
  }

  if (debug)
    fprintf(stderr, "Read %d bytes...\n", amount_read);

  return SUCCESS;
}

int send_http_response_binary(Client *cl, char *body, int body_len) {
  const char *canned_msg___fmt = "HTTP/1.1 200\n"
                                 "Content-type: text/html\n"
                                 "Content-Length: %d\n"
                                 "Connection: Keep-Alive\n"
                                 "\n";

  // 10 = space for formatted %d
  int response_buffer_size = strlen(canned_msg___fmt) + 10;
  char *response = malloc(response_buffer_size);

  snprintf(response, response_buffer_size, canned_msg___fmt, body_len);

  int result = client_write_string(cl, response);
  free(response);
  if (result == FAIL) {
    return FAIL;
  }

  client_write_buffer(cl, body, body_len);

  return result;
}

int send_http_response(Client *cl, char *body) {
  return send_http_response_binary(cl, body, strlen(body));
}

int send_error_response(Client *cl) {
  return send_http_response(cl, "Invalid request.\n"
                                "\n"
                                "Not found.\n");
}

int respond_to_http_request(Client *cl, char *request, char *requestBody) {

  if (!strncmp(request, "GET /post/", strlen("GET /post/"))) {
    return handle_post_request(cl, request);
  }

  if (!strncmp(request, "GET /posts", strlen("GET /posts"))) {
    return handle_post_index_request(cl, request);
  }

  if (!strncmp(request, "GET /", strlen("GET /"))) {
    return handle_static_request(cl, request);
  }

  if (!strncmp(request, "POST /publish", strlen("POST /publish"))) {
    return handle_publish_request(cl, request);
  }

  send_error_response(cl);
  return SUCCESS;
}

int handle_static_request(Client *cl, char *request) {
  char file_path[MAX_GENERATED_LENGTH];
  int result = sscanf(request, "GET /%s ", file_path);

  if (result < 1 || result == EOF) {
    send_error_response(cl);
    return SUCCESS;
  }

  if (strcmp(file_path, "HTTP/1.1") == 0) {
    strcpy(file_path, "index");
  }

  char *file_contents = NULL;
  int file_sz;
  strcat(file_path, ".html");


  result = read_file_contents(file_path, &file_contents, &file_sz);

  if (result == FAIL)
    return FAIL;
  if (result == NONEXISTENT_FILE) {
    return send_http_response(cl, "Nonexistent resource\n");
  }
  return send_http_response_binary(cl, file_contents, file_sz);
}

int handle_publish_request(Client *cl, char *request) {
  char file_path[MAX_GENERATED_LENGTH];

  // parse the request and extract name, title, post

  char *requestBody = request;
  while (requestBody[0] && strncmp(requestBody, "\r\n\r\n", 4)) {
    requestBody++;
  }
  if (requestBody[0])
    requestBody += strlen("\r\n\r\n");

  printf("ABOUT TO PARSE\n\n\n");

  char user[MAX_GENERATED_LENGTH];
  char title[MAX_GENERATED_LENGTH];
  char content[MAX_GENERATED_LENGTH];

  parse_blog_post(requestBody, user, title, content);

  BlogPost post;
  post.user = user;
  post.title = title;
  post.content = content;
  post.post_id = get_next_post_id(&db);

  printf("PARSED THE POST");
  printf("THE NAME OFT EH POST IS %s\n\n\n", post.title);

  if (insert_blog_post(&db, &post) != 0) {
    fprintf(stderr, "Error insterting post!\n");
    close_db_connection(&db);
    exit(EXIT_FAILURE);
  }


  // BlogPost select_post;
  // if (select_blog_post(&db, post->post_id, &select_post) != 0) {
  //   fprintf(stderr, "Error selecting post");
  //   close_db_connection(&db);
  //   exit(EXIT_FAILURE);
  // }

  return send_http_response(cl, "<html><h1>Blog Posted!</h1>\n\n<a href=\"index\">Click to go back</a></html>\n");
}
 


int handle_post_request(Client *cl, char *request) {
  char post_id_str[MAX_GENERATED_LENGTH];
  int result = sscanf(request, "GET /post/%s ", post_id_str);

  if (result < 1 || result == EOF) {
        send_error_response(cl);
        return SUCCESS;
    }

    int post_id = atoi(post_id_str);

    BlogPost post;
    result = select_blog_post(&db, post_id, &post);

    if (result == 1) {
        send_http_response(cl, "Could not select post\n");
        return SUCCESS;
    }

    char html[MAX_GENERATED_LENGTH];
    sprintf(html, "<html><head><title>%s</title></head><body><h1>%s</h1><h3>%s</h3><p>%s</p><a href=\"/index\">back</a></body></html>", post.title, post.title, post.user, post.content);

    return send_http_response_binary(cl, html, strlen(html));

}

int handle_post_index_request(Client *cl, char *request) {
  generate_blog_index(&db);
  return handle_static_request(cl, request);
}


int file_size(FILE *fp) {
  // https://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c

  int current_position = ftell(fp);

  fseek(fp, 0L, SEEK_END);

  int file_sz = ftell(fp);

  fseek(fp, current_position, SEEK_SET);

  return file_sz;
}

int read_file_contents(const char *file_path, char **buf, int *file_sz) {
  FILE *fp = fopen(file_path, "r");

  if (!fp) {
    return NONEXISTENT_FILE;
  }

  *file_sz = file_size(fp);
  *buf = malloc(*file_sz);
  fread(*buf, sizeof(char), *file_sz, fp);

  fclose(fp);

  return SUCCESS;
}

void generate_blog_index(DBConnection *db) {
    FILE *fp = fopen("posts.html", "w");
    if (fp == NULL) {
        fprintf(stderr, "Error opening output file for writing\n");
        return;
    }
    fprintf(fp, "<html>\n<head>\n<title>Blog Index</title>\n</head>\n<body>\n");
    fprintf(fp, "<h1>Blog Index</h1>\n");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT post_id, title FROM blog_posts";
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error preparing SQL statement: %s\n", sqlite3_errmsg(db->db));
        fclose(fp);
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int post_id = sqlite3_column_int(stmt, 0);
        const char *title = (const char *) sqlite3_column_text(stmt, 1);
        fprintf(fp, "<p><a href=\"/post/%d\">%s</a></p>\n", post_id, title);
    }
    sqlite3_finalize(stmt);

    fprintf(fp, "</body>\n</html>\n");
    fclose(fp);
}


