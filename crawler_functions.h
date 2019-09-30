#include <pthread.h>

struct job_node{
  char *page;
  struct job_node *next;
};
typedef struct job_node job_node;

struct job_queue{
  job_node *head;       /*pointer to the first url of the queue*/
  job_node *rear;       /*pointer to the last url of the queue*/
  job_node *next_job;   /*pointer to the url that has to be requested next*/
  pthread_mutex_t mtx;  /*mutex for reading and writing in the queue*/
  pthread_cond_t empty_queue;     /*condition for empty queue*/
  pthread_mutex_t empty_queue_mtx;  /*mutex for empty queue*/
  int n_jobs;                     /*number of jobs in the queue*/
};
typedef struct job_queue job_queue;

struct thread_info{
  int port;           /*port to send requests*/
  int terminate;      /*variable to inform threads to terminate*/
  char *save_dir;     /*directory where downloaded pages are saved*/
  job_queue *jobs;    /*pointer to the queue where jobs are kept*/
  int active_threads; /*number of threads that are currently working on a request*/
  long int bytes;     /*bytes receives*/
  int downloads;      /*number of downloaded pages*/
  struct hostent *host_address;     /*address to make requests*/
  pthread_mutex_t counter_mtx;      /*mutex for counting downloads*/
  pthread_mutex_t active_mtx;       /*mutex for counting active threads*/
};

typedef struct thread_info thread_info;

void makeDocfile(char *,char *);
long int ReceiveResponse(int ,char *,char *,job_queue *);
void GET(char *,int);

job_queue *JQ_Init();
void JQ_Addjob(job_queue *qjob,char *page);
char *JQ_Pickjob(job_queue *qjob);
thread_info *THR_Init(char *page,int,char *,struct hostent *);
void THR_free(thread_info *thr_info);
