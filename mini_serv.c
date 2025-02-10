#include <string.h>  // headers copied from main
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>

int maxFd = 0;  // ndfs, see select manpage

// read, write and all fds
fd_set readFds, writeFds, allFds;

// buffers to read and write messages
char readBuf[1001];
char writeBuf[42];

// arrays to store client ids and messages
char *clientMsg[424242];
int  clientId[424242];
int  id = 0;

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

void error() {
  write(2, "Fatal error\n", 12);
  exit(1);
}

void notify(int clientFd, char *msg) {
  for (int fd = 0; fd <= maxFd; fd++) {
    if (FD_ISSET(fd, &writeFds) && fd != clientFd)
      send(fd, msg, strlen(msg), 0);
  }
}

// void message(int fd) {
//   char *temp, *msg;
//   sprintf(writeBuf, "client %d: ", clientId[fd]);
//   while(extract_message(&(clientMsg[fd]), &temp)) {
//     msg = str_join(writeBuf, temp);
//     notify(fd, msg);
//     free(msg);
//     free(temp);
//   }
// }

void message(int fd) {
  char *temp;
  sprintf(writeBuf, "client %d: ", clientId[fd]);
  while(extract_message(&(clientMsg[fd]), &temp)) {
    notify(fd, writeBuf);
    notify(fd, temp);
    free(temp);
  }
}

void append(int fd) {
  if (fd > maxFd) 
    maxFd = fd;
  clientId[fd] = id++;
  FD_SET(fd, &allFds);
  char *msg = "server: client %d just arrived\n";
  sprintf(writeBuf, msg, clientId[fd]);
  notify(fd, writeBuf);
}

void delete(int fd) {
  char *msg = "server: client %d just left\n";
  sprintf(writeBuf, msg, clientId[fd]);
  notify(fd, writeBuf);
  free(clientMsg[fd]);
  close(fd);
  FD_CLR(fd, &allFds);
}

int init() {
  maxFd = socket(AF_INET, SOCK_STREAM, 0);  // from main
  if (maxFd < 0)
    error();
  FD_ZERO(&allFds);
  FD_SET(maxFd, &allFds);
  return maxFd;
}

int main (int argc, char **argv) {

  if (argc != 2) {  // added argc check
    write(2, "Wrong number of arguments\n", 26);
    exit(1);
  }

  int sockfd = init();  // replaced socket creation

  struct sockaddr_in servaddr;  // from main, change port
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(2130706433);
  servaddr.sin_port = htons(atoi(argv[1]));

  if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
    error();  // if statements from main, replaced error handling
  if (listen(sockfd, 10) != 0)
    error();

  // while(1) {  // replaced the rest of main
  //   readFds = writeFds = allFds;  // assign all active fds to read and write fds
  //   if (select(maxFd + 1, &readFds, &writeFds, NULL, NULL) < 0)
  //     error();  // select needs maxFd + 1, exit on error 
  //   for (int fd = 0; fd <= maxFd; fd++) {
  //     if (!FD_ISSET(fd, &readFds)) {  // not ready to read
  //       continue;
  //     } else if (fd == sockfd) {  // new client
  //       socklen_t len = sizeof(servaddr);  // from main, has to be socklen_t
  //       int connfd = accept(sockfd, (struct sockaddr *)&servaddr, &len);
  //       if (connfd >= 0)  // no exit on error
  //         append(connfd);
  //     } else {
  //       int bytesRead = recv(fd, readBuf, 1000, 0); // 1001 - 1 = 1000
  //       if (!bytesRead) {  // client disconnected
  //         delete(fd);
  //         continue;
  //       } else {
  //         readBuf[bytesRead] = '\0';  // terminate buffer
  //         clientMsg[fd] = str_join(clientMsg[fd], readBuf);
  //         message(fd);
  //       } 
  //     }
  //   }
  // }

  while(1) {
    readFds = writeFds = allFds;
    if (select(maxFd + 1, &readFds, &writeFds, NULL, NULL) < 0)
      error();
    for (int fd = 0; fd <= maxFd; fd++) {
      if (!FD_ISSET(fd, &readFds))
        continue;
      if (fd == sockfd) {
        socklen_t len = sizeof(servaddr);
        int connfd = accept(sockfd, (struct sockaddr *)&servaddr, &len);
        if (connfd >= 0) {
          append(connfd);
          break;
        }
      } else {
        int bytesRead = recv(fd, readBuf, 1000, 0);
        if (!bytesRead) {
          delete(fd);
          break;
        }
        readBuf[bytesRead] = '\0';
        clientMsg[fd] = str_join(clientMsg[fd], readBuf);
        message(fd);
      }
    }
  }
}
