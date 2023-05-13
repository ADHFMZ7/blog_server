#ifndef BLOG_H
#define BLOG_H

#include "sqlite3/sqlite3.h"

typedef struct {
    int post_id;
    char *user;
    char *title;
    char *content;
} BlogPost;

typedef struct {
    sqlite3 *db;
    char *errmsg;
} DBConnection;

int open_db_connection(DBConnection *conn, const char *db_filename);

int close_db_connection(DBConnection *conn);

int create_blog_table(DBConnection *conn);

int insert_blog_post(DBConnection *conn, BlogPost *post);

int select_blog_post(DBConnection *conn, int post_id, BlogPost *post);

int get_next_post_id(DBConnection *conn);

void parse_blog_post(const char *query_string, char *user, char *title, char *content);
#endif

