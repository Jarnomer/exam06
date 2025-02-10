#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>

int    nfds = 0;
fd_set rfds, wfds, afds;
int  bsize = 1000;
char rbuf[1001], wbuf[42];
int  ids[424242];
char *msgs[424242];
int  count = 0;

// Unmodified function copied from main
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

// Unmodified function copied from main
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

void notify(int current, char *msg) {
  for (int fd = 0; fd <= nfds; fd++) {
    if (FD_ISSET(fd, &wfds) && fd != current)
      send(fd, msg, strlen(msg), 0);
  }
}

void send_message(int fd) {
  char *temp;
  while(extract_message(&(msgs[fd]), &temp)) {
    sprintf(wbuf, "client %d: ", ids[fd]);
    notify(fd, wbuf);
    notify(fd, temp);
    free(temp);
  }
}

void new_client(int fd) {
  nfds = fd > nfds ? fd : nfds;
  ids[fd] = count++;
  msgs[fd] = NULL;
  FD_SET(fd, &afds);
  sprintf(wbuf, "server: client %d just arrived\n", ids[fd]);
  notify(fd, wbuf);
}

void remove_client(int fd) {
  sprintf(wbuf, "server: client %d just left\n", ids[fd]);
  notify(fd, wbuf);
  FD_CLR(fd, &afds);
  free(msgs[fd]);
  close(fd);
}

int new_socket() {
  nfds = socket(AF_INET, SOCK_STREAM, 0);
  if (nfds < 0)
    fatal_error();
  FD_SET(nfds, &afds);
  return nfds;
}

int main (int ac, char **av) {
  if (ac != 2) {
    write(2, "Wrong number of arguments\n", 26);
    exit(1);
  }

  FD_ZERO(&afds);
  int sockfd = new_socket();

  struct sockaddr_in servaddr;
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(2130706433);
  servaddr.sin_port = htons(atoi(av[1]));

  if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
    fatal_error();
  if (listen(sockfd, 10) != 0)
    fatal_error();

  while(1) {
    rfds = wfds = afds;
    if (select(nfds + 1, &rfds, &wfds, NULL, NULL) < 0)
      fatal_error();
    for (int fd = 0; fd <= nfds; fd++) {
      if (!FD_ISSET(fd, &rfds))
        continue;
      if (fd == sockfd) {
        socklen_t len = sizeof(servaddr);
        int connfd = accept(sockfd, (struct sockaddr *)&servaddr, &len);
        if (connfd >= 0) {
          new_client(connfd);
          break;
        }
      } else {
        int bytes = recv(fd, rbuf, bsize, 0);
        if (!bytes) {
          remove_client(fd);
          break;
        }
        rbuf[bytes] = '\0';
        msgs[fd] = str_join(msgs[fd], rbuf);
        send_message(fd);
      }
    }
  }

  // while(1) {
  //   rfds = wfds = afds;
  //   if (select(nfds + 1, &rfds, &wfds, NULL, NULL) < 0)
  //     fatal_error();
  //   for (int fd = 0; fd <= nfds; fd++) {
  //     if (!FD_ISSET(fd, &rfds)) {
  //       continue;
  //     } else if (fd == sockfd) { // changed to else if
  //       socklen_t len = sizeof(servaddr);
  //       int connfd = accept(sockfd, (struct sockaddr *)&servaddr, &len);
  //       if (connfd >= 0)
  //         new_client(connfd);  // removed break here
  //     } else {
  //       int bytes = recv(fd, rbuf, bsize, 0);
  //       if (bytes) {  // flipped !bytes
  //         rbuf[bytes] = '\0';
  //         msgs[fd] = str_join(msgs[fd], rbuf);
  //         send_message(fd);
  //       } else {  // added else
  //         remove_client(fd);
  //       } 
  //     }
  //   }
  // }
}
