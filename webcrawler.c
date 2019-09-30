#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include  <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include "crawler_functions.h"
#include <signal.h>
#include "threadpool.h"

#include <sys/stat.h>

#define MYSIG 278
#define BUFFERSIZE 256


void crawler_thread_function(void *arguments){
  thread_info *thr_info = (thread_info *)arguments;
  job_queue *qjob = thr_info->jobs;


  char *requested_page;
  int fd;
  long int bytes;
  struct sockaddr_in server;
  struct sockaddr *serverptr = (struct sockaddr *)&server;
  server.sin_family=AF_INET;
  memcpy(&server.sin_addr,thr_info->host_address->h_addr,thr_info->host_address->h_length);
  server.sin_port=htons(thr_info->port);
  while(1){
    pthread_mutex_lock(&qjob->empty_queue_mtx);
    while(qjob->n_jobs==0){
      pthread_cond_wait(&qjob->empty_queue , &qjob->empty_queue_mtx);
    }
    pthread_mutex_unlock(&qjob->empty_queue_mtx);
    requested_page = JQ_Pickjob(qjob);
    if(requested_page!=NULL){
      if(requested_page[0]=='.' && requested_page[1]=='.' && requested_page[2]=='/'){
        requested_page+=3;
      }
      pthread_mutex_lock(&thr_info->active_mtx);
      thr_info->active_threads++;
      pthread_mutex_unlock(&thr_info->active_mtx);
      if((fd=socket(AF_INET,SOCK_STREAM,0))<0){ //kano sindesi
        perror("Error while creating socket.");
        exit(1);
      }
      if(connect(fd,(struct sockaddr *)&server,sizeof(server))<0){
        perror("Error while connecting");
        exit(1);
      }
      GET(requested_page,fd);
      if((bytes=ReceiveResponse(fd,requested_page,thr_info->save_dir,qjob))>0){
        pthread_mutex_lock(&thr_info->counter_mtx);
        thr_info->downloads++;
        thr_info->bytes+=bytes;
        pthread_mutex_unlock(&thr_info->counter_mtx);
      }
      pthread_mutex_lock(&thr_info->active_mtx);
      thr_info->active_threads--;
      pthread_mutex_unlock(&thr_info->active_mtx);
      close(fd);
    }
  }
}

int main(int argc,char **argv){
  int i,serving_port,port,command_port,n_threads,socketfd,socket_command,fd_command,fdRead,fdWrite,pid,jobexecutor_exec;
  int hours,minutes,seconds;
  char *save_dir=NULL,response[BUFFERSIZE],pipe[BUFFERSIZE],temp[BUFFERSIZE];
  char *starting_URL=NULL,*token=NULL,docfile[BUFFERSIZE],request[BUFFERSIZE];
  threadpool *threadpool=NULL;
  job_queue *qjob=NULL;
  thread_info *thr_info=NULL;
  struct hostent *hostaddr;
  struct sockaddr_in server_command;
  struct sockaddr *serverptr = (struct sockaddr *)&server_command;
  time_t start_time, current_time;
  socklen_t commandlen;
  time(&start_time);


  /*the names of directories will be written in docfile,so jobexecutor can use them */
  bzero(docfile,BUFFERSIZE);
  strcpy(docfile,"docfile.txt");

  if(argc!=12){
    printf("Sorry,wrong execution.");
    printf("Please try: ./mycrawler -h host_or_IP -p port  -c command_port -t num_of_threads -d save_dir starting_URL\n");
    exit(1);
  }

  for(i=1;i<argc;i++){
    if(!strcmp(argv[i],"-p")){  // serving_port
      port = atoi(argv[i+1]);
    }else if(!strcmp(argv[i],"-c")){
      command_port=atoi(argv[i+1]);
    }else if(!strcmp(argv[i],"-t")){
      n_threads = atoi(argv[i+1]);
    }else if(!strcmp(argv[i],"-d")){
      save_dir = malloc(sizeof(char)*strlen(argv[i+1])+1);
      if(save_dir==NULL){
        printf("Error while allocating memory\n");
        exit(1);
      }
      strcpy(save_dir,argv[i+1]);
      mkdir(save_dir,0777);
    }else if(!strcmp(argv[i],"-h")){
      if((hostaddr = gethostbyname(argv[i+1])) == NULL){
        printf("Gethostbyname\n");
        exit(1);
      }
    }else{
      if(i==argc-1){//URL
        starting_URL = malloc(sizeof(char)*strlen(argv[i])+1);
        if(starting_URL==NULL){
          printf("Error while allocating memory\n");
          exit(1);
        }
        strcpy(starting_URL,argv[i]);
      }else if(i%2!=0){
        printf("%s\n",argv[i] );
        printf("!Sorry,wrong execution.\n");
        printf("Please try: ./mycrawler -h host_or_IP -p port  -c command_port -t num_of_threads -d save_dir starting_URL\n");
        if(save_dir!=NULL) free(save_dir);
        if(starting_URL!=NULL) free(starting_URL);
        exit(1);
      }
    }
  }

  if((socket_command=socket(AF_INET,SOCK_STREAM,0))== -1){
    perror("Error while creating socket");
    exit(1);
  }
  fcntl(socket_command, F_SETFL, O_NONBLOCK);

  bzero(&server_command,sizeof(server_command));
  server_command.sin_family = AF_INET;
  server_command.sin_addr.s_addr = INADDR_ANY;
  server_command.sin_port = htons(command_port);

  if(bind(socket_command,(struct sockaddr *) &server_command,sizeof(server_command))<0){
    perror("Error while binding");
    exit(1);
  }
  if(listen(socket_command,5)<0){ //will accept one connection
    perror("Error while socket listening.\n");
    exit(1);
  }
  commandlen=sizeof(struct sockaddr_in);


  thr_info = THR_Init(starting_URL,port,save_dir,hostaddr);
  threadpool = TP_Init(n_threads,(void *)crawler_thread_function,(void *)thr_info);

  /*pipe created for the jobexecutor*/
  bzero(pipe,BUFFERSIZE);
  strcpy(pipe,"Crawler2Executor");
  if(mkfifo(pipe, 0666) == -1){
    if(errno!=EEXIST){
      perror("Make fifo failed.");
      return -1;
    }
  }

  jobexecutor_exec=0;

  while(1){
    fd_command=accept(socket_command,serverptr,&commandlen);
    if(fd_command!=-1){ //connection has been accepted
      bzero(response,BUFFERSIZE);
      while(read(fd_command,response,BUFFERSIZE)>0){
        if(!strcmp(response,"STATS\r\n")){
          bzero(response,BUFFERSIZE);
          time(&current_time);
          seconds = difftime(current_time,start_time);
          hours = seconds/3600;
          seconds = seconds%3600;
          minutes = seconds/60;
          seconds=seconds%60;
          sprintf(response,"Server up for %02d:%02d:%02d , %d downloads "
                ",%ld bytes.\n",hours,minutes,seconds,thr_info->downloads,thr_info->bytes);
          write(fd_command,response,BUFFERSIZE);
        }else if(!strcmp(response,"SHUTDOWN\r\n")){
          if(jobexecutor_exec==1){
            write(fdWrite,response,BUFFERSIZE);
            close(fdWrite);
          }
          THR_free(thr_info);
          free(thr_info);
          free(starting_URL);
          close(fd_command);
          close(fdWrite);
          exit(1);
        }else{
          bzero(temp,BUFFERSIZE);
          strcpy(temp,response);
          token=strtok(temp," ");
          if(!strcmp(token,"SEARCH")){
            if(thr_info->jobs->n_jobs>0 || thr_info->active_threads>0){
              bzero(response,BUFFERSIZE);
              strcpy(response,"Crawling still in progress.Sorry.\n");
              write(fd_command,response,BUFFERSIZE);
            }else{
              if(jobexecutor_exec==0){  //if jobexecutor has not been set
                  jobexecutor_exec=1;
                  pid=fork();
                  if(pid==0){
                    makeDocfile(save_dir,docfile);    /*file including directory names will be created*/
                    char *args[]={"./jobExecutor",docfile,NULL};
                    execvp(args[0],args);
                  }
                  /*opening pipe for writing */
                  strcpy(pipe,"Crawler2Executor");
                  if((fdWrite=open(pipe, O_WRONLY )) < 0){ /*pipes for writing wait for the read-end to be opened*/
                     perror("66 fifo open problem"); exit(3);
                  }
                  write(fdWrite,&fd_command,sizeof(int));
                  bzero(temp,BUFFERSIZE);
                  sscanf(response,"%[^\n]\r\n",temp);
                  write(fdWrite,temp,BUFFERSIZE);
              }else{
                write(fdWrite,&fd_command,sizeof(int));
                bzero(temp,BUFFERSIZE);
                sscanf(response,"%[^\n]\r\n",temp);
                write(fdWrite,temp,BUFFERSIZE);
              }

            }
          }else{
            bzero(response,BUFFERSIZE);
            strcpy(response,"Sorry , I don't serve this command.\n");
            write(fd_command,response,BUFFERSIZE);
          }
        }
        bzero(response,BUFFERSIZE);
      }//end of while read
      close(fd_command);
    }
  }


}
