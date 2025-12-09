#include "server.h"

/* USE THESE LOCKS AND COUNTER TO SYNCHRONIZE (managed inside list.c) */

extern int numReaders;
extern pthread_mutex_t rw_lock;
extern pthread_mutex_t mutex;

extern const char *server_MOTD;

/* Context used when broadcasting a chat message */
struct send_ctx {
    user_t *sender;
    char message[MAXBUFF];
};

/* Helper to trim whitespace (unchanged) */
char *trimwhitespace(char *str)
{
  char *end;

  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)
    return str;

  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  end[1] = '\0';
  return str;
}

/* Callback used by for_each_user to send a message to appropriate recipients */
static void send_message_cb(user_t *u, void *ctx_void) {
    struct send_ctx *ctx = (struct send_ctx *)ctx_void;
    user_t *sender = ctx->sender;

    if (!u || !sender) return;
    if (u == sender) return; // don't send to self

    // Check: share a room?
    bool shared_room = false;
    room_list_t *sr = sender->rooms;
    while (sr && !shared_room) {
        room_list_t *ur = u->rooms;
        while (ur) {
            if (ur->room == sr->room) {
                shared_room = true;
                break;
            }
            ur = ur->next;
        }
        sr = sr->next;
    }

    // Check: one-way DM from sender -> u ?
    bool dm = false;
    dm_list_t *dl = sender->dms;
    while (dl && !dm) {
        if (dl->peer == u) {
            dm = true;
            break;
        }
        dl = dl->next;
    }

    if (shared_room || dm) {
        send(u->socket, ctx->message, strlen(ctx->message), 0);
    }
}

/*
 * Main thread for each client. Receives all messages,
 * and passes the data off to the correct function.
 */
void *client_receive(void *ptr) {
   int client = *(int *) ptr;  // socket
  
   int received, i;
   char buffer[MAXBUFF], sbuffer[MAXBUFF];  
   char tmpbuf[MAXBUFF];     
   char cmd[MAXBUFF], username[20];
   char *arguments[80];

   // Create guest user and add to Lobby
   sprintf(username,"guest%d", client);
   user_t *me = create_user(client, username);
   room_t *lobby = create_room(DEFAULT_ROOM);
   if (me && lobby) {
       user_join_room(me, lobby);
   }

   // Send MOTD
   send(client, server_MOTD, strlen(server_MOTD), 0);

   while (1) {
      received = read(client , buffer, MAXBUFF);

      if (received <= 0) {
          // client disconnected
          if (me) {
              remove_user(me);
          }
          close(client);
          break;
      }

      buffer[received] = '\0'; 
      strcpy(cmd, buffer);  
      strcpy(sbuffer, buffer);

      /////////////////////////////////////////////////////
      // Tokenize the input in buffer
      arguments[0] = strtok(cmd, delimiters);

      i = 0;
      while (arguments[i] != NULL) {
          arguments[++i] = strtok(NULL, delimiters);
          if (arguments[i-1]) {
              arguments[i-1] = trimwhitespace(arguments[i-1]);
          }
      }

      if (arguments[0] == NULL) {
          sprintf(buffer, "\nchat>");
          send(client , buffer , strlen(buffer) , 0 );
          continue;
      }

      // Arg[0] = command
      // Arg[1] = user or room (if present)

      /////////////////////////////////////////////////////
      // Commands

      if(strcmp(arguments[0], "create") == 0)
      {
           if (!arguments[1]) {
               sprintf(buffer, "Usage: create <room>\nchat>");
               send(client, buffer, strlen(buffer), 0);
           } else {
               room_t *r = create_room(arguments[1]);
               if (r && me) {
                   user_join_room(me, r);
                   snprintf(buffer, MAXBUFF, "Created and joined room '%s'\nchat>", arguments[1]);
               } else {
                   snprintf(buffer, MAXBUFF, "Error creating room '%s'\nchat>", arguments[1]);
               }
               send(client, buffer, strlen(buffer), 0);
           }
      }
      else if (strcmp(arguments[0], "join") == 0)
      {
           if (!arguments[1]) {
               sprintf(buffer, "Usage: join <room>\nchat>");
               send(client, buffer, strlen(buffer), 0);
           } else {
               room_t *r = create_room(arguments[1]); // idempotent
               if (r && me) {
                   user_join_room(me, r);
                   snprintf(buffer, MAXBUFF, "Joined room '%s'\nchat>", arguments[1]);
               } else {
                   snprintf(buffer, MAXBUFF, "Error joining room '%s'\nchat>", arguments[1]);
               }
               send(client, buffer, strlen(buffer), 0);
           }
      }
      else if (strcmp(arguments[0], "leave") == 0)
      {
           if (!arguments[1]) {
               sprintf(buffer, "Usage: leave <room>\nchat>");
               send(client, buffer, strlen(buffer), 0);
           } else {
               room_t *r = find_room(arguments[1]);
               if (r && me) {
                   user_leave_room(me, r);
                   snprintf(buffer, MAXBUFF, "Left room '%s'\nchat>", arguments[1]);
               } else {
                   snprintf(buffer, MAXBUFF, "Room '%s' does not exist\nchat>", arguments[1]);
               }
               send(client, buffer, strlen(buffer), 0);
           }
      } 
      else if (strcmp(arguments[0], "connect") == 0)
      {
           if (!arguments[1]) {
               sprintf(buffer, "Usage: connect <user>\nchat>");
               send(client, buffer, strlen(buffer), 0);
           } else if (!me) {
               sprintf(buffer, "Error: user not initialized\nchat>");
               send(client, buffer, strlen(buffer), 0);
           } else {
               user_t *other = find_user_by_name(arguments[1]);
               if (other) {
                   user_connect_dm(me, other);   // one-way DM from me -> other
                   snprintf(buffer, MAXBUFF, "Connected (DM) to user '%s'\nchat>", arguments[1]);
               } else {
                   snprintf(buffer, MAXBUFF, "User '%s' not found\nchat>", arguments[1]);
               }
               send(client, buffer, strlen(buffer), 0);
           }
      }
      else if (strcmp(arguments[0], "disconnect") == 0)
      {             
           if (!arguments[1]) {
               sprintf(buffer, "Usage: disconnect <user>\nchat>");
               send(client, buffer, strlen(buffer), 0);
           } else if (!me) {
               sprintf(buffer, "Error: user not initialized\nchat>");
               send(client, buffer, strlen(buffer), 0);
           } else {
               user_t *other = find_user_by_name(arguments[1]);
               if (other) {
                   user_disconnect_dm(me, other);
                   snprintf(buffer, MAXBUFF, "Disconnected DM from user '%s'\nchat>", arguments[1]);
               } else {
                   snprintf(buffer, MAXBUFF, "User '%s' not found\nchat>", arguments[1]);
               }
               send(client, buffer, strlen(buffer), 0);
           }
      }                  
      else if (strcmp(arguments[0], "rooms") == 0)
      {
           printf("List all the rooms\n");

           char listbuf[MAXBUFF];
           list_all_rooms(listbuf);

           snprintf(buffer, MAXBUFF, "Rooms:\n%s\nchat>", listbuf);
           send(client , buffer , strlen(buffer) , 0 );                            
      }   
      else if (strcmp(arguments[0], "users") == 0)
      {
           printf("List all the users\n");

           char listbuf[MAXBUFF];
           list_all_users(listbuf);

           snprintf(buffer, MAXBUFF, "Users:\n%s\nchat>", listbuf);
           send(client , buffer , strlen(buffer) , 0 );
      }                           
      else if (strcmp(arguments[0], "login") == 0)
      {
           if (!arguments[1]) {
               sprintf(buffer, "Usage: login <username>\nchat>");
               send(client, buffer, strlen(buffer), 0);
           } else if (!me) {
               sprintf(buffer, "Error: user not initialized\nchat>");
               send(client, buffer, strlen(buffer), 0);
           } else {
               user_rename(me, arguments[1]);
               snprintf(buffer, MAXBUFF, "Logged in as '%s'\nchat>", arguments[1]);
               send(client, buffer, strlen(buffer), 0);
           }
      } 
      else if (strcmp(arguments[0], "help") == 0 )
      {
           sprintf(buffer,
               "login <username> - \"login with username\" \n"
               "create <room>   - \"create a room\" \n"
               "join <room>     - \"join a room\" \n"
               "leave <room>    - \"leave a room\" \n"
               "users           - \"list all users\" \n"
               "rooms           - \"list all rooms\" \n"
               "connect <user>  - \"connect to user\" \n"
               "disconnect <user> - \"disconnect from user\" \n"
               "exit/logout     - \"exit chat\" \n"
               "help            - \"show this help\" \n"
               "Any other text  - \"chat message\"\nchat>");
           send(client , buffer , strlen(buffer) , 0 ); 
      }
      else if (strcmp(arguments[0], "exit") == 0 || strcmp(arguments[0], "logout") == 0)
      {
           if (me) {
               remove_user(me);
               me = NULL;
           }
           close(client);
           break;    
      }                         
      else { 
           /////////////////////////////////////////////////////////////
           // Sending a chat message:
           // Format:
           // ::[userfrom]> <message>\nchat>

           struct send_ctx ctx;
           ctx.sender = me;
           snprintf(ctx.message, MAXBUFF, "\n::%s> %s\nchat>",
                    me ? me->username : "unknown", sbuffer);

           for_each_user(send_message_cb, &ctx);
      }

      memset(buffer, 0, sizeof(buffer));
   }

   return NULL;
}