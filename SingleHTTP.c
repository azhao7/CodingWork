#include "httpserver.h"
#include <string.h>
#define BUFFER_LEN 1024
void generateResponse(int fd, int messageType, int contentLength) {
  int shouldClose = 1;
  if (messageType == CREATED) {
    char message[MESSAGESIZE];
    int count = 0;
    sprintf(message, "201 Created\r\nContent-Length:%d\r\n\r\n",
            count);
    send(fd, message, strlen(message), 0);
    //close(fd);
  } else if (messageType == BADREQ) {
    char message[MESSAGESIZE];
    sprintf(message, "HTTP/1.1 400 bad Request\r\nContent-Length: 0\r\n\r\n");
    send(fd, message, strlen(message), 0);
    //close(fd);
  } else if (messageType == OK) {
    printf("head is here\n");
    char message[MESSAGESIZE];
    char http[ARGUMENTSIZE] = "HTTP/1.1";
    sprintf(message,
            "%s 200 OK\r\nContent-Length: %d\r\n\r\n", http,
            contentLength);
    send(fd, message, strlen(message), 0);
    shouldClose = 0;
  } else if (messageType == FORBIDDEN) {
    char message[MESSAGESIZE];

    sprintf(message, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");
    send(fd, message, strlen(message), 0);
    //close(fd);
  } else if (messageType == NOTFOUND) {
    char message[MESSAGESIZE];
    sprintf(message,
            "HTTP/1.1 404 File not found\r\nContent-Length: 0\r\n\r\n");
    send(fd, message, strlen(message), 0);
  } else if (messageType == SERVERERR) {
    char message[MESSAGESIZE];
    sprintf(message,
            "HTTP/1.1 500 Internal server Error\r\nContent-Length: 0\r\n\r\n");
    send(fd, message, strlen(message), 0);

  }
  if(shouldClose){
    close(fd);
  }
}
int validateFileName(int client_sockd, char* fileName){
  printf("%s\n",fileName);

  if (strlen(fileName) > 27) {
    printf("file name is > 27 characters\n");
    //generateResponse(client_sockd, BADREQ, 0);
    return -1;
  }
  for (size_t i = 0; i < (size_t)strlen(fileName); i++) {
    if ((!isalnum(fileName[i])) && fileName[i] != '_' &&
        fileName[i] != '-') {
      return -2;
    }
  }
  return 0;
}
ssize_t validateHeader(int client_sockd,char* buffer){
  ssize_t bytesRead = 0;
  int headerFound = 0;
  size_t headerCount = 0;
  while (headerFound == 0) {
    if (headerCount > 4096) {
      generateResponse(client_sockd, BADREQ, 0);
      close(client_sockd);
    } else {
      printf("reading\n");
      bytesRead = recv(client_sockd, buffer + headerCount, 4096-headerCount, 0);
      if(bytesRead<=0){
        return bytesRead;
      }

      headerCount += bytesRead;
      char *test = strstr(buffer, "\r\n\r\n");
      if (test) {
        headerFound = 1;
        int pos = test-buffer;
        printf("this is the position %d\n",pos);
        printf("this is the buffer length: %d\n",strlen(buffer));
        if(pos+4<strlen(buffer)){
          send(client_sockd,buffer+pos+4,strlen(buffer)-(pos+4),0);
        }
        return bytesRead;
      }
    }
  }
  return -1;
}
void Read_Write(int fd, int socket, ssize_t fileSize,int valid) {
  printf("%d\n",fd);
  if(valid < 0){
    printf("here\n");
    char* buffer[BUFFER_LEN];
    int count =0;
    while(count<fileSize){
      ssize_t readBytes = read(socket, buffer, BUFFER_LEN);
      count+=readBytes;
      if(readBytes<BUFFER_LEN){
        generateResponse(socket,BADREQ,0);
        close(socket);
        return;
      }
    }
  }
  //int fd = open(fileName, O_CREAT | O_RDWR, 0777);
  int count = 0;
  // printf("in read_write\n");
  char buffer[BUFFER_LEN];
  while (count < fileSize) {
    ssize_t readBytes = read(socket, buffer, BUFFER_LEN);
    if (readBytes == 0) {
      if (close(fd) < 0) {
        warn("close");
        return;
      }
    } else if (readBytes < 0) {
      generateResponse(socket, BADREQ, 0);
      warn("read");
      return;
    } else {
      ssize_t total = 0;
      ssize_t byteWritten = 0;
      while (total < readBytes) {
        if ((byteWritten = write(fd, buffer, readBytes - total)) < 0) {
          printf("%d\n",fd);
          warn("write");
          close(socket);
          break;
        }
        total += byteWritten;
      }
      count += readBytes;
    }
    if (readBytes < BUFFER_LEN) {
      generateResponse(socket, CREATED, 0);
      return;
    }
  }
}
void sendPutRequest(int client_sockd, char *fileName, ssize_t fileSize) {
  if (fileSize == -1) {
    generateResponse(client_sockd, BADREQ, 0);
    return;
  }
  int valid = validateFileName(client_sockd,fileName);
  if(valid < 0){
    generateResponse(client_sockd,BADREQ,0);
    close(client_sockd);
    return;
  }
  int fd1 = open(fileName, O_RDONLY);
  if(fd1<0){
   if (errno == EACCES) {
     printf("this is in permissions\n");
      generateResponse(client_sockd, FORBIDDEN, 0);
    }
  }
  if (fd1 > 0) {
    remove(fileName);
    close(fd1);
  }


  int fd2 = open(fileName, O_CREAT | O_RDWR, 0777);;
  printf("%d\n",fd2);
  Read_Write(fd2, client_sockd, fileSize,valid);
   printf("done\n");
}
void sendGetRequest(int client_sockd, char *fileName) {
  int valid = validateFileName(client_sockd,fileName);
  printf("valid is: %d\n",valid);
  if(valid < 0){
    generateResponse(client_sockd,BADREQ,0);
    close(client_sockd);
    return;
  }

  int fd2 = open(fileName, O_RDONLY);
  // printf("%s\n",fileName);
  if (fd2 < 0) {

    if (errno == EACCES) {
      generateResponse(client_sockd, FORBIDDEN, 0);
    } else if (errno == ENOENT) {
      generateResponse(client_sockd, NOTFOUND, 0);
    }
    warn("Open");
    return;
  }
  size_t fileSize = 0;
  struct stat st;
  if (stat(fileName, &st) == 0)
    fileSize = (st.st_size);
  // send OK message here
  //close(fd2);
  generateResponse(client_sockd, OK, fileSize);
  Read_Write(client_sockd, fd2, fileSize,0);
  close(client_sockd);
}
void sendHeadRequest(int client_sockd, char *fileName) {
  printf("in head\n");
  printf("%s\n",fileName);
  int fd2 = open(fileName, O_RDONLY);
  printf("%d\n",fd2);
  int valid = validateFileName(client_sockd,fileName);
  if(valid < 0){
    generateResponse(client_sockd,BADREQ,0);
    close(client_sockd);
    return;
  }
  // printf("%s\n",fileName);
  if (fd2 < 0) {
    if (errno == ENOENT) {
      printf("error\n");
      generateResponse(client_sockd, NOTFOUND, 0);
    } else if (errno == EACCES) {
      generateResponse(client_sockd, FORBIDDEN, 0);
    }
    warn("Open Header");
    return;
  }
  ssize_t fileSize = 0;
  struct stat st;
  if (stat(fileName, &st) == 0)
    fileSize = (st.st_size);
  // send OK message here
  printf("%zd\n",fileSize);
  generateResponse(client_sockd, OK, fileSize);
  close(client_sockd);
}
void stringParser(char *buffer, char *reqType, char *fileName, char *http,
                  ssize_t *Length, int fd) {
  //           int count = 1;
  char *pch = NULL;
  char placeholder[40];
  sscanf(buffer, "%s %s %s", reqType, fileName, http);
  printf("%s\n", reqType);
  pch = strtok(buffer, "\r\n");
  if(strcmp(reqType,"GET")==0){
    printf("request: GET\n");
    return;
  }
  else if(strcmp(reqType,"HEAD")==0){
    printf("request: GET\n");
    return;
  }
//loop taken from stackoverflow how to parse http headers
  int found = 0;
  while (pch != NULL) {
    sscanf(pch, "%s", placeholder);
    printf("%s\n",placeholder);
    if(strcmp(placeholder, "Content-Length:") == 0&& found == 1){
      printf("here second occurrence \n");
      generateResponse(fd,BADREQ,0);
    }
    if (strcmp(placeholder, "Content-Length:") == 0&&!found) {
      sscanf(pch, "%s%ld", placeholder, Length);
      found = 1;
      //continue;

    //  return;
    }

    pch = strtok(NULL, "\r\n");
  }
  if(found == 1){
    printf("here\n");
    return;
  }
  // sscanf(buffer,"%s\n%s\n%s\n",reqType,fileName,http);
  // printf("%s\n",reqType);
  // printf("%s\n",fileName);
  // printf("%d\n",Length);
  generateResponse(fd, BADREQ, 0);
  return;
}
int main(int argc, char *argv[]) {
  if (argc < 2) {
    warn("too few arguments");
    return -1;
  }
  char *port = argv[1];
  for(size_t i = 0; i < strlen(port); i++){
    if(!isalnum(port[i])){
      warn("error");
      return -1;
    }
  }
  int p = atoi(port);
  if(p<=1024){
    warn("error");
    return -1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(port));
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  socklen_t ai_addrlen = sizeof server_addr;
  int server_sockd = socket(AF_INET, SOCK_STREAM, 0);
  int enable = 1;
  // source
  // https://stackoverflow.com/questions/2876024/linux-is-
  // there-a-read-or-recv-from-socket-with-timeout
  struct timeval tv;
  tv.tv_sec = 5; /* 5 Secs Timeout */
  // int ret=setsockopt(server_sockd, SOL_SOCKET,
  // SO_REUSEADDR,&enable,sizeof(enable));

  int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable,
                       sizeof(enable));
  if (ret == -1) {
    warn("setsocketopt");
    return -1;
  }
  ret = bind(server_sockd, (struct sockaddr *)&server_addr, ai_addrlen);
  if (ret == -1) {
    warn("bind");
    return -1;
  }
  ret = listen(server_sockd, 5);
  if (ret == -1) {
    warn("listen");
    return -1;
  }
  while (1) {
    struct sockaddr client_addr;
    socklen_t client_addrlen;
    int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
    if (client_sockd < 0) {
      warn("accept");
      return -1;
    }
    char buffer[BUFFERSIZE + 1] = "";
    char fileName[ARGUMENTSIZE] = "";
    char requestType[ARGUMENTSIZE] = "";
    ssize_t fileSize = -1;
    char http[ARGUMENTSIZE] = "";
    // read from clientSocket for header

    ssize_t bytesRead = validateHeader(client_sockd,buffer);
    if (bytesRead < 0) {
      warn("read error");
      close(client_sockd);
    } else {
      stringParser(buffer, requestType, fileName, http, &fileSize,
                   client_sockd);

      if(strcmp(http,"HTTP/1.1")!=0){
        generateResponse(client_sockd,BADREQ,0);
      }
      //printf("%s\n", requestType);
      if (fileName[0] == '/') {
        memmove(fileName, fileName + 1, strlen(fileName));
      }else{
        generateResponse(client_sockd,BADREQ,0);
      }
      if (strcmp(fileName, "") == 0 || strcmp(requestType, "") == 0 ||
          strcmp(http,"") == 0) {

        printf("error filename\n");
        generateResponse(client_sockd, BADREQ, 0);
        close(client_sockd);
        //return -1;
        //return;
      }
      if (strcmp(requestType, "GET") == 0) {
        sendGetRequest(client_sockd, fileName);
      } else if (strcmp(requestType, "PUT") == 0) {
        // printf("this is the socket Number: %d\n",client_sockd);
        sendPutRequest(client_sockd, fileName, fileSize);
      } else if (strcmp(requestType, "HEAD") == 0) {
        sendHeadRequest(client_sockd, fileName);
        close(client_sockd);
        printf("done\n");
      } else {
        printf("%s\n",requestType);
        generateResponse(client_sockd, BADREQ, 0);
      }
    }
  }
}
