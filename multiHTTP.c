
#define FAIL 6
#define CONSTANTSPACE 30
#define BUFFER_LEN 1024
size_t offset = 0;
size_t errCount = 0;
size_t totalCount = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t offsetMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t errMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t totalMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condvar = PTHREAD_COND_INITIALIZER;
// size_t setCounts(int fd, int type) {
//   char buff[1024] = "";
//   char *totalString = NULL;
//   if (type == 0) {
//     totalString = "========\n";
//   } else {
//     totalString = "FAIL";
//   }
//   size_t result = 0;
//   while (read(fd, buff, 1024) > 0) {
//     char *tmp = buff;
//     while (strstr(tmp, totalString)) {
//       result++;
//       tmp += strlen(totalString);
//     }
//   }
//   return result;
// }
size_t writeFormatter(uint8_t *writeBuffer, int byteCount, int fd,
                      size_t currentOffset, size_t bytesRead) {
  // printf("POST is in here\n");

  char temp[10] = "";
  char hexRep[256] = "";
  char buff[1024] = "";
  /*
  credit for the hexloop goes to
  https://www.includehelp.com/c/convert-ascii-string-to
  -hexadecimal-string-in-c.aspx
  */
  // printf("this is write buffer: %s\n",writeBuffer);
  int loop;
  int i;
  i = 0;
  size_t count = 0;
  loop = 0;
  while (count < bytesRead) {
    sprintf((char *)(hexRep + i), " %02x", writeBuffer[loop]);
    loop += 1;
    i += 3;
    count++;
  }
  hexRep[i++] = '\0';
  sprintf(temp, "%08d", byteCount);
  sprintf(buff, "%s%s\n", temp, hexRep);
  // printf("buffer is: %s\n",buff);
  size_t shouldWrite = strlen(buff);
  size_t bytesWritten = 0;
  while (bytesWritten != shouldWrite) {
    bytesWritten +=
        pwrite(fd, buff, strlen(buff) - bytesWritten, currentOffset);
    currentOffset += bytesWritten;
  }
  return bytesWritten;
}
size_t calculateSpace(char *fileName, char *requestType, char* http, ssize_t fileSize) {

  size_t reserve = 0;
  //
  // if(strcmp(fileName, "healthcheck")==0){
  //   char buff[80] = "";
  //   sprintf(buff,"%zu\n%zu\n",errCount,totalCount);
  //   reserve+=strlen(buff)+9;
  //   return reserve;
  // }
  if (strcmp(fileName, "healthcheck") == 0 && strcmp(requestType, "GET") != 0) {
    int NameLength = strlen(fileName);
    reserve = FAIL + strlen(requestType) + CONSTANTSPACE + NameLength + strlen(http);


    return reserve;
  }
  int lengthOfNumber = 1;
  if (fileSize == -1) {
    // pthread_mutex_lock(&errMutex);
    // errCount++;
    // pthread_mutex_unlock(&errMutex);
    // int nDigits = floor(log10(abs(the_integer))) + 1;
    int NameLength = 0;
    if (strlen(fileName) <= 27) {
      NameLength = strlen(fileName);
    } else {
      NameLength = strlen(fileName);
    }
    reserve = FAIL + strlen(requestType) + CONSTANTSPACE + NameLength+ strlen(http);

    return reserve;
  } else {
    char str[256] = "";
    char buff[80] = "";
    if (strcmp(fileName, "healthcheck") == 0) {
      printf("the total count at healthcheck is: %ld\n",totalCount);
      printf("the error count at healthcheck is: %ld\n",errCount);
      sprintf(buff, "%zu\n%zu", errCount, totalCount);
      fileSize = strlen(buff);
    }
    snprintf(str, sizeof str, "%ld", fileSize);
    lengthOfNumber = strlen(str);
    //printf("length of number is: %d", lengthOfNumber);

    int lengthOfHeader =
        strlen(requestType) + 2 + strlen(fileName) + strlen(http) + lengthOfNumber + 1;
  //  printf("length of header:%d\n", lengthOfHeader);
    size_t lines = fileSize / 20;
    size_t remaining = fileSize % 20;
    int extra =1;
    // printf("this is the fileSize: %ld\n",fileSize);
    //printf("this is the file size %zu\n",fileSize);
    if(fileSize%20==0){
    //  printf("100 is here\n");
      extra = 0;
    }
    size_t sizeOfFile =
        8 * (lines + extra) + 20 * 3 * lines + (remaining)*3 + lines + extra;
    // printf("this is the size of file: %zu\n",sizeOfFile);

    reserve += lengthOfHeader + sizeOfFile + 9;

    if (strcmp(requestType, "HEAD") == 0) {
    //  printf("space for head\n");
      reserve = lengthOfHeader + 9;
    //  printf("this head space: %zu\n", reserve);


      return reserve;
    }

    printf("the total count is: %ld\n",totalCount);
    return reserve;
  }
}
void writeToLog(int fd, char *fileName, char *requestType, ssize_t fileSize,
                int code,char* http) {
  size_t healthcheckTotal =0;
  size_t healthecheckError = 0;
  // printf("this is the file name: %s\n",fileName);
  // printf("this is the file size: %ld\n",fileSize);
  // printf("this is the code: %d\n",code);
  char writeBuffer[4096] = "";
  char convertToHex[20] = "";
  int filefd = -1;
  pthread_mutex_lock(&offsetMutex);
  //printf("this is offset====================> : %zu\n ", offset);
  size_t currentOffset = offset;
  size_t space = calculateSpace(fileName, requestType,http, fileSize);
  //printf("space allocated is: %zu\n", space);
  offset += space;
  if(code == 201||code ==200){
    pthread_mutex_lock(&totalMutex);
    if(strcmp(fileName,"healthcheck") == 0){
      healthcheckTotal = totalCount;
      healthecheckError = errCount;
    }
    totalCount++;
    pthread_mutex_unlock(&totalMutex);
  }else{
    pthread_mutex_lock(&totalMutex);
    totalCount++;
    errCount++;
    pthread_mutex_unlock(&totalMutex);

  }
//  printf("this is new offset: %zu\n ", offset);
  pthread_mutex_unlock(&offsetMutex);
  filefd = open(fileName, O_RDWR);
  //  printf("filefd: =============> %d\n",filefd);
  if (strcmp(fileName, "healthcheck") == 0 && strcmp(requestType, "GET") == 0) {
    filefd = 0;
  }
  if (strcmp(fileName, "healthcheck") == 0 && strcmp(requestType, "GET") != 0) {
    code = 403;
  }

  if (filefd == -1||fileSize== -1) {
    if (strlen(fileName) > 27)
    snprintf(writeBuffer,4096, "FAIL: %s /%s %s --- response %d\n",
            requestType, fileName, http, code);
    else {
    //  printf("length of write buffer is: %zu\n",strlen(writeBuffer));
      snprintf(writeBuffer,4096, "FAIL: %s /%s %s --- response %d\n",
              requestType, fileName, http, code);
            //  printf("length of write buffer is: %zu\n",strlen(writeBuffer));
    }


    pwrite(fd, writeBuffer, strlen(writeBuffer), currentOffset);
    currentOffset += strlen(writeBuffer);
    char finished[] = "========\n";
    pwrite(fd, finished, strlen(finished), currentOffset);
    return;
  }
  // else if (fileSize == -1) {
  //   printf("fileSize is -1\n");
  //   if (strlen(fileName) > 27)
  //     sprintf(writeBuffer, "FAIL: %s /%s HTTP/1.1 --- response %d\n",
  //             requestType, fileName, code);
  //   else {
  //     sprintf(writeBuffer, "FAIL: %s /%s HTTP/1.1 --- response %d\n",
  //             requestType, fileName, code);
  //   }
  //   // sprintf(writeBuffer,"FAIL: %s /%.*s HTTP/1.1 --- response
  //   // %d\n",requestType,27,fileName,code);
  //   // pwrite(fd,writeBuffer,strlen(writeBuffer),currentOffset);
  //   currentOffset += strlen(writeBuffer);
  //   char finished[] = "========\n";
  //   pwrite(fd, finished, strlen(finished), currentOffset);
  //   return;
  // }
  if(!(strcmp(requestType,"GET")==0||strcmp(requestType,"HEAD")==0||strcmp(requestType,"PUT")==0)){
    snprintf(writeBuffer,4096, "FAIL: %s /%s %s --- response %d\n",
            requestType, fileName,http, code);
            pwrite(fd, writeBuffer, strlen(writeBuffer), currentOffset);
            currentOffset += strlen(writeBuffer);
            char finished[] = "========\n";
            pwrite(fd, finished, strlen(finished), currentOffset);
            return;
  }
  else {
    //printf("healthcheck? %s\n", fileName);
    // format the header and write to logfile
    sprintf(writeBuffer, "%s /%s length %zu\n", requestType, fileName,
            fileSize);
    pwrite(fd, writeBuffer, strlen(writeBuffer), currentOffset);
    currentOffset += strlen(writeBuffer);
    if (strcmp(requestType, "HEAD") == 0) {
    //  printf("in here\n");
      char finished[] = "========\n";
      pwrite(fd, finished, strlen(finished), currentOffset);
      return;
    }
    size_t bytesRead = 0;
    int byteCount = 0;
    if (strcmp(fileName, "healthcheck") == 0) {
      char buff[80] = "";
      sprintf(buff, "%zu\n%zu", healthecheckError, healthcheckTotal);
    //  printf("this is the buffer: %s\n", buff);
      int count =
          writeFormatter((uint8_t*)buff, byteCount, fd, currentOffset, strlen(buff));
      currentOffset += count;
      char finished[] = "========\n";
      pwrite(fd, finished, strlen(finished), currentOffset);
      return;
    }
    while ((bytesRead = read(filefd, convertToHex, 20)) > 0) {
    //  printf("byteCount is=======>%zu\n",byteCount);
      int bytesWritten =
          writeFormatter((uint8_t*) convertToHex, byteCount, fd, currentOffset, bytesRead);
    //  printf("bytesRead is : %zu\n",bytesRead);
    //  printf("fileSize is : %zu\n",fileSize);
      byteCount += bytesRead;
    //  printf("byteCount is : %zu\n",byteCount);
      currentOffset += bytesWritten;
      if(byteCount==fileSize){
        break;
      }
    }
    //printf("out of loop\n");
    char finished[] = "========\n";
    pwrite(fd, finished, strlen(finished), currentOffset);
  }
}
void generateResponse(int fd, int messageType, int contentLength) {
  int shouldClose = 1;
  if (messageType == CREATED) {
    char message[MESSAGESIZE];
    int count = 0;
    sprintf(message, "201 Created\r\nContent-Length:%d\r\n\r\n", count);
    send(fd, message, strlen(message), 0);
    // close(fd);
  } else if (messageType == BADREQ) {
    char message[MESSAGESIZE];
    sprintf(message, "HTTP/1.1 400 bad Request\r\nContent-Length: 0\r\n\r\n");
    send(fd, message, strlen(message), 0);
    // close(fd);
  } else if (messageType == OK) {
    //printf("head is here\n");
    char message[MESSAGESIZE];
    char http[ARGUMENTSIZE] = "HTTP/1.1";
    sprintf(message, "%s 200 OK\r\nContent-Length: %d\r\n\r\n", http,
            contentLength);
    send(fd, message, strlen(message), 0);
    shouldClose = 0;
  } else if (messageType == FORBIDDEN) {
    char message[MESSAGESIZE];

    sprintf(message, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");
    send(fd, message, strlen(message), 0);
    // close(fd);
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
  if (shouldClose) {
    close(fd);
  }
}
int validateFileName(char *fileName) {
  // printf("%s\n",fileName);
  if (strlen(fileName) > 27) {
    // printf("file name is > 27 characters\n");
    // generateResponse(client_sockd, BADREQ, 0);
    return -1;
  }
  for (size_t i = 0; i < (size_t)strlen(fileName); i++) {
    if ((!isalnum(fileName[i])) && fileName[i] != '_' && fileName[i] != '-') {
      return -2;
    }
  }
  return 0;
}
ssize_t validateHeader(int client_sockd, char *buffer, char *body) {
  printf("this is in validateHeader\n");
  ssize_t bytesRead = 0;
  int headerFound = 0;
  size_t headerCount = 0;
  while (headerFound == 0) {
    if (headerCount > 4096) {
      generateResponse(client_sockd, BADREQ, 0);
      close(client_sockd);
    } else {
      // printf("reading\n");
      bytesRead =
          recv(client_sockd, buffer + headerCount, 4096 - headerCount, 0);
          printf("this is the buffer %s\n",buffer);
      if (bytesRead <= 0) {
        return bytesRead;
      }

      headerCount += bytesRead;
      char *test = strstr(buffer, "\r\n\r\n");
      if (test) {
        //printf("header found\n");
        headerFound = 1;
        size_t pos = test - buffer;
        // if (pos + 4 < strlen(buffer)) {
        //   int bodyIndex =0;
        //   // for(ssize_t i = pos+4;i<strlen(buffer);i++){
        //   //   body[bodyIndex] = buffer[i];
        //   //   bodyIndex++;
        //   // }
        // //  printf("the body is: %s\n",body);
        //   //send(body, buffer + pos + 4, strlen(buffer) - (pos + 4), 0);
        // }
        return bytesRead;
      }
    }
  }
  return -1;
}

void writeToFile(int fd, char*buffer, ssize_t size){
  write(fd,buffer,size);
}

void Read_Write(int fd, int socket, ssize_t fileSize, int valid, char* body) {
  // printf("%d\n",fd);
  if (valid < 0) {
    // printf("here\n");
    char *buffer[BUFFER_LEN];
    int count = 0;
    while (count < fileSize) {
      ssize_t readBytes = read(socket, buffer, BUFFER_LEN);
      count += readBytes;
      if (readBytes < BUFFER_LEN) {
        generateResponse(socket, BADREQ, 0);
        close(socket);
        return;
      }
    }
  }
  if(body != NULL && strlen(body)>0){
    printf("body is : %s \n", body);
    writeToFile(fd,body,strlen(body));
    printf("sent\n");
    generateResponse(socket, CREATED, 0);
    close(socket);
    return;
  }

  // int fd = open(fileName, O_CREAT | O_RDWR, 0777);
  int count = 0;
   //printf("in read_write\n");
  char buffer[BUFFER_LEN];
  //printf("this is the fileSize: %zu\n",fileSize);
  while (count < fileSize) {
   //printf("this is the count: %d\n",count);
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
    //  printf("this is readBytes %d\n",readBytes);
      ssize_t total = 0;
      ssize_t byteWritten = 0;
      while (total < readBytes) {
        if ((byteWritten = write(fd, buffer, readBytes - total)) < 0) {
          //  printf("%d\n",fd);
          warn("write");
          close(socket);
          break;
        }
        total += byteWritten;
      }
      count += readBytes;
    }
    if (readBytes < BUFFER_LEN&&count==fileSize) {
    //  printf("this is the count: %d and this is the size is %ld\n",count,fileSize);
      generateResponse(socket, CREATED, 0);
      return;
    }
  }
  //printf("returned\n");
}
void sendPutRequest(int client_sockd, char *fileName, ssize_t fileSize,
                    int logfd, char* body,char* http) {

  if (logfd < 0 && strcmp(fileName, "healthcheck") == 0) {
    generateResponse(client_sockd, NOTFOUND, 0);
    close(client_sockd);
    return;
  }
  if (logfd > 0 && strcmp(fileName, "healthcheck") == 0) {
    generateResponse(client_sockd, FORBIDDEN, 0);
    writeToLog(logfd,fileName,"PUT",-1,FORBIDDEN,http);
    close(client_sockd);
    return;
  }
  if (fileSize == -1) {
    writeToLog(logfd, fileName, "PUT", fileSize, BADREQ,http);
    generateResponse(client_sockd, BADREQ, 0);
    return;
  }
  int valid = validateFileName(fileName);
  if (valid < 0) {
    if (logfd != -1) {
      writeToLog(logfd, fileName, "PUT", -1, BADREQ,http);
    }
    generateResponse(client_sockd, BADREQ, 0);
    close(client_sockd);
    return;
  }
  int fd1 = open(fileName, O_RDWR);
  //printf("this is the fd1: %d\n",fd1);
  if (fd1 < 0) {
    if(errno == EPERM){
      writeToLog(logfd, fileName, "PUT", -1, FORBIDDEN,http);
      generateResponse(client_sockd, FORBIDDEN, 0);
      return;
    }
    if (errno == EACCES) {
      if(logfd!=-1)
      writeToLog(logfd, fileName, "PUT", -1, FORBIDDEN,http);
      generateResponse(client_sockd, FORBIDDEN, 0);
      return;
    }
    if(errno == EISDIR){
      printf("file is a directory\n");
      if(logfd!=-1){
        writeToLog(logfd, fileName, "PUT", -1, FORBIDDEN,http);
      }
      generateResponse(client_sockd, FORBIDDEN, 0);
      return;
    }
  }
  // if (fd1 > 0) {
  //   remove(fileName);
  //   close(fd1);
  // }
  int fd2 = open(fileName, O_CREAT | O_RDWR | O_TRUNC, 0644);
  //printf("%d\n", fd2);
  Read_Write(fd2, client_sockd, fileSize, valid, body);
  if (close(client_sockd) < 0)
  printf("this is the ");
  if (logfd != -1)
    writeToLog(logfd, fileName, "PUT", fileSize, CREATED,http);
  close(fd2);
}
void sendGetRequest(int client_sockd, char *fileName, int logfd,char* http) {
 printf("getting %s\n",fileName);
  if (logfd < 0 && strcmp(fileName, "healthcheck") == 0) {
    generateResponse(client_sockd, NOTFOUND, 0);
    close(client_sockd);
    return;
  }
  if (strcmp(fileName, "healthcheck") == 0) {

    char buff[80] = "";
    printf("this is error count %zu\n",errCount);
    snprintf(buff,4, "%zu\n%zu", errCount, totalCount);
    generateResponse(client_sockd, OK, strlen(buff));
    //printf("this is buffer: %s\n",buff);
    send(client_sockd,buff,strlen(buff),0);
    if (logfd != -1) {
      char buff1[80] = "";
      snprintf(buff1,4, "%zu\n%zu", errCount, totalCount);
      writeToLog(logfd, fileName, "GET", strlen(buff1), OK,http);

    //  printf("buffer is: %s\n",buff1);

      close(client_sockd);
      return;
    }

  }
  int valid = validateFileName(fileName);
  //printf("valid is: %d\n",valid);
  if (valid < 0) {
    writeToLog(logfd, fileName, "GET", -1, BADREQ,http);
    generateResponse(client_sockd, BADREQ, 0);
    close(client_sockd);
    return;
  }



  int fd2 = open(fileName, O_RDONLY);
  if(fd2>0){
    struct stat path_stat;
    stat(fileName, &path_stat);
    if(S_ISREG(path_stat.st_mode)==0){
      writeToLog(logfd, fileName, "GET", -1, FORBIDDEN,http);
      generateResponse(client_sockd, FORBIDDEN, 0);
      close(fd2);
      return;
    }
  }

  //printf("file descriptor is: %d\n",fd2);
  if (fd2 < 0) {
  //  printf("file descriptor is: %d\n",fd2);
    if(errno == EPERM){
      writeToLog(logfd, fileName, "GET", -1, FORBIDDEN,http);
    generateResponse(client_sockd, FORBIDDEN, 0);
    }
    if (errno == EISDIR) {
      if (logfd != -1)
        writeToLog(logfd, fileName, "GET", -1, BADREQ,http);
      generateResponse(client_sockd, BADREQ, 0);
    }
    if (errno == EACCES) {
      if (logfd != -1)
        writeToLog(logfd, fileName, "GET", -1, FORBIDDEN,http);
      generateResponse(client_sockd, FORBIDDEN, 0);
    } else if (errno == ENOENT) {
        if (logfd != -1){
          writeToLog(logfd, fileName, "GET", -1, NOTFOUND,http);
        }

          generateResponse(client_sockd, NOTFOUND, 0);
        }
    warn("Open");
    close(fd2);
    return;
  }
  size_t fileSize = 0;
  struct stat st;
  if (stat(fileName, &st) == 0)
    fileSize = (st.st_size);
  //printf("this is the result from stat: %zu\n", fileSize);
  // send OK message here
  // close(fd2);
  generateResponse(client_sockd, OK, fileSize);
  Read_Write(client_sockd, fd2, fileSize, 0, NULL);
  close(fd2);
  close(client_sockd);
  if (logfd != -1) {
    writeToLog(logfd, fileName, "GET", fileSize, OK,http);
    //printf("done?\n");
    return;
  }
}
void sendHeadRequest(int client_sockd, char *fileName, int logfd,char* http) {
  if (logfd < 0 && strcmp(fileName, "healthcheck") == 0) {
    generateResponse(client_sockd, NOTFOUND, 0);
    close(client_sockd);
    return;
  }
  if (logfd > 0 && strcmp(fileName, "healthcheck") == 0) {
    writeToLog(logfd, fileName, "HEAD", -1, FORBIDDEN,http);
    generateResponse(client_sockd, FORBIDDEN, 0);

    close(client_sockd);
    return;
  }
  //printf("in head\n");
//  printf("%s\n", fileName);
  int fd2 = open(fileName, O_RDONLY);
  int valid = validateFileName(fileName);
  if (valid < 0) {
    if (logfd != -1)
      writeToLog(logfd, fileName, "HEAD", -1, BADREQ,http);
    generateResponse(client_sockd, BADREQ, 0);
    close(client_sockd);
    return;
  }
  // printf("%s\n",fileName);
  if (fd2 < 0) {
    if (errno == ENOENT) {
    //  printf("error\n");
      if (logfd != -1)
        writeToLog(logfd, fileName, "HEAD", -1, NOTFOUND,http);
      generateResponse(client_sockd, NOTFOUND, 0);
      return;

    } else if (errno == EACCES) {
      if (logfd != -1)
        writeToLog(logfd, fileName, "HEAD", -1, FORBIDDEN,http);
      generateResponse(client_sockd, FORBIDDEN, 0);
      return;
    }
    //warn("Open Header");
    close(fd2);
    return;
  }
  ssize_t fileSize = 0;
  struct stat st;
  if (stat(fileName, &st) == 0)
    fileSize = (st.st_size);
  // send OK message here
  if (logfd != -1)
    writeToLog(logfd, fileName, "HEAD", fileSize, OK,http);
  generateResponse(client_sockd, OK, fileSize);

  close(client_sockd);
  close(fd2);
//  printf("finished\n");
}
void stringParser(char *buffer, char *reqType, char *fileName, char *http,
                  ssize_t *Length, int fd) {
  //           int count = 1;
  char *pch = NULL;
  char *saveptr = NULL;
  char placeholder[2048];
  sscanf(buffer, "%s %s %s", reqType, fileName, http);
  //printf("%s\n", reqType);
  pch = strtok_r(buffer, "\r\n", &saveptr);
  if (strcmp(reqType, "GET") == 0) {
    return;
  } else if (strcmp(reqType, "HEAD") == 0) {
    return;
  }
  //printf("NOT STACKSMASH\n");
  // loop taken from stackoverflow how to parse http headers
  int found = 0;
  while (pch != NULL) {
    sscanf(pch, "%s", placeholder);
    //printf("%s\n", placeholder);
    if (strcmp(placeholder, "Content-Length:") == 0 && found == 1) {
      //printf("here second occurrence \n");
      generateResponse(fd, BADREQ, 0);
    }
    if (strcmp(placeholder, "Content-Length:") == 0 && !found) {
      sscanf(pch, "%s%ld", placeholder, Length);
      found = 1;
      // continue;

      //  return;
    }

    pch = strtok_r(NULL, "\r\n", &saveptr);
  }
  if (found == 1) {
    //  printf("here\n");
    return;
  }
  // sscanf(buffer,"%s\n%s\n%s\n",reqType,fileName,http);
  // printf("%s\n",reqType);
  // printf("%s\n",fileName);
  // printf("%d\n",Length);
  generateResponse(fd, BADREQ, 0);
  return;
}
void connectionHandler(int *pclient_sockd, int logfd) {
  int client_sockd = *pclient_sockd;
  free(pclient_sockd);
  printf("this is %d\n", client_sockd);
  char buffer[BUFFERSIZE + 1] = "";
  char* body = NULL;
  char fileName[ARGUMENTSIZE] = "";
  char requestType[ARGUMENTSIZE] = "";
  ssize_t fileSize = -1;
  char http[ARGUMENTSIZE] = "";
  ssize_t bytesRead = validateHeader(client_sockd, buffer, body);
  printf("bytesRead is %ld\n",bytesRead);
  if (bytesRead < 0) {
  warn("read error");
    close(client_sockd);
  } else {
    printf("header is valid\n");
    stringParser(buffer, requestType, fileName, http, &fileSize, client_sockd);
      printf("this is the file Name ===========> %s\n", fileName);
      printf("this is the requestType Name ===========> %s\n", requestType);
    if(!(strcmp(requestType,"GET")==0||strcmp(requestType,"PUT")==0||strcmp(requestType,"HEAD")==0)){
    //  printf("requestType is wrong\n");
      if (fileName[0] == '/') {
        memmove(fileName, fileName + 1, strlen(fileName));
      }
  //    printf("this is the file size: %ld\n",fileSize);
      writeToLog(logfd, fileName, requestType, -1, BADREQ,http);
      generateResponse(client_sockd, BADREQ, 0);
      return;
    }
  //  printf("this is http %s\n",http);
    if (strcmp(http, "HTTP/1.1") != 0) {
      if (logfd != -1)
      if (fileName[0] == '/') {
        memmove(fileName, fileName + 1, strlen(fileName));
      }
        writeToLog(logfd, fileName, requestType, fileSize, BADREQ,http);
      generateResponse(client_sockd, BADREQ, 0);
    }
    // printf("%s\n", requestType);
    if (fileName[0] == '/') {
      memmove(fileName, fileName + 1, strlen(fileName));
    } else {
      if (logfd != -1)
        writeToLog(logfd, fileName, requestType, fileSize, BADREQ,http);
      generateResponse(client_sockd, BADREQ, 0);
    }
    if (strcmp(fileName, "") == 0 || strcmp(requestType, "") == 0 ||
        strcmp(http, "") == 0) {
      //printf("error filename\n");
      if (logfd != -1)
        writeToLog(logfd, fileName, requestType, fileSize, BADREQ,http);
      generateResponse(client_sockd, BADREQ, 0);
      // return -1;
      return;
    }
    if (strcmp(requestType, "GET") == 0) {
      sendGetRequest(client_sockd, fileName, logfd,http);
      // writeToLog(fileName,requestType,fileSize,OK);
    } else if (strcmp(requestType, "PUT") == 0) {
      // printf("this is the socket Number: %d\n",client_sockd);
      sendPutRequest(client_sockd, fileName, fileSize, logfd, body,http);
      // writeToLog(fileName,requestType,fileSize,CREATED);
    //  printf("done\n");
      return;
    } else if (strcmp(requestType, "HEAD") == 0) {

      sendHeadRequest(client_sockd, fileName, logfd,http);
      // writeToLog(fileName,requestType,fileSize,OK);
      close(client_sockd);
      //printf("done\n");
    } else {
    //  printf("the request Type is: %s\n", requestType);
      if(logfd!=-1){
      //printf("Hello is not valid\n");
      writeToLog(logfd, fileName, requestType, -1, BADREQ,http);
      generateResponse(client_sockd, BADREQ, 0);
    }
      return;
    }
  }
}
void *runThread(void *arg) {
  printf("here\n");
  int *argp = (int *)arg;
  int logfd = *argp;
printf("this is log fd: %d\n", logfd);
  while (1) {
    pthread_mutex_lock(&mutex);
    int *client = dequeue();
    if (client == NULL) {
      pthread_cond_wait(&condvar, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    // printf("here\n");
    if (client != NULL) {
      printf("got work \n");
      connectionHandler(client, logfd);
      printf("done by thread\n");
    }
  }
}
int main(int argc, char *argv[]) {
  // warn("the argument is:");
  // for(int i = 0; i < argc;i++){
  //   //warn("%s ",argv[i]);
  // }
  int numThreads = 4;
  char *port = NULL;
  int logfd = -1;
  if (argc < 2) {
    warn("too few arguments");
    return -1;
  }
  int c = 0;
  //printf("argc is: %d\n",argc);

  while ((c = getopt(argc, argv, "N:l:")) != -1){
  //  printf("in loop\n");

    switch (c) {

    case 'N':
      //warn("number of threads is: %d\n",atoi(optarg));
      numThreads = atoi(optarg);
      break;
    case 'l':
    //  warn("log file name is: %s\n",optarg);
      logfd = open(optarg, O_CREAT | O_WRONLY | O_TRUNC, 0644);

      break;

    }
  }
  // warn("this is opt ind %d\n",optind);
  // warn("this is argc %d\n",argc);
  if(optind < argc){
    port = argv[optind];
  }
  else{
    //warn("Error optind is : %d",optind);
  }
  if(port == NULL){

    return -1;
  }
  for (size_t i = 0; i < strlen(port); i++) {
    if (!isdigit(port[i])) {
      //warn("port is not a number\n");
      return -1;
    }
  }
  int p = atoi(port);
  if (p <= 1024) {
    warn("error");
    return -1;
  }
  pthread_t workers[numThreads];
  for (int i = 0; i < numThreads; i++) {
    pthread_create(&workers[i], NULL, runThread, &logfd);
    printf("done\n");
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
  ret = listen(server_sockd, 10);
  if (ret == -1) {
    warn("listen");
    return -1;
  }
  while (1) {
    struct sockaddr client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
    if (client_sockd < 0) {
      warn("accept");
      return -1;
    }
    // idea to use a queue and condition variables comes from
    // Michael Covarrubias
    int *clientPointer = malloc(sizeof(int));
    *clientPointer = client_sockd;

    pthread_mutex_lock(&mutex);
    enqueue(clientPointer);
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&mutex);
    printf("enqueued\n");
  }
}
