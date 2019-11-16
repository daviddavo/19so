#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

int total_sum = 0;
sem_t semaphore;

void * partial_sum(void * arg) {
  int j = 0;
  int ni=((int*)arg)[0];
  int nf=((int*)arg)[1];

  for (j = ni; j < nf; j++) {
    sem_wait(&semaphore);
    total_sum = total_sum + j;
    sem_post(&semaphore);
  }

  pthread_exit(0);
}

int main(int argc, char * argv[]) {
  pthread_t * th;
  int * num;
  int n;
  int gap;
  int rem;
  int nthreads;

  if (argc != 3) {
    fprintf(stderr, "Incorrect number of arguments\n");
    exit(1);
  }

  if ((nthreads = atoi(argv[1])) <= 0 || (n = atoi(argv[2])) <= 0) {
    fprintf(stderr, "Argument must be a number greater than 0\n");
    exit(1);
  }

  if (nthreads > n) {
    fprintf(stderr, "Number of threads can't be greater than end value\n");
    exit(1);
  }

  th = calloc(sizeof(pthread_t), nthreads+1);
  num = calloc(sizeof(int), nthreads+2);
  gap = n / nthreads;
  rem = n % nthreads;

  for (int i = 0; i <= nthreads; ++i) {
    num[i] = i * gap;
  }

  if (rem != 0) {
    num[++nthreads] = n;
  }

  for (int i = 0; i <= nthreads; ++i) {
    printf("num[%d]=%d\n", i, num[i]);
  }

  num[nthreads]++;

  sem_init(&semaphore, 0, 1);
  for (int i = 0; i < nthreads; ++i) {
    pthread_create(&th[i], NULL, partial_sum, (void*) &num[i]);
  }

  for (int i = 0; i < nthreads; ++i) {
    pthread_join(th[i], NULL);
  }
  sem_destroy(&semaphore);

  printf("total_sum=%d and it should be %d\n", total_sum, n*(n+1)/2);

  free(th);
  free(num);

  return 0;
}
