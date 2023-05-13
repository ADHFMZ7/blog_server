#include "blog.h"

#include "sqlite3/sqlite3.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int open_db_connection(DBConnection *conn, const char *db_filename) {
    int rc = sqlite3_open(db_filename, &(conn->db));
    if (rc != SQLITE_OK) {
        conn->errmsg = strdup(sqlite3_errmsg(conn->db));
        return 1;
    }
    return 0;
}

int close_db_connection(DBConnection *conn) {
    int rc = sqlite3_close(conn->db);
    if (rc != SQLITE_OK) {
        conn->errmsg = strdup(sqlite3_errmsg(conn->db));
        return 1;
    }
    return 0;
}

int create_blog_table(DBConnection *conn) {
    const char *sql = "CREATE TABLE IF NOT EXISTS blog_posts ("
                      "post_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "user TEXT NOT NULL,"
                      "title TEXT NOT NULL,"
                      "content TEXT NOT NULL);";
    char *errmsg;
    int rc = sqlite3_exec(conn->db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        conn->errmsg = strdup(errmsg);
        sqlite3_free(errmsg);
        return 1;
    }
    return 0;
}

int insert_blog_post(DBConnection *conn, BlogPost *post) {
    const char *sql = "INSERT INTO blog_posts (user, title, content) "
                      "VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(conn->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        conn->errmsg = strdup(sqlite3_errmsg(conn->db));
        return 1;
    }
    sqlite3_bind_text(stmt, 1, post->user, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, post->title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, post->content, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        conn->errmsg = strdup(sqlite3_errmsg(conn->db));
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int select_blog_post(DBConnection *conn, int post_id, BlogPost *post) {
    const char *sql = "SELECT user, title, content FROM blog_posts WHERE post_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(conn->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        conn->errmsg = strdup(sqlite3_errmsg(conn->db));
        return 1;
    }
    sqlite3_bind_int(stmt, 1, post_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        conn->errmsg = strdup(sqlite3_errmsg(conn->db));
        sqlite3_finalize(stmt);
        return 1;
    }
    post->post_id = post_id;
    post->user = strdup((const char *) sqlite3_column_text(stmt, 0));
    post->title = strdup((const char *) sqlite3_column_text(stmt, 1));
    post->content = strdup((const char *) sqlite3_column_text(stmt, 2));
    sqlite3_finalize(stmt);
    return 0;
}

int get_next_post_id(DBConnection *conn) {
    sqlite3_stmt *stmt;
    int max_id = 0;
    const char *sql = "SELECT MAX(post_id) FROM blog_posts;";
    int rc = sqlite3_prepare_v2(conn->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        conn->errmsg = strdup((const char *) sqlite3_errmsg(conn->db));
        return -1;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        max_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return max_id + 1;
}

void parse_blog_post(const char *query_string, char *user, char *title, char *content) {
    char *token, *pair, *saveptr;

    char buffer[strlen(query_string) + 1];
    strcpy(buffer, query_string);

    for (int i = 0; buffer[i]; i++) {
        if (buffer[i] == '+') {
            buffer[i] = ' ';
        }
    }

    token = strtok_r(buffer, "&", &saveptr);
    while (token != NULL) {
        pair = strtok(token, "=");
        if (pair != NULL) {
            if (strcmp(pair, "user") == 0) {
                pair = strtok(NULL, "=");
                if (pair != NULL) {
                    strcpy(user, pair);
                }
            } else if (strcmp(pair, "title") == 0) {
                pair = strtok(NULL, "=");
                if (pair != NULL) {
                    strcpy(title, pair);
                }
            } else if (strcmp(pair, "content") == 0) {
                pair = strtok(NULL, "=");
                if (pair != NULL) {
                    strcpy(content, pair);
                }
            }
        }
        // get the next pair
        token = strtok_r(NULL, "&", &saveptr);
    }
}

