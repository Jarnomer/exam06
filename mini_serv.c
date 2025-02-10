#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>

int maxFd = 0;

fd_set readFds, writeFds, allFds;

char readBuffer[1001];
char writeBuffer[42];

int clientId[424242];
char *clientMsg[424242];
int id = 0;

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

void error() {
  write(2, "Fatal error\n", 12);
  exit(1);
}

void notify(int client, char *msg) {
  for (int fd = 0; fd <= maxFd; fd++) {
    if (FD_ISSET(fd, &writeFds) && fd != client)
      send(fd, msg, strlen(msg), 0);
  }
}

void message(int fd) {
  char *temp;
  sprintf(writeBuffer, "client %d: ", clientId[fd]);
  while(extract_message(&(clientMsg[fd]), &temp)) {
    notify(fd, writeBuffer);
    notify(fd, temp);
    free(temp);
  }
}

void append(int fd) {
  if (fd > maxFd)
    maxFd = fd;
  clientId[fd] = id++;
  FD_SET(fd, &allFds);
  sprintf(writeBuffer, "server: client %d just arrived\n", clientId[fd]);
  notify(fd, writeBuffer);
}

void delete(int fd) {
  sprintf(writeBuffer, "server: client %d just left\n", clientId[fd]);
  notify(fd, writeBuffer);
  FD_CLR(fd, &allFds);
  free(clientMsg[fd]);
  close(fd);
}

int init() {
  maxFd = socket(AF_INET, SOCK_STREAM, 0);
  if (maxFd < 0)
    error();
  FD_ZERO(&allFds);
  FD_SET(maxFd, &allFds);
  return maxFd;
}

int main (int ac, char **av) {
  if (ac != 2) {
    write(2, "Wrong number of arguments\n", 26);
    exit(1);
  }

  int sockfd = init();

  struct sockaddr_in servaddr;
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(2130706433);
  servaddr.sin_port = htons(atoi(av[1]));

  if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
    error();
  if (listen(sockfd, 10) != 0)
    error();

  while(1) {
    readFds = writeFds = allFds;
    if (select(maxFd + 1, &readFds, &writeFds, NULL, NULL) < 0)
      error();
    for (int fd = 0; fd <= maxFd; fd++) {
      if (!FD_ISSET(fd, &readFds))
        continue;
      if (fd == sockfd) {
        socklen_t len = sizeof(servaddr);
        int clientFd = accept(sockfd, (struct sockaddr *)&servaddr, &len);
        if (clientFd >= 0) {
          append(clientFd);
          break;
        }
      } else {
        int readBytes = recv(fd, readBuffer, 1000, 0);
        if (!readBytes) {
          delete(fd);
          break;
        }
        readBuffer[readBytes] = '\0';
        clientMsg[fd] = str_join(clientMsg[fd], readBuffer);
        message(fd);
      }
    }
  }
}
