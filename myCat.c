// the Makefile for this assignment is a modified version from Ethan Miller
// Fall 2019
#include "dog.h"
//void Read_Write(char* fileName);
int main(int argc, char*argv[]){
  if(argc==1){
    Read_Write(NULL);
  }
  for(int i=argc-1;i>0;i--){
    char*fileName = argv[i];
    Read_Write(fileName);
  }
}
// suggestions from Clark for organization
// Idea comes from Clark's sections
void Read_Write(char* fileName){
  uint8_t buffer[BUFFER_LEN];
  if(fileName==NULL||strcmp(fileName,"-")==0){
    ssize_t valRead = 0;
    while((valRead=read(STDIN,buffer,BUFFER_LEN))>0){
     if( write(STDOUT,buffer,valRead)<0){
	warn("%s",fileName);
     }
   }
    return;
  }
  int fd = open(fileName,O_WRONLY);
  int fd2 = open(fileName,O_WRONLY);
  printf("%d\n",fd);
  printf("%d\n",fd2);
  return;
  if(fd<0){
    warn("%s",fileName);
    return;
  }
  while(1){
    ssize_t readBytes = read(fd,buffer,BUFFER_LEN);
    if(readBytes==0){
      if(close(fd)<0){
      	warn("%s",fileName);
      }
      return;
    }
    else if(readBytes<0){
      warn("%s",fileName);
      break;
    }
    else{
      if(write(STDOUT,buffer,readBytes)<0){
        warn("%s",fileName);
      }
    }
  }
}
