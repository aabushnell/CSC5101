#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#define MEM_SIZE 1

typedef enum { NA, ATOMIC, MUTEX, TM } algo_t;

typedef struct cell_t {
  int64_t value;
  uint64_t counter;
} cell_t;

typedef struct thread_args {
  size_t id;
  algo_t algo;
  pthread_mutex_t *lock;
  void **memory;
  uint64_t dur_ms;
} thread_args_t;

typedef struct TX {
  int64_t reads[MEM_SIZE];
  int64_t writes[MEM_SIZE];
  uint64_t counters[MEM_SIZE];
} TX;
