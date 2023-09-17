#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include "msg.h"

void Usage(char *progname);
int Listen(char *portnum, int *sock_family);
typedef struct client_args {
  int c_fd;
  struct sockaddr *addr;
  size_t addrlen;
  int sock_family;
} client_args;
void* HandleClient(void *arg);

int put(struct record *r, int32_t *fd);
int get(int32_t id, char r[MAX_NAME_LENGTH], int32_t *fd);

pthread_mutex_t lock;

int main(int argc, char **argv) 
{
  // Expect the port number as a command line argument.
  if (argc != 2) {
    Usage(argv[0]);
  }

  if(pthread_mutex_init(&lock, NULL) != 0) {
    printf("Error creating thread lock.\n");
    return EXIT_FAILURE;
  }

  int sock_family;
  int listen_fd = Listen(argv[1], &sock_family);
  if (listen_fd <= 0) {
    // We failed to bind/listen to a socket.  Quit with failure.
      printf("Couldn't bind to any addresses.\n");
      return EXIT_FAILURE;
  }

  // Loop forever, accepting a connection from a client and doing
  // an echo trick to it.
  while (1) {
    struct sockaddr_storage caddr;
    socklen_t caddr_len = sizeof(caddr);
    int client_fd = accept(listen_fd,
                           (struct sockaddr *)(&caddr),
                           &caddr_len);
    if (client_fd < 0) {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
        continue;
      printf("Failure on accept:%s \n ", strerror(errno));
      break;
    }

    // arguments from the client to pass into the thread
    client_args client_info;
    client_info.c_fd = client_fd;
    client_info.addr = (struct sockaddr *)(&caddr);
    client_info.addrlen = caddr_len;
    client_info.sock_family = sock_family;

    // create thread and handle client
    pthread_t thread_id;
    int result = pthread_create(&thread_id, NULL, HandleClient, (void*)&client_info);
    if(result != 0) {
      printf("Error creating a new thread.");
    }
  }

  pthread_mutex_destroy(&lock);
  // Close socket
  close(listen_fd);
  return EXIT_SUCCESS;
}


// print the proper usage of the program
void Usage(char *progname) {
  printf("usage: %s port \n", progname);
  exit(EXIT_FAILURE);
}

// function that listens for an incoming client connection
int Listen(char *portnum, int *sock_family) {

  // Populate the "hints" addrinfo structure for getaddrinfo().
  // ("man addrinfo")
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;       // IPv6 (also handles IPv4 clients)
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  // getaddrinfo() returns a list of
  // address structures via the output parameter "result".
  struct addrinfo *result;
  int res = getaddrinfo(NULL, portnum, &hints, &result);

  // Did addrinfo() fail?
  if (res != 0) {
	printf( "getaddrinfo failed: %s", gai_strerror(res));
    return -1;
  }

  // Loop through the returned address structures until we are able
  // to create a socket and bind to one.  The address structures are
  // linked in a list through the "ai_next" field of result.
  int listen_fd = -1;
  struct addrinfo *rp;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    listen_fd = socket(rp->ai_family,
                       rp->ai_socktype,
                       rp->ai_protocol);
    if (listen_fd == -1) {
      // Creating this socket failed.  So, loop to the next returned
      // result and try again.
      printf("socket() failed:%s \n ", strerror(errno));
      listen_fd = -1;
      continue;
    }

    // Configure the socket; we're setting a socket "option."  In
    // particular, we set "SO_REUSEADDR", which tells the TCP stack
    // so make the port we bind to available again as soon as we
    // exit, rather than waiting for a few tens of seconds to recycle it.
    int optval = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    // Try binding the socket to the address and port number returned
    // by getaddrinfo().
    if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      // Bind worked!  Print out the information about what
      // we bound to.
      //PrintOut(listen_fd, rp->ai_addr, rp->ai_addrlen);

      // Return to the caller the address family.
      *sock_family = rp->ai_family;
      break;
    }

    // The bind failed.  Close the socket, then loop back around and
    // try the next address/port returned by getaddrinfo().
    close(listen_fd);
    listen_fd = -1;
  }

  // Free the structure returned by getaddrinfo().
  freeaddrinfo(result);

  // If we failed to bind, return failure.
  if (listen_fd == -1)
    return listen_fd;

  // Success. Tell the OS that we want this to be a listening socket.
  if (listen(listen_fd, SOMAXCONN) != 0) {
    printf("Failed to mark socket as listening:%s \n ", strerror(errno));
    close(listen_fd);
    return -1;
  }

  // Return to the client the listening file descriptor.
  return listen_fd;
}

// thread function that handles the client
void* HandleClient(void *arg) {
  client_args params = *(client_args *)arg; // get the client arguments

  // Print out information about the client.
  printf("\nNew client connection \n");
  // open the database
  int32_t fd = open("str", O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
  if(fd == -1)
    printf("\nError opening database.\n");

  // Loop, reading data and echo'ing it back, until the client
  // closes the connection.
  while (1) {
    struct msg client_msg;
    ssize_t res = read(params.c_fd, &client_msg, sizeof(client_msg));
    if (res == 0) {
      printf("\nA client disconnected.\n");
      break;
    }

    if (res == -1) {
      if ((errno == EAGAIN) || (errno == EINTR))
        continue;
	printf("Error on client socket:%s \n ", strerror(errno));
      break;
    }

    pthread_mutex_lock(&lock);

    // process client request
    switch(client_msg.type) {
      case 1:
        client_msg.type = put(&(client_msg.rd), &fd);
        break;
      case 2:
        client_msg.type = get(client_msg.rd.id, client_msg.rd.name, &fd);
        break;
    }

    pthread_mutex_unlock(&lock);
    
    // send message back to client
    write(params.c_fd, &client_msg, sizeof(client_msg));
  }

  // cleanup after client disconnects
  close(params.c_fd);
  close(fd);

  pthread_exit(NULL);
}

// Put the record in the appropriate offset of the file described by fd
int
put(struct record *r, int32_t *fd)
{

  // seek to the appropriate offset in fd
  lseek(*fd, 0, SEEK_END);

  // write record r to fd
  int res = write(*fd, r, sizeof(*r));

  if(res == -1)
    return 5; // return failure

  return 4; // return success
}

// read the record stored at position index in fd
int
get(int32_t id, char r[MAX_NAME_LENGTH], int32_t *fd)
{
  off_t end = lseek(*fd, 0, SEEK_END); // mark the end of the file
  struct record temp;
  int i = 0;
  // loop through all entries in the database and look for the given id
  while(lseek(*fd, i * sizeof(temp), SEEK_SET) != end) {
    read(*fd, &temp, sizeof(temp));
    if(temp.id == id) {
      strcpy(r, temp.name);
      return 4; // return success
    }
    i++;
  }
  return 5; // return failure
}
