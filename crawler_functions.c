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
#include <pthread.h>
#include <dirent.h>

#define BUFFERSIZE 256


void check_links(char *chunk,job_queue *qjob){
  /**************************************************************
   *    this function receives a chunk and searches for urls    *
   *     if urls are found they are inserted in the queue       *
   **************************************************************/
  char temp[BUFFERSIZE];
  char *link=NULL,*token=NULL;
  bzero(temp,BUFFERSIZE);
  link=strstr(chunk,"<a href=");
  while(link!=NULL){  /*link has been found*/
    strcpy(temp,link);
    token=strtok(temp,"\"");  /*seperate page*/
    token=strtok(NULL,"\"");
    //token=URL
    if(token[0]=='.' && token[1]=='.' && token[2]=='/') token+=3;
    JQ_Addjob(qjob,token);
    link=strstr(link+strlen(token),"<a href="); /*move pointer , and search again*/
  }
}

void makedir(char *out_dir,const char *page){
  /*this function makes the directory the page will be  created in*/
  char file_path[BUFFERSIZE];
  char *token=NULL;
  char temp[BUFFERSIZE];
  strcpy(temp,page);
  strcpy(file_path,out_dir);
  token=strtok(temp,"/");
  strcat(file_path,"/");
  strcat(file_path,token);
  mkdir(file_path,0700);
}

long int ReceiveResponse(int socketfd,char *page,char *save_dir,job_queue *qjob){
  /*********************************************************************************
  *   response is received , and then analyzed . If response is 200 , the page     *
  *   is stored in the directory ,and the links are inserted in the queue          *
  *     If the page is downloades the number of bytes is being returnes,else -1    *
  **********************************************************************************/
  char request1[BUFFERSIZE] ,request2[BUFFERSIZE],file_path[BUFFERSIZE],temp[BUFFERSIZE];
  char *token1=NULL,*token2=NULL;
  FILE *fp=NULL;
  bzero(request1,BUFFERSIZE);
  bzero(request2,BUFFERSIZE);
  bzero(file_path,BUFFERSIZE);
  long int length;
  int chunk=0,bytes;
  /*reading until connection is closed*/
  while((bytes=read(socketfd,request1,BUFFERSIZE))>0){
    strcpy(request2,request1);
    if(chunk==0){           //if this is the first chuck we receive
      token1 = strtok(request2," ");
      while(token1!=NULL){   //we read message word by word to collect the information we need
        if(!strcmp(token1,"HTTP/1.1")){  //1st header
            token1=strtok(NULL," ");
            if(!strcmp(token1,"200")){  //file will be transfered,so we prepare
              makedir(save_dir,page);
              sprintf(file_path,"%s/%s",save_dir,page);
              fp = fopen(file_path,"w");
            }else if(!strcmp(token1,"404")){ //page not found
              return -1;
            }else if(!strcmp(token1,"403")){ //no access to the page
              return -1;
            }
        }else if(!strcmp(token1,"Content-Length:")){ //content header
          token1=strtok(NULL," ");
          length=atoi(token1);     //length of page
          /*once we find the length we can move to the content*/
          break;
        }
        token1=strtok(NULL," \n");
      }
      chunk++;
    }else{  //else we write the chunk in the file
        check_links(request1,qjob);
        fputs(request1,fp);
    }
    bzero(request1,BUFFERSIZE);
    bzero(request2,BUFFERSIZE);
  }
  if(fp!=NULL)  fclose(fp);
  close(socketfd);
  return length;
}

void GET(char *page,int socketfd){
  /*a get request for page is sent to socket */
  char request[BUFFERSIZE];
  bzero(request,BUFFERSIZE);
  sprintf(request,"Get %s HTTP/1.1\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\n"
  "Host: www.tutorialspoint.com\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate\r\nConnection:"
  "Keep-Alive\r\n\r\n",page);
  write(socketfd,request,BUFFERSIZE);
}


void makeDocfile(char *savedirectory,char *file){
  /*the docfile that will be sent as an argument to jobexecutor is made*/
  FILE *fp = fopen(file,"w");
  DIR *dir;
  struct dirent *d;
  char directory[BUFFERSIZE];
  if((dir=opendir(savedirectory))!=NULL){
    while((d = readdir(dir))!=NULL){
        if(strcmp(d->d_name,".")!=0 && strcmp(d->d_name,"..")!=0){
          bzero(directory,BUFFERSIZE);
          sprintf(directory,"%s/%s",savedirectory,d->d_name);
          fputs(directory,fp);
          fputs("\n",fp);
        }
    }
    closedir(dir);
  }
  fclose(fp);
}



job_queue *JQ_Init(){
  job_queue *qjob = malloc(sizeof(job_queue));
  qjob->head=NULL;
  qjob->rear=NULL;
  qjob->next_job=NULL;
  qjob->n_jobs=0;
  pthread_mutex_init (&qjob->mtx , NULL);
  return qjob;
}

void JQ_Addjob(job_queue *qjob,char *page){
  /*a new job is added in the queue, meanwhile mutex is locked so the queue will get updated properly
    when job added condition signals for waiting threads that queue now has jobs*/
  job_node *current=qjob->head;
  pthread_mutex_lock(&qjob->mtx);

  if(qjob->head==NULL){   //insert first node
    qjob->head = malloc(sizeof(job_node));
    qjob->rear = qjob->head;
    qjob->next_job=qjob->head;
    qjob->n_jobs++;
    qjob->head->page =malloc(sizeof(char)*strlen(page)+1);
    strcpy(qjob->head->page,page);
    qjob->head->next = NULL;
  }else{    //insert at end
    while(current!=NULL){ /*parse queue to check if page has been inserted before*/
      if(!strcmp(current->page,page)){
        pthread_mutex_unlock(&qjob->mtx);
        return;
      }
      current = current->next;
    }
    qjob->rear->next=malloc(sizeof(job_node));
    qjob->rear = qjob->rear->next;
    qjob->rear->page=malloc(sizeof(char)*strlen(page)+1);
    strcpy(qjob->rear->page,page);
    qjob->rear->next =NULL;
    qjob->n_jobs++;
  }
  if(qjob->next_job==NULL){
    qjob->next_job=qjob->rear;
  }
  pthread_cond_signal(&qjob->empty_queue);
  pthread_mutex_unlock(&qjob->mtx);
}

char *JQ_Pickjob(job_queue *qjob){
  /*next job of the queue is returnes , and next job is updated*/
  char *page;
  pthread_mutex_lock(&qjob->mtx);
  if(qjob->next_job==NULL){
    pthread_mutex_unlock(&qjob->mtx);
    return NULL;
  }
  qjob->n_jobs--;
  page=qjob->next_job->page;
  qjob->next_job = qjob->next_job->next;
  pthread_mutex_unlock(&qjob->mtx);
  return page;
}

void JQ_Free(job_queue *jobs){
  job_node *current = jobs->head;
  while(current!=NULL){
    jobs->head = jobs->head->next;
    free(current->page);
    free(current);
    current=jobs->head;
  }
}

void THR_free(thread_info *thr_info){
  JQ_Free(thr_info->jobs);
  pthread_cond_destroy(&thr_info->jobs->empty_queue);
  pthread_mutex_destroy(&thr_info->jobs->empty_queue_mtx);
  pthread_mutex_destroy(&thr_info->jobs->empty_queue_mtx);
  pthread_mutex_destroy(&thr_info->active_mtx);
  pthread_mutex_destroy(&thr_info->counter_mtx);
  free(thr_info->jobs);
  free(thr_info->save_dir);
}



thread_info *THR_Init(char *page,int port ,char *save_dir,struct hostent *hostaddr){
  /*struct thread info initialized , starting url added to the queue*/
  thread_info *thr_info;
  thr_info = malloc(sizeof(thread_info));
  thr_info->port = port;
  thr_info->save_dir=save_dir;
  thr_info->jobs=JQ_Init();
  JQ_Addjob(thr_info->jobs,page);
  pthread_cond_init(&thr_info->jobs->empty_queue ,0);
  thr_info->active_threads=0;
  thr_info->host_address = hostaddr;
  thr_info->bytes=0;
  thr_info->downloads=0;
  pthread_mutex_init(&thr_info->active_mtx , NULL);
  pthread_mutex_init(&thr_info->counter_mtx , NULL);
  pthread_mutex_init(&thr_info->jobs->empty_queue_mtx , 0);
  return thr_info;
}
