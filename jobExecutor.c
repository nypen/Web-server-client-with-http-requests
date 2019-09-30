#include  <sys/types.h>
#include  <sys/stat.h>
#include  <fcntl.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include "functions.h"
#include <dirent.h>
#include "parentMessages.h"
#include "childMessages.h"
#include "trie.h"


#define BUFFERSIZE 256

int main(int argc,char **argv){
  char *request;
  int numWorkers=3;
  int fd,socketfd;
  char **queries_words;int no_queries;
  int length,worker, noDirs,i ,status , options , *fdWrite,*fdRead ;
  pid_t pid;
  FILE *fp;
  char *line = NULL,**directories,pipe[BUFFERSIZE];
  size_t  buf ;

  /*open file that includes directories*/
  fp = fopen(argv[1],"r");
  if(fp==NULL){
    perror("Error while opening file.\n");
    exit(1);
  }


  /*counting directories in docfile*/
  noDirs = 0;
  while(getline(&line,&buf,fp)!=-1){
    noDirs++;
  }
  rewind(fp);
  if(noDirs<numWorkers){     //if directories are less than the workers reduce workers
    numWorkers = noDirs;
  }
  directories = malloc(sizeof(char *)*noDirs);
  if(directories==NULL){
    perror("Error while allocating memory.\n");
    exit(1);
  }
  /*parsing name of paths into array directories*/
  i=0;
  while((length = getline(&line,&buf,fp))!=-1){
    if(line[length-1]=='\n') line[length-1] = '\0'; //excluding \n from line
    directories[i] = malloc(sizeof(char)*length+1);
    strcpy(directories[i],line);
    i++;
  }
  /*make pipes*/
  if(makePipes(numWorkers)<0){
    exit(1);
  }

  for(i=0;i<numWorkers;i++){      //forkinf processes
      pid = fork();
      if(pid == -1){
        perror("Fork failed");
        exit(3);
      }
      worker = i;
      if(pid==0)  break;
  }

  if(pid == 0){   //child process

    fdWrite = malloc(sizeof(int));
    fdRead = malloc(sizeof(int));
    /*open pipe for reading */
    sprintf(pipe,"Executor2Worker%d",i);
    if((*fdRead=open(pipe, O_RDONLY |O_NONBLOCK  )) < 0){
      perror("4 fifo open problem"); exit(3);
    }
    /*opening pipe for writing */
    sprintf(pipe,"Worker2Executor%d",i);
    if((*fdWrite=open(pipe, O_WRONLY )) < 0){ /*pipes for writing wait for the read-end to be opened*/
       perror("3 fifo open problem"); exit(3);
    }

    int dirs;
    char command[BUFFERSIZE];
    char **dir_names;
    int j;
    char **queries;
    char temp[BUFFERSIZE];
    InvertedIndex **results;
    trieNode *root = NULL;
    char file[1024];
    char word[256];
    DIR *dir;
    struct dirent *d;
    int max,min,totalBytes,totalWords,totalLines;
    char max_path[BUFFERSIZE],min_path[BUFFERSIZE];
    InvertedIndex *count_result = NULL;

    while(1){
      strcpy(command,readString(*fdRead));
     //DIRNAME is sent by the parent process to inform children that the dirnames will be sent
      if(strcmp(command,"DIRNAMES")==0){
        /*read number of directories child will receive*/
        dirs = readInt(*fdRead);
        dir_names = getDirectories(dirs,*fdRead);

        for(j=0;j<dirs;j++){
          if((dir=opendir(dir_names[j]))!=NULL){
            while((d = readdir(dir))!=NULL){
                if(strcmp(d->d_name,".")!=0 && strcmp(d->d_name,"..")!=0){
                  strcpy(file,dir_names[j]);
                  strcat(file,"/");
                  strcat(file,d->d_name);
                  insertTextIntoTrie(&root,file);
                }
                memset(file,'\0',BUFFERSIZE);
            }
            closedir(dir);
          }
        }
        memset(command,'\0',BUFFERSIZE);
        strcpy(command,"READY");
        write(*fdWrite,command,BUFFERSIZE);
      }else if(strcmp(command,"/search")==0){
        int noQueries = readInt(*fdRead);   //reading number of queris
        InvertedIndex *final_result = NULL;

        queries = malloc(sizeof(char *)*noQueries);
        results = malloc(sizeof(InvertedIndex *)*(noQueries));
        for(j=0;j<noQueries;j++){   /*reading words of query*/
              strcpy(temp,readString(*fdRead));
              queries[j] = malloc(sizeof(char)*strlen(temp)+1);
              strcpy(queries[j],temp);
              
        }
        for(j=0;j<noQueries;j++){
          results[j] = Trie_Search(root,queries[j]);
        }
        final_result = CombineSearchResults(results,noQueries);
        SendSearchResults(final_result,*fdWrite);
        for(j=0;j<noQueries;j++){
          free(queries[j]);
          if(results[j]!=NULL){
            postingListFree(results[j]);
            free(results[j]);
          }
        }
        postingListFree(final_result);
        //free final_result too
      }else if(strcmp(command,"/exit")==0){
        TrieFree(root);
        for(j=0;j<dirs;j++){
          free(dir_names[j]);
        }
        free(dir_names);
        close(*fdWrite);
        close(*fdRead);
        free(fdRead);
        free(fdWrite);
        exit(1);
      }
    }
  /*parent process*/
  }else{
    fdWrite = malloc(sizeof(int)*numWorkers);
    fdRead = malloc(sizeof(int)*numWorkers);
    char command[BUFFERSIZE];
    int number,crWrite,crRead;
    /*open pipe ends for the communication between crawler and job executor*/
    bzero(pipe,BUFFERSIZE);
    strcpy(pipe,"Crawler2Executor");
    if((crRead=open(pipe, O_RDONLY |O_NONBLOCK  )) < 0){
      perror("46 fifo open problem"); exit(3);
    }

    /*open read and write ends of pipes for every child*/
    for(i=0;i<numWorkers;i++){
      sprintf(pipe,"Executor2Worker%d",i);
      if((fdWrite[i]=open(pipe, O_WRONLY)) < 0){    /*pipes for writing wait for the read-end to be opened*/
         perror("4 fifo open problem");
         return -1;
      }
      sprintf(pipe,"Worker2Executor%d",i);
      if((fdRead[i]=open(pipe, O_RDONLY | O_NONBLOCK)) < 0){
         perror("3 fifo open problem");
         return -1;
      }
    }

    sendDirectoryInfo(fdWrite,numWorkers,directories,noDirs);
    for(i=0;i<numWorkers;i++){
      if(strcmp(readString(fdRead[i]),"READY")!=0){
        //programm has to exit
      }
    }

    char *input=NULL,*token;
    int args,found;
    int j , totalBytes,totalWords,totalLines;
    int no,no2,each_max,general_max,each_min,general_min;
    char path[BUFFERSIZE] , temp[BUFFERSIZE],temp2[BUFFERSIZE],row[BUFFERSIZE];
    int deadline;
    char *line2 = NULL;

    while(1){
      /*parent process receives information from the crawler*/
      socketfd=readInt(crRead); //receives fd of the socket
      bzero(command,BUFFERSIZE);
      strcpy(command,readString(crRead)); //receives command
      if(command[strlen(command)-1] == '\n') command[strlen(command)-1]='\0';
      if(command[strlen(command)-1] == '\r') command[strlen(command)-1]='\0';
      if(!strcmp(command,"SHUTDOWN\r\n")){
        bzero(command,BUFFERSIZE);
        strcpy(command,"/exit");
        for(i=0;i<numWorkers;i++){      //parse search command to workers
          write(fdWrite[i],command,BUFFERSIZE);
        }
        for(i=0;i<numWorkers;i++){
          close(fdRead[i]);
          close(fdWrite[i]);
          sprintf(pipe,"Worker2Executor%d",i);
          remove(pipe);
          sprintf(pipe,"Executor2Worker%d",i);
          remove(pipe);
        }
        free(fdRead);
        free(fdWrite);
        while ((wait(&status)) > 0);
        close(crRead);
        exit(1);
      }else{
        bzero(temp,BUFFERSIZE);
        strcpy(temp,command);
        token=strtok(temp," \r\n");
        if(!strcmp(token,"SEARCH")){
          bzero(temp2,BUFFERSIZE);
          strcpy(temp2,"/search");
          for(i=0;i<numWorkers;i++){      //parse search command to workers
            write(fdWrite[i],temp2,BUFFERSIZE);
          }
          token = strtok(NULL," \r\n");
          no_queries=0;
          /*count number of words in the query*/
          while(token!=NULL){
            no_queries++;
            token = strtok(NULL," \r\n");
          }

          for(i=0;i<numWorkers;i++){      //parse search command to workers
            write(fdWrite[i],&no_queries,sizeof(int));
          }
          bzero(temp,BUFFERSIZE);
          strcpy(temp,command);
          token=strtok(temp," \r\n"); //skip word SEARCH
          token = strtok(NULL," \r\n");
          while(token!=NULL){
            if(!strcmp(token,"\n")){
              token = strtok(NULL," \r\n");
              continue;
            }
            for(i=0;i<numWorkers;i++){      //parse words to workers
              bzero(command,BUFFERSIZE);
              strcpy(command,token);

              write(fdWrite[i],command,BUFFERSIZE);
            }
            token = strtok(NULL," \r\n");
          }
        sleep(3);    //waiting for results
        number = numWorkers;
        found = 0;
        for(i=0;i<numWorkers;i++){
          bzero(command,BUFFERSIZE);
          if(read(fdRead[i],command,BUFFERSIZE)<=0){
            number--;
            continue;
          }
          if(strcmp(command,"NORES")==0) continue;
          no = readInt(fdRead[i]);  //read number of paths
          found = 1;
          for(j=0;j<no;j++){      //for every path read the name
            bzero(command,BUFFERSIZE);
            strcpy(path,readString(fdRead[i]));
            sprintf(command,"%s : %d \n",path,readInt(fdRead[i]));
            write(socketfd,command,BUFFERSIZE);
            read(fdRead[i],&no2,sizeof(int));
            int f=0;
            while(no2!=-1){
              bzero(command,BUFFERSIZE);
              strcpy(row,readString(fdRead[i]));
              sprintf(command,"%d,%d,%s\n",f,no2,row );
              f++;
              write(socketfd,command,BUFFERSIZE);
              no2=readInt(fdRead[i]);
            }
          }
        }
        bzero(command,BUFFERSIZE);
        sprintf(command,"%d/%d workers replied on time\n",number,numWorkers );
        write(socketfd,command,BUFFERSIZE);
        if(found==0){
          bzero(command,BUFFERSIZE);
          strcpy(command,"No results found\n");
          write(socketfd,command,BUFFERSIZE);
        }
        bzero(command,BUFFERSIZE);
      }


    }

    }

    for(i=0;i<numWorkers;i++){
      close(fdRead[i]);
      close(fdWrite[i]);
      sprintf(pipe,"Worker2Executor%d",i);
      remove(pipe);
      sprintf(pipe,"Executor2Worker%d",i);
      remove(pipe);
    }
    free(fdRead);
    free(fdWrite);

    while ((wait(&status)) > 0);
    printf("Bye\n");

  }
}
