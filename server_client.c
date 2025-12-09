#include "server.h"

#define DEFAULT_ROOM "Lobby"

// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE
extern int numReaders;
extern pthread_mutex_t rw_lock;
extern pthread_mutex_t mutex;

extern struct node *head;

extern char *server_MOTD;


/*
 *Main thread for each client.  Receives all messages
 *and passes the data off to the correct function.  Receives
 *a pointer to the file descriptor for the socket the thread
 *should listen on
 */

char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

void *client_receive(void *ptr) {
   int client = *(int *) ptr;  // socket
  
   int received, i;
   char buffer[MAXBUFF], sbuffer[MAXBUFF];  //data buffer of 2K  
   char tmpbuf[MAXBUFF];  //data temp buffer of 1K  
   char cmd[MAXBUFF], username[20];
   char *arguments[80];

   struct node *currentUser;
    
   send(client  , server_MOTD , strlen(server_MOTD) , 0 ); // Send Welcome Message of the Day.

   // Creating the guest user name
  
   sprintf(username,"guest%d", client);
   head = insertFirstU(head, client , username);
   
   // Add the GUEST to the DEFAULT ROOM (i.e. Lobby)

   while (1) {
       
      if ((received = read(client , buffer, MAXBUFF)) != 0) {
      
            buffer[received] = '\0'; 
            strcpy(cmd, buffer);  
            strcpy(sbuffer, buffer);
         
            /////////////////////////////////////////////////////
            // we got some data from a client

            // 1. Tokenize the input in buf (split it on whitespace)

            // get the first token 

             arguments[0] = strtok(cmd, delimiters);

            // walk through other tokens 

             i = 0;
             while( arguments[i] != NULL ) {
                arguments[++i] = strtok(NULL, delimiters); 
                strcpy(arguments[i-1], trimwhitespace(arguments[i-1]));
             } 

             // Arg[0] = command
             // Arg[1] = user or room
             
             /////////////////////////////////////////////////////
             // 2. Execute command: TODO


            if(strcmp(arguments[0], "create") == 0)
            {
               printf("create room: %s\n", arguments[1]); 
              
               // perform the operations to create room arg[1]
              
              
               sprintf(buffer, "\nchat>");
               send(client , buffer , strlen(buffer) , 0 ); // send back to client
            }
            else if (strcmp(arguments[0], "join") == 0)
            {
               printf("join room: %s\n", arguments[1]);  

               // perform the operations to join room arg[1]
              
               sprintf(buffer, "\nchat>");
               send(client , buffer , strlen(buffer) , 0 ); // send back to client
            }
            else if (strcmp(arguments[0], "leave") == 0)
            {
               printf("leave room: %s\n", arguments[1]); 

               // perform the operations to leave room arg[1]

               sprintf(buffer, "\nchat>");
               send(client , buffer , strlen(buffer) , 0 ); // send back to client
            } 
            else if (strcmp(arguments[0], "connect") == 0)
            {
               printf("connect to user: %s \n", arguments[1]);

               // perform the operations to connect user with socket = client from arg[1]

               sprintf(buffer, "\nchat>");
               send(client , buffer , strlen(buffer) , 0 ); // send back to client
            }
            else if (strcmp(arguments[0], "disconnect") == 0)
            {             
               printf("disconnect from user: %s\n", arguments[1]);
               
               // perform the operations to disconnect user with socket = client from arg[1]
                
               sprintf(buffer, "\nchat>");
               send(client , buffer , strlen(buffer) , 0 ); // send back to client
            }                  
            else if (strcmp(arguments[0], "rooms") == 0)
            {
                printf("List all the rooms\n");
              
                // must add put list of users into buffer to send to client
       
              
                strcat(buffer, "\nchat>");
                send(client , buffer , strlen(buffer) , 0 ); // send back to client                            
            }   
            else if (strcmp(arguments[0], "users") == 0)
            {
                printf("List all the users\n");
              
                // must add put list of users into buffer to send to client
                
                strcat(buffer, "\nchat>");
                send(client , buffer , strlen(buffer) , 0 ); // send back to client
            }                           
            else if (strcmp(arguments[0], "login") == 0)
            {
                
                //rename their guestID to username. Make sure any room or DMs have the updated username.
                
                sprintf(buffer, "\nchat>");
                send(client , buffer , strlen(buffer) , 0 ); // send back to client
            } 
            else if (strcmp(arguments[0], "help") == 0 )
            {
                sprintf(buffer, "login <username> - \"login with username\" \ncreate <room> - \"create a room\" \njoin <room> - \"join a room\" \nleave <room> - \"leave a room\" \nusers - \"list all users\" \nrooms -  \"list all rooms\" \nconnect <user> - \"connect to user\" \nexit - \"exit chat\" \n");
                send(client , buffer , strlen(buffer) , 0 ); // send back to client 
            }
            else if (strcmp(arguments[0], "exit") == 0 || strcmp(arguments[0], "logout") == 0)
            {
    
                //Remove the initiating user from all rooms and direct connections, then close the socket descriptor.
                close(client);
            }                         
            else { 
                 /////////////////////////////////////////////////////////////
                 // 3. sending a message
           
                 // send a message in the following format followed by the promt chat> to the appropriate receipients based on rooms, DMs
                 // ::[userfrom]> <message>
              
                 sprintf(tmpbuf,"\n::%s> %s\nchat>", "PUTUSERFROM", sbuffer);
                 strcpy(sbuffer, tmpbuf);
                       
                 currentUser = head;
                 while(currentUser != NULL) {
                     
                     if(client != currentUser->socket){  // dont send to yourself 
                       
                         send(currentUser->socket , sbuffer , strlen(sbuffer) , 0 ); 
                     }
                     currentUser = currentUser->next;
                 }
          
            }
 
         memset(buffer, 0, sizeof(1024));
      }
   }
   return NULL;
}