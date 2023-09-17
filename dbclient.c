#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <inttypes.h>
#include "msg.h"

#define BUF 256

void Usage(char *progname);

int LookupName(char *name,
                unsigned short port,
                struct sockaddr_storage *ret_addr,
                size_t *ret_addrlen);

int Connect(const struct sockaddr_storage *addr,
             const size_t addrlen,
             int *ret_fd);

struct record* put();
struct record* get();

int main(int argc, char **argv) {
  // expect two arguments
  if (argc != 3) {
    Usage(argv[0]);
  }
  
  // print appropriate usage if no port is provided
  unsigned short port = 0;
  if (sscanf(argv[2], "%hu", &port) != 1) {
    Usage(argv[0]);
  }

  // Get an appropriate sockaddr structure.
  struct sockaddr_storage addr;
  size_t addrlen;
  if (!LookupName(argv[1], port, &addr, &addrlen)) {
    Usage(argv[0]);
  }

  // Connect to the remote host.
  int socket_fd;
  if (!Connect(&addr, addrlen, &socket_fd)) {
    Usage(argv[0]);
  }

  int8_t choice, flag;
  flag = 1;
  // loop until the user decides to exit
  while(flag) {
    // prompt the user and call appropriate function, put or get
    struct msg m;
    printf("Enter your choice (1 to put, 2 to get, 0 to quit): ");
    scanf("%" SCNd8 "%*c", &choice);
    switch (choice) {
      case 1: 
        m.rd = *(put());
        m.type = 1;
        break;
      case 2:
        m.rd = *(get());
        m.type = 2;
        break;
      default:
        flag = 0;
    }

    // Send the message to the remote host.
    while (1) {
      int wres = write(socket_fd, &m, sizeof(m)); // write the message to the server
      // exit if something goes wrong
      if (wres == 0) {
      printf("socket closed prematurely \n");
        close(socket_fd);
        return EXIT_FAILURE;
      }
      if (wres == -1) {
        if (errno == EINTR)
          continue;
        printf("socket write failure \n");
        close(socket_fd);
        return EXIT_FAILURE;
      }
      break;
    }

    // Read the message from the server
    struct msg server_m;
    while (1) {
      int res = read(socket_fd, &server_m, sizeof(server_m)); // store server message in server_m
      // exit if something goes wrong
      if (res == 0) {
        printf("socket closed prematurely \n");
        close(socket_fd);
        return EXIT_FAILURE;
      }
      if (res == -1) {
        if (errno == EINTR)
          continue;
        printf("socket read failure \n");
        close(socket_fd);
        return EXIT_FAILURE;
      }
      // Process message from server
      switch(choice) {
        case 1: // print put result
	  if(server_m.type == 4)
	    printf("Put success.\n");
	  else
	    printf("Put failure.\n");
	  break;
	case 2: // print get result
	  if(server_m.type == 4) {
	    printf("name: %s\n", server_m.rd.name);
	    printf("id: %d\n", server_m.rd.id);
	  } else {
	    printf("No record found with that id.\n");
	  }
	  break;
	  
      }
      break;
    }
  }

  // Clean up.
  close(socket_fd);
  return EXIT_SUCCESS;
}

// prints the proper usage of the program
void 
Usage(char *progname) {
  printf("usage: %s  hostname port \n", progname);
  exit(EXIT_FAILURE);
}

// Looks up DNS name
int 
LookupName(char *name,
                unsigned short port,
                struct sockaddr_storage *ret_addr,
                size_t *ret_addrlen) {
  struct addrinfo hints, *results;
  int retval;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  // Do the lookup by invoking getaddrinfo().
  if ((retval = getaddrinfo(name, NULL, &hints, &results)) != 0) {
    printf( "getaddrinfo failed: %s", gai_strerror(retval));
    return 0;
  }

  // Set the port in the first result.
  if (results->ai_family == AF_INET) {
    struct sockaddr_in *v4addr =
            (struct sockaddr_in *) (results->ai_addr);
    v4addr->sin_port = htons(port);
  } else if (results->ai_family == AF_INET6) {
    struct sockaddr_in6 *v6addr =
            (struct sockaddr_in6 *)(results->ai_addr);
    v6addr->sin6_port = htons(port);
  } else {
    printf("getaddrinfo failed to provide an IPv4 or IPv6 address \n");
    freeaddrinfo(results);
    return 0;
  }

  // Return the first result.
  assert(results != NULL);
  memcpy(ret_addr, results->ai_addr, results->ai_addrlen);
  *ret_addrlen = results->ai_addrlen;

  // Clean up.
  freeaddrinfo(results);
  return 1;
}

int 
Connect(const struct sockaddr_storage *addr,
             const size_t addrlen,
             int *ret_fd) {
  // Create the socket.
  int socket_fd = socket(addr->ss_family, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    printf("socket() failed: %s", strerror(errno));
    return 0;
  }

  // Connect the socket to the remote host.
  int res = connect(socket_fd,
                    (const struct sockaddr *)(addr),
                    addrlen);
  if (res == -1) {
    printf("connect() failed: %s", strerror(errno));
    return 0;
  }

  *ret_fd = socket_fd;
  return 1;
}

// Prompts the user to enter data for the record and creates the record to be sent in the message to the server
struct record* put() {
	struct record m;
	void* rd = &m;

	printf("Enter the name: ");	
	
	// fgets doesnt remove newline
	fgets(m.name, MAX_NAME_LENGTH, stdin);
	*(m.name + strlen(m.name) - 1) = '\0';

	printf("Enter the id: ");
	// read student id from stdin
	// store it in m.id
	scanf("%d", &m.id);

  return rd;
}

// Prompts the user to enter id of the record they would like to lookup
struct record* get() {
  struct record m;
  void* rd = &m;

  printf("Enter the id: ");
  scanf("%d", &m.id);

  return rd;
}
