#include <string.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include "client.h"
#include "listObj.h"
#include "connectionObj.h"
#include <pthread.h>
#include "queue.h"
#include "server.h"
#include <time.h>
#define MAX 1000
listObj* servers[MAX];
pthread_cond_t condvar = PTHREAD_COND_INITIALIZER;
pthread_cond_t condvarwait = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t serversLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t minLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waitMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ConnectionsMutex = PTHREAD_MUTEX_INITIALIZER;
int count = 0;
void parseBuffer(char *buffer, ssize_t*errors,ssize_t* total) {
  //printf("buffer is: %s\n", buffer);

  char *pch = NULL;
  //char *saveptr = NULL;
  pch = strstr(buffer, "\r\n\r\n");
// printf("this is pch %s\n",pch);

 sscanf(pch, "%ld%ld", errors,total);
// sscanf(pch,"%d[^!]",total);

 //printf("total is: %ld\n",*total);
 //printf("error is: %ld\n",*errors);

}
//this needs to connect between server and clientfd
//goes through the load balancer
int bridge_connections(int fromfd, int tofd) {
    char recvline[100];
    int n = recv(fromfd, recvline, 100, 0);

    if (n < 0) {
        //printf("connection error receiving\n");
        return -1;
    } else if (n == 0) {
        //printf("receiving connection ended\n");
        return 0;
    }
    recvline[n] = '\0';
    n = send(tofd, recvline, n, 0);
    if (n < 0) {
        //printf("connection error sending\n");
        return -1;
    } else if (n == 0) {
        //printf("sending connection ended\n");
        return 0;
    }
    return n;
}
int waitForResponse(int sockfd){
  fd_set set;
  struct timeval timeout;

  while(1) {
      // set for select usage must be initialized before each select call
      // set manages which file descriptors are being watched
      FD_ZERO (&set);
      FD_SET (sockfd, &set);


      // same for timeout
      // max time waiting, 5 seconds, 0 microseconds
      timeout.tv_sec = 2;
      timeout.tv_usec = 0;

      // select return the number of file descriptors ready for reading in set
      switch (select(FD_SETSIZE, &set, NULL, NULL, &timeout)) {
          case -1:
              //printf("error during select, exiting\n");
              return -1;
          case 0:
              return -1;
          default:
              if (FD_ISSET(sockfd, &set)) {
                  return 1;
                }
              else {
                //  printf("this should be unreachable\n");
                  return -1;
              }
          }

    }
}


void bridge_loop(int sockfd1, int sockfd2){
  //printf("called in bridge_loop\n");
    fd_set set;
    struct timeval timeout;

    int fromfd, tofd;
    while(1) {
        // set for select usage must be initialized before each select call
        // set manages which file descriptors are being watched
        FD_ZERO (&set);
        FD_SET (sockfd1, &set);
        FD_SET (sockfd2, &set);

        // same for timeout
        // max time waiting, 5 seconds, 0 microseconds
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;

        // select return the number of file descriptors ready for reading in set
        switch (select(FD_SETSIZE, &set, NULL, NULL, &timeout)) {
            case -1:
                //printf("error during select, exiting\n");
                return;
            case 0:
                //printf("both channels are idle, waiting again\n");
                continue;
            default:
                if (FD_ISSET(sockfd1, &set)) {
                    fromfd = sockfd1;
                    tofd = sockfd2;
                } else if (FD_ISSET(sockfd2, &set)) {
                    fromfd = sockfd2;
                    tofd = sockfd1;
                } else {
                    //printf("this should be unreachable\n");
                    return;
                }
        }
        if (bridge_connections(fromfd, tofd) <= 0)
            return;
    }
}
listObj* findMin(){

  //printf("%s\n",servers[0]->portNumber);
  //printf("in findMin\n");
  listObj* min =malloc(sizeof(listObj));
  int flag =0;
  pthread_mutex_lock(&minLock);
  //printf("size of servers is %zu\n",sizeof(servers));
  //printf("size of listObj is %zu\n",sizeof(listObj*));
    for(size_t i =0;i<sizeof(servers)/sizeof(listObj*);i++){
      //printf("in loop\n");
      if(servers[i]==NULL){

        if(flag!=0){

            pthread_mutex_unlock(&minLock);
            //printf("servers[%d] status is %d portNumber is %s\n",index,min->status,min->portNumber);;
          return min;
        }
          pthread_mutex_unlock(&minLock);

        return NULL;
      }
      //printf("%d\n",servers[i]->status);
      if(servers[i]->status!=0){
        //printf("in servers\n");
        if(flag==0){
          min = servers[i];
          flag =1;

        }
        else{
          if(min->totalConnections > servers[i]->totalConnections){
            //printf("servers[%d] status is %d portNumber is %s\n",i,servers[i]->status,servers[i]->portNumber);
            min = servers[i];
          }
          else if(min->totalConnections == servers[i]->totalConnections &&
          min->invalidConnections > servers[i]->invalidConnections){
            min = servers[i];
            //printf("in here\n");
          }
        }
      }
    }
    pthread_mutex_unlock(&minLock);
    if(min==NULL){
      return NULL;
    }
  //printf("this is the portNumber: %s\n",min->portNumber);
    return min;
}
// char* readFromServer(int clientfd){
//   return NULL;
// }
// void writeToServer(int serverfd,char*message){
//   return;
// }
void healthcheck(){
  //printf("begin?\n");
  char* message = "GET /healthcheck HTTP/1.1\r\n\r\n";
  for(size_t i =0;i<sizeof(servers)/sizeof(listObj);i++){
    if(servers[i]==NULL){
      break;
    }
    //make a connection to servers[i];
    //if connection is -1, server is down
    //else write message to connection port
    int clientfd = -1;
    int port = atoi(servers[i]->portNumber);
    //printf("THIS IS THE PORT: %d\n",port);
    if((clientfd = client_connect(port))<0){
      close(clientfd);
      pthread_mutex_lock(&serversLock);
      //printf("clientfd is -1\n");
      servers[i]->status = 0;
      pthread_mutex_unlock(&serversLock);
      continue;
    }
    //printf("this is the fd %d\n",fd);
    //printf("in formatted string\n");
    send(clientfd,message,strlen(message),0);
    int valid = waitForResponse(clientfd);
    if(valid==-1){
      printf("%s\n",servers[i]->portNumber);
      pthread_mutex_lock(&serversLock);
      servers[i]->status = 0;
      pthread_mutex_unlock(&serversLock);
      close(clientfd);
      continue;
    }
    char buff[4096] = "";
    char buff2[4096] = "";
    int readBytes = 0;
    int total1 = 0;
    while((readBytes=read(clientfd,buff,4096))>0){
      memcpy(buff2+total1,buff,readBytes);
      total1+=readBytes;
    }
    close(clientfd);

    ssize_t errors = -1;
    ssize_t total = -1;
    if(strlen(buff)==0){
      pthread_mutex_lock(&serversLock);
      servers[i]->status = 0;
      pthread_mutex_unlock(&serversLock);
      close(clientfd);
      continue;
    }

    //printf("%s\n",buff2);
    parseBuffer(buff2,&errors,&total);
    // printf("this is : %ld\n",errors);
    // printf("this is : %ld\n",total);
    if(errors==-1||total==-1){
      pthread_mutex_lock(&serversLock);
      servers[i]->status = 0;
      pthread_mutex_unlock(&serversLock);
    }
    else{
    //  printf("here?\n");
      pthread_mutex_lock(&serversLock);
      if(servers[i]->status==0){
        servers[i]->status = 1;
        servers[i]->totalConnections = total;
        servers[i]->invalidConnections = errors;
      //  printf("error number is %ld\n",errors);
        //printf("total number is %ld\n",total);
      }
      else{
        servers[i]->totalConnections = total;
        servers[i]->invalidConnections = errors;
        // printf("error number is ======================> %ld\n",errors);
        //   printf("total number is ======================> %ld\n",total);
      }

      pthread_mutex_unlock(&serversLock);
    }
  }
}
void connectionHandler(node_t* clientPointer,int argp);
void *runThread(void *arg) {

  int *argp = (int *)arg;

  int loadBalancerfd = *argp;
  while (1) {
    pthread_mutex_lock(&mutex);
    node_t* client = dequeue();
    if (client == NULL) {
      pthread_cond_wait(&condvar, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    // printf("here\n");
    if (client != NULL) {
      //printf("thread has got work \n");
      connectionHandler(client,loadBalancerfd);
      //printf("done by thread\n");
    }
  }
}
void connectionHandler(node_t* clientPointer,int loadBalancerfd){
  node_t client = *clientPointer;

  free(clientPointer);

  int portNumber = client.server;
  //printf("portNumber is %d\n",portNumber);
  int client_sockd = client.clientfd;
  //printf("this is the serverfd: %d\n",client_sockd);
  //make a connection to servers
  int socket = client_connect(portNumber);
  //printf("this is the LB client socket: %d\n",socket);
  bridge_loop(socket,client_sockd);

  // char buffer[4096]="";
  // read(client_sockd,buffer,4096);
  // printf("this is the buffer: %s\n",buffer);
  // write(socket,buffer,4096);
  //
  // char buffer2[4096]="";
  //
  //   printf("this is the buffer: %s\n",buffer2);
  //   int bytesRead = 0;
  //   while((bytesRead=read(socket,buffer2,4096))>0){
  //     printf(buffer2);
  //   }
  close(socket);

  close(client_sockd);
  //printf("done\n");
  return;
}
void *healthCheckThread(void* args){
  int* fd = (int*) args;
  struct timespec timeout;
  struct timeval now;
  while(1){
    gettimeofday(&now,NULL);
    timeout.tv_sec = now.tv_sec+3;
    timeout.tv_nsec = 0;
    pthread_mutex_lock(&waitMutex);
    int rt = pthread_cond_timedwait(&condvarwait, &waitMutex, &timeout);
    //printf("rt is ====> %d\n",rt);
    if(rt==-1){
     //pthread_mutex_lock(&mutex);
     healthcheck();
     //pthread_mutex_unlock(&mutex);
    }
    else{

     //pthread_mutex_lock(&mutex);
     healthcheck();
     //pthread_mutex_unlock(&mutex);
    }
    pthread_mutex_unlock(&waitMutex);

  }
}

int main(int argc, char *argv[]) {

  int numThreads = 4;
  int requestThreshold = 5;
  char *port = NULL;

  if (argc < 3) {
    warn("too few arguments");
    return -1;
  }
  int c = 0;
  //printf("argc is: %d\n",argc);

  while ((c = getopt(argc, argv, "N:R:")) != -1){
  //printf("in loop\n");
    switch (c) {
    case 'N':
      //warn("number of threads is: %d\n",atoi(optarg));
      numThreads = atoi(optarg);
      break;
    case 'R':
    //  warn("log file name is: %s\n",optarg);
      requestThreshold = atoi(optarg);
      //printf("this is the request threshold: %d\n",requestThreshold);
      break;

    }
  }

  int serverIndex = 0;
  while (optind < argc){
    //printf("this is optind:%d\n",optind);
    if(port==NULL){
      port = argv[optind];
      optind++;
      continue;
    }
    //printf("setting server\n");
    //start_server(argv[optind]);
    //printf("=============================\n");
    listObj* obj = createNewObj(argv[optind]);

    //ree(obj);
    //printf("this is assigned: %s\n",server.portNumber);
    //printf("=============================\n");
    servers[serverIndex] = obj;
    //printf("this is assigned: %s\n",servers[serverIndex]->portNumber);
    //printf("=============================\n");
    serverIndex++;
    optind++;
  }
  //printf("out of while loop\n");
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
//rintf("port is tested\n");
  int LBListenfd=start_server(port);
  //printf("%d\n",LBListenfd);
  pthread_t workers[numThreads];
 pthread_t healthchecker;
  for (int i = 0; i < numThreads; i++) {
    pthread_create(&workers[i], NULL, runThread,&LBListenfd);
  }
  pthread_create(&healthchecker,NULL,healthCheckThread,&LBListenfd);
  //printf("threads made\n");
  healthcheck();

    //printf("infinite loop\n");
  while (1) {
    struct sockaddr client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    int client_sockd = accept(LBListenfd, &client_addr, &client_addrlen);
    if (client_sockd < 0) {
      warn("accept");
      return -1;
    }

    // idea to use a queue and condition variables comes from
    // Michael Covarrubias
    //warn("connection received\n");
    pthread_mutex_lock(&ConnectionsMutex);
  //  printf("====================================HELLLLLLLLLLLL\n");
    count++;
    //printf("======================= count is %d\n",count);
    //printf("this is the requestThreshold%d\n",requestThreshold);
    if(count==requestThreshold){
      //printf("this is the count ================> %d\n",count);
      pthread_mutex_lock(&mutex);
      pthread_cond_signal(&condvarwait);
      sleep(0.5);
      pthread_mutex_unlock(&mutex);

      count = 0;
      //printf("triggered\n");
    }
    pthread_mutex_unlock(&ConnectionsMutex);

    int *clientPointer = malloc(sizeof(int));
    *clientPointer = client_sockd;
    //printf("%s\n",servers[0]->portNumber);
    listObj*server;
    server = findMin();

    //printf("here\n");
    if(server == NULL){
      //warn("server is null\n");
      char message[1024];
      sprintf(message,
              "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
        send(client_sockd,message,strlen(message),0);
        close(client_sockd);
        continue;
    }
    // printf("server is =====================> %s\n",server->portNumber);
    // printf("server TOTAL IS =====================> %d\n",server->totalConnections);
    // printf("OTHER SERVER TOTAL IS =====================> %d\n",servers[1]->totalConnections);


    pthread_mutex_lock(&mutex);
    //printf("this is the portNumber %s\n",server->portNumber);
    enqueue(clientPointer,server->portNumber);
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&mutex);
    //printf("enqueued\n");
  }
}
