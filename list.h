#ifndef LIST_H
#define LIST_H

#include <stdbool.h>
#include <pthread.h>

#define MAX_NAME 30

/* Forward declarations */
typedef struct user user_t;
typedef struct room room_t;
typedef struct user_list user_list_t;
typedef struct room_list room_list_t;
typedef struct dm_list dm_list_t;

/* -------------------- USER STRUCT -------------------- */

struct user {
    int socket;                 // socket descriptor
    char username[MAX_NAME];    // username
    room_list_t *rooms;         // rooms this user is in
    dm_list_t *dms;             // users this user has DM connections TO (one-way)
    user_t *next;               // next user in global user list
};

/* -------------------- ROOM STRUCT -------------------- */

struct room {
    char name[MAX_NAME];        // room name
    user_list_t *users;         // users in this room
    room_t *next;               // next room in global room list
};

/* ------------- LINKED LIST NODE STRUCTS --------------- */

struct user_list {
    user_t *user;
    user_list_t *next;
};

struct room_list {
    room_t *room;
    room_list_t *next;
};

struct dm_list {
    user_t *peer;               // one-way DM: from -> peer
    dm_list_t *next;
};

/* ================== GLOBAL HEADS ====================== */

extern user_t *users_head;
extern room_t *rooms_head;

/* ================== FUNCTION PROTOTYPES =============== */

/* User operations */
user_t *create_user(int socket, const char *username);
user_t *find_user_by_name(const char *username);
user_t *find_user_by_socket(int socket);
void    user_rename(user_t *u, const char *newname);
void    remove_user(user_t *u);

/* Room operations */
room_t *create_room(const char *room_name);
room_t *find_room(const char *room_name);
void    delete_room(room_t *room);   // not strictly required

/* Relationships: rooms */
void user_join_room(user_t *u, room_t *r);
void user_leave_room(user_t *u, room_t *r);

/* Relationships: DMs (one-way) */
void user_connect_dm(user_t *from, user_t *to);      // from → to
void user_disconnect_dm(user_t *from, user_t *to);   // remove from->to link

/* Helpers for messaging logic (optional outside usage) */
bool users_share_room(user_t *a, user_t *b);
bool is_dm_peer(user_t *from, user_t *to);

/* Iterate over all users with proper read-locking */
void for_each_user(void (*cb)(user_t *u, void *ctx), void *ctx);

/* Listing (results written into buffer as text) */
void list_all_users(char *buffer);
void list_all_rooms(char *buffer);

/* Cleanup everything (for Ctrl-C) */
void cleanup_all(void);

#endif