#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

#define MYSIG 278

threadpool *TP_Init(int n,void(*function)(void *args),void *arguments){
  threadpool *thpool;
  int i,error;
  thpool = (threadpool *)(malloc(sizeof(threadpool)));
  if(thpool==NULL){
    printf("Error while allocating memory\n");
    return NULL;
  }
  thpool->n = n;
  thpool->threads = malloc(sizeof(pthread_t *)*thpool->n);
  for(i=0;i<thpool->n;i++){
    thpool->threads[i]=malloc(sizeof(pthread_t));
    //thread is created , with threads[i] the whole pool and thread info is given as arguments
    if((error=pthread_create(thpool->threads[i],NULL,(void *)function,arguments))!=0){
      printf("Error while creating threads\n");
      return NULL;
    }
  }
  return thpool;
}
