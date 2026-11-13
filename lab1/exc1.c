#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void *thread_main(void *arg) {
  int id = *(int *)arg;
  pthread_t tid = pthread_self();

  printf("[%lu] Hello, World %d!!!\n", (unsigned long)tid, id);
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

  for (int i = 0; i < N; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("Main quitting\n");

  return EXIT_SUCCESS;
}
