#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

_Atomic unsigned int counter = 0;

pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t counter_signal = PTHREAD_COND_INITIALIZER;

void *thread_main(void *arg) {
  int id = *(int *)arg;
  pthread_t tid = pthread_self();

  printf("[%lu] Hello, World %d!!!\n", (unsigned long)tid, id);

  counter += 1;
  pthread_cond_signal(&counter_signal);

  return NULL;
}

int main(int argc, char **argv) {
  printf("~~Excercise 1~~\n");

  int N = atoi(argv[1]);

  pthread_t threads[N];
  int ids[N];

  for (int i = 0; i < N; i++) {
    ids[i] = i;
    pthread_create(&threads[i], NULL, thread_main, &ids[i]);
  }

  pthread_mutex_lock(&counter_lock);
  while (counter != 100) {
    pthread_cond_wait(&counter_signal, &counter_lock);
  }
  printf("Main quitting, counter = %d\n", counter);
  pthread_mutex_unlock(&counter_lock);

  return EXIT_SUCCESS;
}
