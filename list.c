#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "list.h"

/* These come from server.c */
extern int numReaders;
extern pthread_mutex_t mutex;
extern pthread_mutex_t rw_lock;

/* Global heads */
user_t *users_head = NULL;
room_t *rooms_head = NULL;

/* ========== Reader / Writer lock helpers ========== */

static void begin_read(void) {
    pthread_mutex_lock(&mutex);
    numReaders++;
    if (numReaders == 1) {
        pthread_mutex_lock(&rw_lock);   // first reader locks writers out
    }
    pthread_mutex_unlock(&mutex);
}

static void end_read(void) {
    pthread_mutex_lock(&mutex);
    numReaders--;
    if (numReaders == 0) {
        pthread_mutex_unlock(&rw_lock); // last reader lets writers in
    }
    pthread_mutex_unlock(&mutex);
}

static void begin_write(void) {
    pthread_mutex_lock(&rw_lock);
}

static void end_write(void) {
    pthread_mutex_unlock(&rw_lock);
}

/* ========== Internal small helpers ========== */

static user_list_t *user_list_append(user_list_t *head, user_t *u) {
    user_list_t *node = malloc(sizeof(user_list_t));
    if (!node) return head;
    node->user = u;
    node->next = NULL;

    if (!head) return node;

    user_list_t *cur = head;
    while (cur->next) cur = cur->next;
    cur->next = node;
    return head;
}

static room_list_t *room_list_append(room_list_t *head, room_t *r) {
    room_list_t *node = malloc(sizeof(room_list_t));
    if (!node) return head;
    node->room = r;
    node->next = NULL;

    if (!head) return node;

    room_list_t *cur = head;
    while (cur->next) cur = cur->next;
    cur->next = node;
    return head;
}

static dm_list_t *dm_list_append(dm_list_t *head, user_t *peer) {
    dm_list_t *node = malloc(sizeof(dm_list_t));
    if (!node) return head;
    node->peer = peer;
    node->next = NULL;

    if (!head) return node;

    dm_list_t *cur = head;
    while (cur->next) cur = cur->next;
    cur->next = node;
    return head;
}

/* Remove from singly linked list helpers */

static user_list_t *user_list_remove(user_list_t *head, user_t *u) {
    user_list_t *cur = head;
    user_list_t *prev = NULL;

    while (cur) {
        if (cur->user == u) {
            if (prev) prev->next = cur->next;
            else head = cur->next;
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    return head;
}

static room_list_t *room_list_remove(room_list_t *head, room_t *r) {
    room_list_t *cur = head;
    room_list_t *prev = NULL;

    while (cur) {
        if (cur->room == r) {
            if (prev) prev->next = cur->next;
            else head = cur->next;
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    return head;
}

static dm_list_t *dm_list_remove(dm_list_t *head, user_t *peer) {
    dm_list_t *cur = head;
    dm_list_t *prev = NULL;

    while (cur) {
        if (cur->peer == peer) {
            if (prev) prev->next = cur->next;
            else head = cur->next;
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    return head;
}

/* ========== User operations ========== */

user_t *create_user(int socket, const char *username) {
    user_t *u = malloc(sizeof(user_t));
    if (!u) return NULL;

    u->socket = socket;
    strncpy(u->username, username, MAX_NAME - 1);
    u->username[MAX_NAME - 1] = '\0';
    u->rooms = NULL;
    u->dms = NULL;
    u->next = NULL;

    begin_write();
    u->next = users_head;
    users_head = u;
    end_write();

    return u;
}

user_t *find_user_by_socket(int socket) {
    user_t *result = NULL;

    begin_read();
    user_t *cur = users_head;
    while (cur) {
        if (cur->socket == socket) {
            result = cur;
            break;
        }
        cur = cur->next;
    }
    end_read();
    return result;
}

user_t *find_user_by_name(const char *username) {
    user_t *result = NULL;

    begin_read();
    user_t *cur = users_head;
    while (cur) {
        if (strcmp(cur->username, username) == 0) {
            result = cur;
            break;
        }
        cur = cur->next;
    }
    end_read();
    return result;
}

void user_rename(user_t *u, const char *newname) {
    if (!u || !newname) return;

    begin_write();
    strncpy(u->username, newname, MAX_NAME - 1);
    u->username[MAX_NAME - 1] = '\0';
    end_write();
}

/* Remove user from all lists and free it */
void remove_user(user_t *u) {
    if (!u) return;

    begin_write();

    /* 1) Remove from global user list */
    user_t *cur = users_head;
    user_t *prev = NULL;
    while (cur) {
        if (cur == u) {
            if (prev) prev->next = cur->next;
            else users_head = cur->next;
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    /* 2) Remove from all rooms' user lists */
    room_t *r = rooms_head;
    while (r) {
        r->users = user_list_remove(r->users, u);
        r = r->next;
    }

    /* 3) Remove u from everyone else's DM lists */
    cur = users_head;
    while (cur) {
        cur->dms = dm_list_remove(cur->dms, u);
        cur = cur->next;
    }

    /* 4) Free user's own room and DM lists */
    room_list_t *rl = u->rooms;
    while (rl) {
        room_list_t *tmp = rl;
        rl = rl->next;
        free(tmp);
    }

    dm_list_t *dl = u->dms;
    while (dl) {
        dm_list_t *tmp = dl;
        dl = dl->next;
        free(tmp);
    }

    /* Close socket just in case */
    close(u->socket);

    free(u);

    end_write();
}

/* ========== Room operations ========== */

room_t *find_room(const char *room_name) {
    room_t *result = NULL;

    begin_read();
    room_t *cur = rooms_head;
    while (cur) {
        if (strcmp(cur->name, room_name) == 0) {
            result = cur;
            break;
        }
        cur = cur->next;
    }
    end_read();
    return result;
}

room_t *create_room(const char *room_name) {
    if (!room_name) return NULL;

    begin_write();

    /* If it already exists, return existing one */
    room_t *cur = rooms_head;
    while (cur) {
        if (strcmp(cur->name, room_name) == 0) {
            end_write();
            return cur;
        }
        cur = cur->next;
    }

    room_t *r = malloc(sizeof(room_t));
    if (!r) {
        end_write();
        return NULL;
    }

    strncpy(r->name, room_name, MAX_NAME - 1);
    r->name[MAX_NAME - 1] = '\0';
    r->users = NULL;
    r->next = rooms_head;
    rooms_head = r;

    end_write();
    return r;
}

/* Not strictly needed in this project; left minimal */
void delete_room(room_t *room) {
    if (!room) return;

    begin_write();

    room_t *cur = rooms_head;
    room_t *prev = NULL;
    while (cur) {
        if (cur == room) {
            if (prev) prev->next = cur->next;
            else rooms_head = cur->next;
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    /* free its user list nodes (not the users themselves) */
    user_list_t *ul = room->users;
    while (ul) {
        user_list_t *tmp = ul;
        ul = ul->next;
        free(tmp);
    }

    free(room);

    end_write();
}

/* ========== Relationships: rooms ========== */

void user_join_room(user_t *u, room_t *r) {
    if (!u || !r) return;

    begin_write();

    /* Check if already in room (by scanning user's room list) */
    room_list_t *rl = u->rooms;
    while (rl) {
        if (rl->room == r) {
            end_write();
            return;    // already a member
        }
        rl = rl->next;
    }

    /* Add to user's room list */
    u->rooms = room_list_append(u->rooms, r);

    /* Add to room's user list */
    r->users = user_list_append(r->users, u);

    end_write();
}

void user_leave_room(user_t *u, room_t *r) {
    if (!u || !r) return;

    begin_write();
    u->rooms = room_list_remove(u->rooms, r);
    r->users = user_list_remove(r->users, u);
    end_write();
}

/* ========== Relationships: DMs (one-way) ========== */

void user_connect_dm(user_t *from, user_t *to) {
    if (!from || !to || from == to) return;

    begin_write();

    /* Check if already connected */
    dm_list_t *dl = from->dms;
    while (dl) {
        if (dl->peer == to) {
            end_write();
            return;    // already has one-way DM
        }
        dl = dl->next;
    }

    from->dms = dm_list_append(from->dms, to);

    end_write();
}

void user_disconnect_dm(user_t *from, user_t *to) {
    if (!from || !to) return;

    begin_write();
    from->dms = dm_list_remove(from->dms, to);
    end_write();
}

/* ========== Helpers for messaging logic ========== */

bool users_share_room(user_t *a, user_t *b) {
    if (!a || !b) return false;

    bool shared = false;
    begin_read();

    room_list_t *ra = a->rooms;
    while (ra && !shared) {
        room_list_t *rb = b->rooms;
        while (rb) {
            if (ra->room == rb->room) {
                shared = true;
                break;
            }
            rb = rb->next;
        }
        ra = ra->next;
    }

    end_read();
    return shared;
}

bool is_dm_peer(user_t *from, user_t *to) {
    if (!from || !to) return false;

    bool found = false;
    begin_read();

    dm_list_t *dl = from->dms;
    while (dl) {
        if (dl->peer == to) {
            found = true;
            break;
        }
        dl = dl->next;
    }

    end_read();
    return found;
}

/* ========== for_each_user with read-lock ========== */

void for_each_user(void (*cb)(user_t *u, void *ctx), void *ctx) {
    if (!cb) return;

    begin_read();
    user_t *cur = users_head;
    while (cur) {
        cb(cur, ctx);
        cur = cur->next;
    }
    end_read();
}

/* ========== Listing functions ========== */

void list_all_users(char *buffer) {
    if (!buffer) return;

    buffer[0] = '\0';

    begin_read();
    user_t *cur = users_head;
    while (cur) {
        strcat(buffer, cur->username);
        strcat(buffer, "\n");
        cur = cur->next;
    }
    end_read();
}

void list_all_rooms(char *buffer) {
    if (!buffer) return;

    buffer[0] = '\0';

    begin_read();
    room_t *cur = rooms_head;
    while (cur) {
        strcat(buffer, cur->name);
        strcat(buffer, "\n");
        cur = cur->next;
    }
    end_read();
}

/* ========== Cleanup on Ctrl-C ========== */

void cleanup_all(void) {
    begin_write();

    /* Free all rooms and their user_list nodes */
    room_t *r = rooms_head;
    while (r) {
        room_t *rnext = r->next;

        user_list_t *ul = r->users;
        while (ul) {
            user_list_t *tmp = ul;
            ul = ul->next;
            free(tmp);
        }

        free(r);
        r = rnext;
    }
    rooms_head = NULL;

    /* Free all users and their room_list + dm_list nodes, close sockets */
    user_t *u = users_head;
    while (u) {
        user_t *unext = u->next;

        room_list_t *rl = u->rooms;
        while (rl) {
            room_list_t *tmp = rl;
            rl = rl->next;
            free(tmp);
        }

        dm_list_t *dl = u->dms;
        while (dl) {
            dm_list_t *tmp = dl;
            dl = dl->next;
            free(tmp);
        }

        close(u->socket);
        free(u);
        u = unext;
    }
    users_head = NULL;

    end_write();
}
