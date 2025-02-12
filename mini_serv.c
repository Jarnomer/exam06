#include <string.h>         // headers copied from main
#include <unistd.h>         // OR add from manpages
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>

char *temp;                 // temp to hold extracted message
int maxfd = 0;              // nfds, see select manpage
fd_set rfds, wfds, afds;    // read, write and all fds
char rbuf[1001], wbuf[42];  // read and write buffers for messages
char *msgs[424242];         // arrays to store client messages and ids
int ids[424242];
int id = 0;

// Unmodified function from main
int extract_message(char **buf, char **msg) {
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

// Unmodified function from main
char *str_join(char *buf, char *add) {
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

void fatal_error() {
  write(2, "Fatal error\n", 12);
  exit(1);
}

void notify_clients(int client, char *msg) {
  for (int fd = 0; fd <= maxfd; fd++) {
    if (FD_ISSET(fd, &wfds) && fd != client)
      send(fd, msg, strlen(msg), 0);
  }
}

void send_message(int fd) {
  msgs[fd] = str_join(msgs[fd], rbuf);
  sprintf(wbuf, "client %d: ", ids[fd]);
  while(extract_message(&(msgs[fd]), &temp)) {
    notify_clients(fd, wbuf);
    notify_clients(fd, temp);
    free(temp);
  }
}

void append_client(int fd) {
  if (fd > maxfd)
    maxfd = fd;
  ids[fd] = id++;
  FD_SET(fd, &afds);
  sprintf(wbuf, "server: client %d just arrived\n", ids[fd]);
  notify_clients(fd, wbuf);
}

void delete_client(int fd) {
  sprintf(wbuf, "server: client %d just left\n", ids[fd]);
  notify_clients(fd, wbuf);
  FD_CLR(fd, &afds);
  free(msgs[fd]);
  close(fd);
}

int init_socket() {
  maxfd = socket(AF_INET, SOCK_STREAM, 0);  // copied from main
  if (maxfd < 0)
    fatal_error();
  FD_ZERO(&afds);
  FD_SET(maxfd, &afds);
  return maxfd;
}

int main (int ac, char **av) {
  if (ac != 2) {  // add argc check
    write(2, "Wrong number of arguments\n", 26);
    exit(1);
  }

  int sockfd = init_socket();  // replace socket creation

  struct sockaddr_in servaddr;  // copied from main, replace port
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(2130706433);
  servaddr.sin_port = htons(atoi(av[1]));

  if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
    fatal_error();  // if statements copied from main, change error handling
  if (listen(sockfd, 10) != 0)
    fatal_error();

  while(1) {
    rfds = wfds = afds;  // set read and write fds to all active fds
    if (select(maxfd + 1, &rfds, &wfds, NULL, NULL) < 0)
      fatal_error();  // needs maxfd + 1, exit on error
    for (int fd = 0; fd <= maxfd; fd++) {
      if (!FD_ISSET(fd, &rfds)) {  // if not ready to read
        continue;
      } else if (fd == sockfd) {  // if new client
        socklen_t len = sizeof(servaddr);  // from main, has to be socklen_t
        int connfd = accept(sockfd, (struct sockaddr *)&servaddr, &len);  // from main
        if (connfd >= 0)
          append_client(connfd);
      } else {
        int bytes = recv(fd, rbuf, sizeof(rbuf) - 1, 0);
        if (bytes <= 0) {  // client disconnected
          delete_client(fd);
        } else {
          rbuf[bytes] = '\0';  // terminate buffer
          send_message(fd);
        }
      }
    }
  }
}
