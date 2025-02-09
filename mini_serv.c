#include <string.h>                  // headers copied from main
#include <unistd.h>                  // don't need errno, netdb or socket
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>

int    nfds = 0;                     // number of current active fds, see select manpage
fd_set rfds, wfds, afds;             // read, write and all fds, see select manpage
int  bsize = 1000;                   // how many bytes are read on each receive
char rbuf[1001], wbuf[42];           // read and write buffers, rbuf needs to fit `\0`
int  ids[424242];                    // array to hold client ids for messages
char *msgs[424242];                  // array of messages corresponding the ids
int  count = 0;                      // used to count current id for ids and msgs

int extract_message(char **buf, char **msg) {  // Unmodified function copied from main
  char *newbuf;
  int  i;

  *msg = 0;
  if (*buf == 0)
    return (0);
  i = 0;
  while ((*buf)[i]) {
    if ((*buf)[i] == '\n') {
      newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
      if (newbuf == 0)
        return (-1);
      strcpy(newbuf, *buf + i + 1);
      *msg = *buf;
      (*msg)[i + 1] = 0;
      *buf = newbuf;
      return (1);
    }
    i++;
  }
  return (0);
}

char *str_join(char *buf, char *add) {  // Unmodified function copied from main
  char *newbuf;
  int  len;

  if (buf == 0)
    len = 0;
  else
    len = strlen(buf);
  newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
  if (newbuf == 0)
    return (0);
  newbuf[0] = 0;
  if (buf != 0)
    strcat(newbuf, buf);
  free(buf);
  strcat(newbuf, add);
  return (newbuf);
}

void fatal_error() {                              // function to exit on error
  write(2, "Fatal error\n", 12);                  // write the error message
  exit(1);                                        // call exit()
}

void notify(int current, char *msg) {             // function to notify other clients
  for (int fd = 0; fd <= nfds; fd++) {            // iterate through all active fds
    if (FD_ISSET(fd, &wfds) && fd != current)     // check that fd is in wfds and not current
      send(fd, msg, strlen(msg), 0);              // send message to each client
  }
}

void send_message(int fd) {                       // function send messages to clients
  char *temp;                                     // temporary pointer to store extracted message
  while(extract_message(&(msgs[fd]), &temp)) {    // extract the message in while loop
    sprintf(wbuf, "client %d: ", ids[fd]);        // copy string from subject
    notify(fd, wbuf);                             // first send the client message
    notify(fd, temp);                             // then send the extracted message
    free(temp);                                   // free temp allocated by extract_message()
  }
}

void new_client(int fd) {                         // function to register new client
  nfds = fd > nfds ? fd : nfds;                   // adjust nfds if fd is bigger
  ids[fd] = count++;                              // store client id into ids, starts from zero
  msgs[fd] = NULL;                                // initialize client message to NULL
  FD_SET(fd, &afds);                              // macro to add fd to afds, see select manpage
  sprintf(wbuf, "server: client %d just arrived\n", ids[fd]); // copy string from subject
  notify(fd, wbuf);                               // send notifications using the id and message
}

void remove_client(int fd) {                      // function to remove client, write msg to wbuf with sprintf
  sprintf(wbuf, "server: client %d just left\n", ids[fd]); // copy string from subject
  notify(fd, wbuf);                               // send notifications using the id and wbuf
  FD_CLR(fd, &afds);                              // macro to remove fd from afds, see select manpage
  free(msgs[fd]);                                 // free message allocated by str_join()
  close(fd);                                      // close fd to prevent leaks
}

int new_socket() {                                // function to initialize the socket
  nfds = socket(AF_INET, SOCK_STREAM, 0);         // copied from main, replaced sockfd
  if (nfds < 0)                                   // check if systemcall fails
    fatal_error();                                // exit on error
  FD_SET(nfds, &afds);                            // macro to add nfds to afds, see select manpage
  return nfds;                                    // return new created socket fd
}

int main (int ac, char **av) {                    // changed main to take arguments
  if (ac != 2) {                                  // added check as per subject criteria
    write(2, "Wrong number of arguments\n", 26);  // write the error message
    exit(1);                                      // call exit()
  }

  FD_ZERO(&afds);                                 // macro to clear all fds, see select manpage
  int sockfd = new_socket();                      // replaced main socket creation with function

  struct sockaddr_in servaddr;                    // copied from main, removed unused cli
  servaddr.sin_family = AF_INET;                  // copied from main
  servaddr.sin_addr.s_addr = htonl(2130706433);   // copied from main
  servaddr.sin_port = htons(atoi(av[1]));         // copied from main, replaced port with atoi(av[1])

  if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)  // see below
    fatal_error();                                // unmodified if statements copied from main
  if (listen(sockfd, 10) != 0)                    // replaced sysmtemcall failures with error function
    fatal_error();                                // writes error message and calls exit()

  while(1) {                                      // replaced the rest of the main with while loop
    rfds = wfds = afds;                           // assign active fds to read and write fds
    if (select(nfds + 1, &rfds, &wfds, NULL, NULL) < 0)  // use select, check return value, exit on error
      fatal_error();                              // exit on error before accepting connections
    for (int fd = 0; fd <= nfds; fd++) {          // iterate through all active fds
      if (!FD_ISSET(fd, &rfds))                   // macro to check if fd is found among read fds
        continue;                                 // continue to next fd if current fd is reading
      if (fd == sockfd) {                         // check if current fd is new client
        socklen_t len = sizeof(servaddr);         // replaced cli with servaddr, chanted type to socklen_t
        int connfd = accept(sockfd, (struct sockaddr *)&servaddr, &len);  // same as above, copied from main
        if (connfd >= 0) {                        // check if accecpt was successful
          new_client(connfd);                     // add connfd as new client
          break;                                  // break to run select with new fd
        }                                         // NOTE: accepting connections, no exit on error 
      } else {                                    // if not new client
        int bytes = recv(fd, rbuf, bsize, 0);     // store received bytes
        if (!bytes) {                             // if zero bytes were received
          remove_client(fd);                      // use function to remove fd
          break;                                  // break to run select without fd
        }                                         // if something was received
        rbuf[bytes] = '\0';                       // terminate read buffer
        msgs[fd] = str_join(msgs[fd], rbuf);      // join buffer with current message
        send_message(fd);                         // send message to other clients
      }
    }
  }
}
