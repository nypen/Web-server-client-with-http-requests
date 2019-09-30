#include <pthread.h>

struct threadpool{
  int n;                /*number of threads*/
  pthread_t **threads;
};

typedef struct threadpool threadpool;

void *thread_function(void *argp);
threadpool *TP_Init(int n,void(*function)(void *args),void *);
void TP_addjob(threadpool *thpool,void **args);
void *TP_pickjob(threadpool *thpool);
