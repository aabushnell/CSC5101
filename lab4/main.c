#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

atomic_cell_t **atomic_memory;
cell_t **nonatomic_memory;
void **memory;

_Thread_local uint64_t mark = 0;
_Thread_local TX tx;

void startTX() {
  for (size_t i = 0; i < MEM_SIZE; i++) {
    tx.reads[i] = 0;
    tx.writes[i] = 0;
    tx.counters[i] = 0;
  }

  struct timespec t0;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  mark = t0.tv_sec * 1000 + (t0.tv_nsec / 1000000);
}

uintptr_t read(int n) {
  cell_t *cursor = nonatomic_memory[n];
  if (cursor->counter >= mark) {
    return NULL;
  }

  tx.reads

      return (uintptr_t)cursor;
}

void write(int n, uintptr_t value) {}

bool commitTX() {}

void *pthread_main(void *arg) {
  thread_args_t *args = (thread_args_t *)arg;

  size_t id = args->id;
  printf("Hello I am thread %lu\n", id);

  struct timespec t0;
  struct timespec t1;

  clock_gettime(CLOCK_MONOTONIC, &t0);

  switch (args->algo) {
  case ATOMIC: {
  }
    for (size_t i = 0; i < 1024 * 1024; i++) {
      atomic_memory[0]->value++;
      atomic_memory[0]->counter++;
    }
    break;
  case MUTEX: {
  }
    for (size_t i = 0; i < 1024 * 1024; i++) {
      pthread_mutex_lock(args->lock);
      nonatomic_memory[0]->value++;
      nonatomic_memory[0]->counter++;
      pthread_mutex_unlock(args->lock);
    }
    break;
  case TM:
    for (size_t i = 0; i < 1024 * 1024; i++) {
    restart:
      startTX();
      int value = read(0); // read x
      if (value == INT64_MAX)
        goto restart;
      write(0, value + 1); // write x + 1
      if (!commitTX())     // try to commit
        goto restart;      // in case of abort, restart
    }
    break;
  case NA:
    break;
  }

  clock_gettime(CLOCK_MONOTONIC, &t1);

  args->dur_ms = (int64_t)(t1.tv_sec - t0.tv_sec) * 1000 +
                 (t1.tv_nsec - t0.tv_nsec) / 1000000;

  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "[ERROR] needs (2) args: N, algo\n");
    return EXIT_FAILURE;
  }
  size_t N = atoi(argv[1]);

  algo_t algo = NA;
  if (strcmp(argv[2], "atomic") == 0) {
    algo = ATOMIC;
  }
  if (strcmp(argv[2], "mutex") == 0) {
    algo = MUTEX;
  }
  if (strcmp(argv[2], "tm") == 0) {
    algo = TM;
  }
  if (algo == NA) {
    fprintf(stderr, "[ERROR] unsuported algo: %s\n", argv[2]);
    return EXIT_FAILURE;
  }

  atomic_memory = calloc(MEM_SIZE, sizeof(*atomic_memory));
  for (size_t i = 0; i < MEM_SIZE; i++) {
    atomic_memory[i] = calloc(1, sizeof(*atomic_memory[i]));
    atomic_memory[i]->value = i;
  }

  nonatomic_memory = calloc(MEM_SIZE, sizeof(*nonatomic_memory));
  for (size_t i = 0; i < MEM_SIZE; i++) {
    nonatomic_memory[i] = calloc(1, sizeof(*nonatomic_memory[i]));
    nonatomic_memory[i]->value = i;
  }

  switch (algo) {
  case ATOMIC:
    memory = (void **)atomic_memory;
    break;
  case MUTEX:
    memory = (void **)nonatomic_memory;
    break;
  case NA:
    break;
  }

  pthread_t threads[N];
  thread_args_t *args = calloc(N, sizeof(*args));

  pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  printf("Starting up w/ %lu threads!\n", N);
  for (size_t i = 0; i < N; i++) {
    args[i].id = i;
    args[i].algo = algo;
    args[i].lock = &lock;
    args[i].memory = memory;
    pthread_create(&threads[i], NULL, pthread_main, &args[i]);
  }

  uint64_t t_avg_ms = 0;

  for (size_t i = 0; i < N; i++) {
    pthread_join(threads[i], NULL);
    t_avg_ms += args[i].dur_ms / N;
  }

  printf("EXPECTED = %lu\n", 1024 * 1024 * N);

  switch (algo) {
  case ATOMIC:
    printf("MEM[0] = %lld\n", ((atomic_cell_t **)memory)[0]->value);
    break;
  case MUTEX:
    printf("MEM[0] = %lld\n", ((cell_t **)memory)[0]->value);
    break;
  case NA:
    break;
  }

  printf("AVG TIME = %lld ms\n", t_avg_ms);

  return EXIT_SUCCESS;
}
