#include <pthread.h>

struct Requests{
  int *fds;           /*array of fds*/
  char *root_dir;
  int active_fds;     /*fds in the array*/
  int responses;      /*responses sent*/
  long int bytes;     /*bytes sent*/
  int terminate;
  pthread_cond_t buffer_full; /*condition for full buffer of fds*/
  pthread_cond_t buffer_empty; /*condition for empty buffer of fds*/
  pthread_mutex_t buffer_full_mtx;  /*mutex for the full condiotion*/
  pthread_mutex_t buffer_empty_mtx;  /*mutex for the empty condiotion*/
  pthread_mutex_t mtx;          /*mutex for reading in writing in buffer*/
};

typedef struct Requests Requests;

Requests *BFR_Init(int bsize,char *);
int BFR_PickFd(Requests *buffer);
char *read_request(int socket);
long int send_response(int socket,char *page);
void FDR_free(Requests *pool);
