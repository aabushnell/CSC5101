#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <stdatomic.h>
#include <unistd.h>

#define N_CELLS     128
#define N_THREADS   20
#define N_LOOPS     (1024 * 1024)

typedef enum version_e {
  STM, STM_BACK, LOCK, ATOMIC
} version_t;

typedef struct cell_s {
  _Atomic int value;
  _Atomic int counter;
} cell_t;

typedef struct memory_s {
  pthread_mutex_t lock;
  _Atomic(cell_t*) cells[N_CELLS];
  _Atomic uint64_t clock;
} memory_t;

typedef struct tx_s {
  bool pending_reads[N_CELLS];
  int pending_writes[N_CELLS];
  uint64_t mark;
  uint64_t backoff;
} tx_t;

version_t version;
memory_t memory;
_Thread_local tx_t tx;

cell_t *new_cell(int value, int counter) {
  cell_t *res = malloc(sizeof(*res));
  res->value = value;
  res->counter = counter;
  return res;
}

void init_memory() {
  pthread_mutex_init(&memory.lock, NULL);
  for (size_t i = 0; i < N_CELLS; i++) {
    memory.cells[i] = new_cell(0, 0);
  }
  memory.clock = 1;
}

void backoff() {
  int t = 1 + rand() % tx.backoff;
  usleep(t);
  if (tx.backoff < 512)
    tx.backoff *= 2;
}

void startTX() {
  for (size_t i = 0; i < N_CELLS; i++) {
    tx.pending_reads[i] = false;
    tx.pending_writes[i] = INT_MAX;
  }

  tx.mark = atomic_load_explicit(&memory.clock, memory_order_acquire);
}

int readTX(int n) {
  if (tx.pending_writes[n] != INT_MAX)
    return tx.pending_writes[n];

  cell_t* cur = atomic_load_explicit(&memory.cells[n], memory_order_acquire);
  if (cur->counter >= tx.mark)
    // ABORT
    return INT_MAX;

  tx.pending_reads[n] = true;
  return cur->value;
}

void writeTX(int n, int value) {
  tx.pending_writes[n] = value;
}

bool commitTX() {
  pthread_mutex_lock(&memory.lock);

  for (size_t i = 0; i < N_CELLS; i++) {
    if (tx.pending_reads[i]
        && memory.cells[i]->counter >= tx.mark) {
      // ABORT
      pthread_mutex_unlock(&memory.lock);
      return false;
    }
  }

  for (size_t i = 0; i < N_CELLS; i++) {
    if (tx.pending_writes[i] != INT_MAX) {
      cell_t *new = new_cell(tx.pending_writes[i], memory.clock);
      atomic_store_explicit(&memory.cells[i], new, memory_order_release);
    }
  }

  atomic_store_explicit(&memory.clock, memory.clock + 1, memory_order_release);
  pthread_mutex_unlock(&memory.lock);

  tx.backoff = 1;
  return true;
}

void *thread_main(void *arg) {
  size_t tid = *(size_t *)arg;

  tx.backoff = 1;
  int n;

  switch (version) {
    case STM:
      printf("[th #%2lu]: running STM\n", tid);
      for (size_t i = 0; i < N_LOOPS; i++) {
        n = rand() % N_CELLS;
      restart:
        startTX();
        int value = readTX(n);
        if(value == INT_MAX) {
          goto restart;
        }
        writeTX(n, value + 1);
        if (!commitTX()) {
          goto restart;
        }
      }
      break;
    case STM_BACK:
      printf("[th #%2lu]: running STM_BACK\n", tid);
      for (size_t i = 0; i < N_LOOPS; i++) {
        n = rand() % N_CELLS;
      restart_back:
        startTX();
        int value = readTX(n);
        if(value == INT_MAX) {
          backoff();
          goto restart_back;
        }
        writeTX(n, value + 1);
        if (!commitTX()) {
          backoff();
          goto restart_back;
        }
      }
      break;
    case LOCK:
      printf("[th #%2lu]: running LOCK\n", tid);
      for (size_t i = 0; i < N_LOOPS; i++) {
        n = rand() % N_CELLS;
        pthread_mutex_lock(&memory.lock);
        memory.cells[n]->value++;
        pthread_mutex_unlock(&memory.lock);
      }
      break;
    case ATOMIC:
      printf("[th #%2lu]: running ATOMIC\n", tid);
      for (size_t i = 0; i < N_LOOPS; i++) {
        n = rand() % N_CELLS;
        atomic_fetch_add(&memory.cells[n]->value, 1);
      }
      break;
  }

  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  if (argc != 2)
    errx(EXIT_FAILURE, "required: version [0: STM, 1: STM_BACK, 2: LOCK, 3: ATOMIC]");

  size_t v = atoi(argv[1]);
  if (v > 3)
    errx(EXIT_FAILURE, "unsupported version: %lu", v);

  version = v;

  init_memory();

  pthread_t threads[N_THREADS];
  size_t tid[N_THREADS];

  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  for (size_t i = 0; i < N_THREADS; i++) {
    tid[i] = i;
    pthread_create(&threads[i], NULL, thread_main, &tid[i]);
  }

  for (size_t i = 0; i < N_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  clock_gettime(CLOCK_MONOTONIC, &t1);

  uint64_t elapsed_ms = (
    (t1.tv_sec - t0.tv_sec) * 1e3
    + (t1.tv_nsec - t0.tv_nsec) * 1e-6
  );

  printf("total time elapsed: %llu ms\n", elapsed_ms);

  int expected = N_LOOPS * N_THREADS;
  int actual = 0;
  for (size_t i = 0; i < N_CELLS; i++) {
    actual += memory.cells[i]->value;
  }

  printf("expected: %d, actual: %d\n", expected, actual);
  if (expected == actual) {
    printf("transaction validated!\n");
  } else {
    printf("transaction failed :<\n");
  }

  return EXIT_SUCCESS;
}
