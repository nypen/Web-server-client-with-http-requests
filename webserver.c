#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "server_functions.h"
#include "threadpool.h"
#include <fcntl.h>

#define BUFFERSIZE 256
#define FDBUFFERSIZE 10

void server_thread_function(void *args){
  /*arguments is of type server_job , server_job has attributes page,socket*/
  Requests *pool = (Requests *)args;
  char *page,path[BUFFERSIZE];
  long int bytes;
  int fd,i;
  while(1){
    pthread_mutex_lock(&pool->buffer_empty_mtx);
    while(pool->active_fds==0){
      pthread_cond_wait(&pool->buffer_empty,&pool->buffer_empty_mtx);
    }
    pthread_mutex_unlock(&pool->buffer_empty_mtx);
    if(pool->active_fds!=0){  /*if buffer is not empty*/
      fd=BFR_PickFd(pool);
      if(fd==-1){
        continue;
      }
      page=read_request(fd);
      if(page[0]=='.' && page[1]=='.' &&page[2]=='/'){
        page+=3;
      }
      bzero(path,BUFFERSIZE);
      sprintf(path,"%s/%s",pool->root_dir,page);
      if((bytes=send_response(fd,path))>0){
        pool->responses++;
        pool->bytes+=bytes;
      }
      close(fd);
    }

  }
}

int main(int argc,char **argv){

  int i,serving_port,command_port,n_threads,fd1,fd2,socket_command , socket_client,root_dir,hours,minutes,seconds;
  time_t start_time, current_time;
  threadpool *threadpool;
  char *token=NULL,*file;
  struct sockaddr_in server_client,server_command;
  struct sockaddr *client,*command;
  socklen_t clientlen,commandlen;
  char request[BUFFERSIZE];
  Requests *pool;

  time(&start_time);

  if(argc!=9){
    printf("Sorry,wrong execution.");
    printf("Please try: ./myhttpd -p serving_port -c command_port  -t num_of_threads -d root_dir\n");
    exit(1);
  }

  /*  parsing arguments */
  for(i=1;i<argc;i+=2){
    if(!strcmp(argv[i],"-p")){  // serving_port
      serving_port = atoi(argv[i+1]);
    }else if(!strcmp(argv[i],"-c")){
      command_port=atoi(argv[i+1]);
    }else if(!strcmp(argv[i],"-t")){
      n_threads = atoi(argv[i+1]);
    }else if(!strcmp(argv[i],"-d")){
      root_dir = i+1;
    }else{
      printf("Sorry,wrong execution.\n");
      printf("Please try: ./myhttpd -p serving_port -c command_port  -t num_of_threads -d root_dir\n");
      exit(1);
    }
  }


  pool = BFR_Init(FDBUFFERSIZE,argv[root_dir]);

  if(pool==NULL){
    printf("Error while initializing fd buffer.\n");
    exit(1);
  }

  threadpool = TP_Init(n_threads,(void *)server_thread_function,(void *)pool);

  if((socket_client=socket(AF_INET,SOCK_STREAM,0))== -1){
    perror("Error while creating socket");
    exit(1);
  }
  if((socket_command=socket(AF_INET,SOCK_STREAM,0))== -1){
    perror("Error while creating socket");
    exit(1);
  }
  fcntl(socket_command, F_SETFL, O_NONBLOCK);
  fcntl(socket_client, F_SETFL, O_NONBLOCK);

  bzero(&server_client,sizeof(server_client));
  server_client.sin_family = AF_INET;
  server_client.sin_addr.s_addr = INADDR_ANY;
  server_client.sin_port = htons(serving_port);

  bzero(&server_command,sizeof(server_command));
  server_command.sin_family = AF_INET;
  server_command.sin_addr.s_addr = INADDR_ANY;
  server_command.sin_port = htons(command_port);

  if(bind(socket_client,(struct sockaddr *) &server_client,sizeof(server_client))<0){
    perror("Error while binding");
    exit(1);
  }
  if(bind(socket_command,(struct sockaddr *) &server_command,sizeof(server_command))<0){
    perror("Error while binding");
    exit(1);
  }
  if(listen(socket_client,3*FDBUFFERSIZE)<0){ //will accept one connection
    perror("Error while socket listening.\n");
    exit(1);
  }
  if(listen(socket_command,FDBUFFERSIZE)<0){ //will accept one connection
    perror("Error while socket listening.\n");
    exit(1);
  }
  clientlen=sizeof(struct sockaddr_in);
  commandlen=sizeof(struct sockaddr_in);
  printf("Waiting for connections..\n");
  while(1){
    pthread_mutex_lock(&pool->buffer_full_mtx);
    while(pool->active_fds==FDBUFFERSIZE){
      pthread_cond_wait(&pool->buffer_full,&pool->buffer_full_mtx);
    }   //while buffer is full wait
    pthread_mutex_unlock(&pool->buffer_full_mtx);
    fd1=accept(socket_client,(struct sockaddr *)&client,&clientlen);
    if(fd1!=-1){
      pthread_mutex_lock(&pool->mtx);
      pool->fds[pool->active_fds] = fd1;
      pool->active_fds++;
      pthread_cond_signal(&pool->buffer_empty);
      pthread_mutex_unlock(&pool->mtx);
    }
    fd2=accept(socket_command,(struct sockaddr *)&command,&commandlen);
    if(fd2!=-1){
      bzero(request,BUFFERSIZE);
      while(read(fd2,request,BUFFERSIZE)>0){
        if(strcmp(request,"STATS\r\n")==0){
          time(&current_time);
          seconds = difftime(current_time,start_time);
          bzero(request,BUFFERSIZE);
          hours = seconds/3600;
          seconds = seconds%3600;
          minutes = seconds/60;
          seconds=seconds%60;
          sprintf(request,"Server up for %02d:%02d:%02d , %d pages have "
                "been served ,%ld bytes.\n",hours,minutes,seconds,pool->responses,pool->bytes);
          write(fd2,request,BUFFERSIZE);
        }else if(!strcmp(request,"SHUTDOWN\r\n")){
          pool->terminate=1;
          FDR_free(pool);
          exit(1);
        }else{
          bzero(request,BUFFERSIZE);
          strcpy(request,"Sorry,I don't serve this command.\n");
          write(fd2,request,BUFFERSIZE);
        }
        bzero(request,BUFFERSIZE);
      }//while tou command
      close(fd2);
      
    }
  }
}
