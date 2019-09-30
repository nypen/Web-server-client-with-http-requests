#include "server_functions.h"
#include <stdio.h>
#include <string.h>
#include "crawler_functions.h"
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>

#define BUFFERSIZE 256

Requests *BFR_Init(int bsize,char *root_dir){
  Requests *pool = malloc(sizeof(Requests));
  if(pool==NULL){
    return NULL;
  }
  pool->fds=malloc(sizeof(int)*bsize);
  pool->active_fds=0;
  pool->root_dir=root_dir;
  pool->responses=0;
  pool->bytes=0;
  pool->terminate=0;
  pthread_mutex_init(&pool->mtx , NULL);
  pthread_mutex_init(&pool->buffer_full_mtx , NULL);
  pthread_cond_init(&pool->buffer_full,0);
  pthread_mutex_init(&pool->buffer_empty_mtx , NULL);
  pthread_cond_init(&pool->buffer_empty,0);

  return pool;
}

int BFR_PickFd(Requests *pool){
  int i,fd;
  if(pthread_mutex_trylock(&pool->mtx)!=0){
    return -1;
  }
  if(pool->active_fds==0){
      pthread_mutex_unlock(&pool->mtx);
       return -1;
  }
  fd = pool->fds[0];
  for(i=0;i<pool->active_fds-1;i++){
    pool->fds[i]=pool->fds[i+1];
  }
  pool->active_fds--;
  pthread_cond_signal(&pool->buffer_full);
  pthread_mutex_unlock(&pool->mtx);
  return fd;
}

void FDR_free(Requests *pool){
  free(pool->fds);
  pthread_mutex_destroy(&pool->buffer_empty_mtx);
  pthread_mutex_destroy(&pool->buffer_full_mtx);
  pthread_mutex_destroy(&pool->mtx);
  pthread_cond_destroy(&pool->buffer_full);
  free(pool);
}

long int send_response_200(int socket,char *page){
  char response[BUFFERSIZE];
  FILE *fp = fopen(page,"r");
  long int bytes;
  time_t time1;
  struct tm * time2;
  char date[BUFFERSIZE];

  time(&time1);
  time2=localtime(&time1);
  strftime(date,BUFFERSIZE,"%c GMT",time2);
  fseek(fp,0L,SEEK_END);
  bytes=ftell(fp);
  rewind(fp);
  bzero(response,BUFFERSIZE);
  sprintf(response,"HTTP/1.1 200 OK\r\nDate:%s\r\nServer: myhttpd/1.0.0 (Ubuntu 64)\r\nContent-Length: %ld\r\n"
                  "Content-Type: text/html\r\nConnection: Closed\r\n\r\n",date,bytes);
  write(socket,response,BUFFERSIZE);
  bzero(response,BUFFERSIZE);
  while(fgets(response,BUFFERSIZE,fp)!=NULL){
    write(socket,response,BUFFERSIZE);
    bzero(response,BUFFERSIZE);
  }
  return bytes;

}

void send_response_304(int socket){
  char response[BUFFERSIZE],response2[BUFFERSIZE];
  char temp[BUFFERSIZE];
  long int bytes;
  time_t time1;
  struct tm * time2;
  char date[BUFFERSIZE];

  time(&time1);
  time2=localtime(&time1);
  strftime(date,BUFFERSIZE,"%c GMT",time2);

  sprintf(response,"HTTP/1.1 403 Forbidden\r\nDate:%s\r\nServer: myhttpd/1.0.0 (Ubuntu 64)\r\nContent-Length:",date);
  strcpy(temp,"Content-Type: text/html\r\nConnection: Closed\r\n\r\n"
              "<html>Trying to access this file but don’t think I can make it.</html>");
  bytes = strlen(response)+strlen(temp);
  sprintf(response2 , "%s %ld" , response , bytes);
  strcat(response,temp);
  write(socket,response,BUFFERSIZE);
}


void send_response_404(int socket){
  char response[BUFFERSIZE],response2[BUFFERSIZE];
  char temp[BUFFERSIZE];
  long int bytes;
  time_t time1;
  struct tm * time2;
  char date[BUFFERSIZE];

  time(&time1);
  time2=localtime(&time1);
  strftime(date,BUFFERSIZE,"%c GMT",time2);
  bzero(response2,BUFFERSIZE);

  sprintf(response,"HTTP/1.1 404 Not Found\r\nDate:%s\r\nServer: myhttpd/1.0.0 (Ubuntu 64)\r\nContent-Length:",date);
  strcpy(temp,"Content-Type: text/html\r\nConnection: Closed\r\n\r\n"
              "<html>Sorry dude, couldn’t find this file.</html>");
  bytes = strlen(response)+strlen(temp);
  sprintf(response2 , "%s %ld" , response , bytes);
  strcat(response2,temp);
  write(socket,response2,BUFFERSIZE);
}


long int send_response(int socket,char *page){
  if(access(page,0)==0){ //file exists
      if(access(page,4)==0){   //OK
        return send_response_200(socket,page);;
      }else{//forbidden
        send_response_304(socket);
        return 0;
      }
  }else{  //404
    send_response_404(socket);
    return -1;
  }
}

char *read_request(int socket){
  char request[BUFFERSIZE],request2[BUFFERSIZE];
  int found_LF=0;
  char *token=NULL,*file=NULL;
  bzero(request,BUFFERSIZE);
  file=malloc(sizeof(char)*BUFFERSIZE);
  long int bytes;
  while((bytes=read(socket,request,BUFFERSIZE))>0){
      strcpy(request2,request);
      token=strtok(request," ");
      if(strcmp(token,"Get")==0){    //get found,we have to store the page requested
        token=strtok(NULL," ");
        strcpy(file,token);
      }
      if(strstr(request2,"\r\n\r\n")!=NULL){ //an brethike to LF LF break
          break;
      }
      if(found_LF==1 && strcmp(request,"\r\n")==0){    //it means we found LF at last loop and now
          break;
      }
      if(bytes==BUFFERSIZE && request[BUFFERSIZE-2]=='\r' && request[BUFFERSIZE-1]=='\n' ){
        found_LF=1;
      }else found_LF=0;
      bzero(request,BUFFERSIZE);
  }
  return file;
}
