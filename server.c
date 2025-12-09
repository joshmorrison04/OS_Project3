#include "server.h"

int chat_serv_sock_fd; //server socket

/////////////////////////////////////////////
// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE

int numReaders = 0; // keep count of the number of readers

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;    // mutex lock
pthread_mutex_t rw_lock = PTHREAD_MUTEX_INITIALIZER;  // read/write lock

/////////////////////////////////////////////

const char *server_MOTD = "Thanks for connecting to the BisonChat Server.\n\nchat>";

int main(int argc, char **argv) {

   signal(SIGINT, sigintHandler);
    
   //////////////////////////////////////////////////////
   // create the default room for all clients to join when 
   // initially connecting
   //////////////////////////////////////////////////////
   if (!create_room(DEFAULT_ROOM)) {
       fprintf(stderr, "Error creating default room '%s'\n", DEFAULT_ROOM);
       exit(1);
   }

   // Open server socket
   chat_serv_sock_fd = get_server_socket();

   // get ready to accept connections
   if(start_server(chat_serv_sock_fd, BACKLOG) == -1) {
      printf("start server error\n");
      exit(1);
   }
   
   printf("Server Launched! Listening on PORT: %d\n", PORT);
    
   //Main execution loop
   while(1) {
      //Accept a connection, start a thread
      int new_client = accept_client(chat_serv_sock_fd);
      if(new_client != -1) {
         pthread_t new_client_thread;
         pthread_create(&new_client_thread, NULL, client_receive, (void *)&new_client);
         pthread_detach(new_client_thread);
      }
   }

   close(chat_serv_sock_fd);
   return 0;
}

int get_server_socket(void) {
    int opt = TRUE;   
    int master_socket;
    struct sockaddr_in address; 
    
    //create a master socket  
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) {   
        perror("socket failed");   
        exit(EXIT_FAILURE);   
    }   
     
    //set master socket to allow multiple connections  
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,  
          sizeof(opt)) < 0 ) {   
        perror("setsockopt");   
        exit(EXIT_FAILURE);   
    }   
     
    //type of socket created  
    address.sin_family = AF_INET;   
    address.sin_addr.s_addr = INADDR_ANY;   
    address.sin_port = htons(PORT);   
         
    //bind the socket to localhost port 8888  
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) {   
        perror("bind failed");   
        exit(EXIT_FAILURE);   
    }   

    return master_socket;
}

int start_server(int serv_socket, int backlog) {
   int status = 0;
   if ((status = listen(serv_socket, backlog)) == -1) {
      printf("socket listen error\n");
   }
   return status;
}

int accept_client(int serv_sock) {
   int reply_sock_fd = -1;
   socklen_t sin_size = sizeof(struct sockaddr_storage);
   struct sockaddr_storage client_addr;

   if ((reply_sock_fd = accept(serv_sock,(struct sockaddr *)&client_addr, &sin_size)) == -1) {
      printf("socket accept error\n");
   }
   return reply_sock_fd;
}

/* Handle SIGINT (CTRL+C) */
void sigintHandler(int sig_num) {
    (void)sig_num;  // unused
    printf("\n[Server] Caught SIGINT. Shutting down...\n");

    printf("--------CLOSING ACTIVE USERS--------\n");

    // Use our centralized cleanup (closes all user sockets, frees rooms/users)
    cleanup_all();

    // Destroy locks
    pthread_mutex_destroy(&rw_lock);
    pthread_mutex_destroy(&mutex);

    // Close the listening socket
    close(chat_serv_sock_fd);

    printf("[Server] Shutdown complete. Bye.\n");
    exit(0);
}