#include <linux/futex.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define N 10
#define IT 10
#define CSD 1000
#define DCSD 2000

// POSIX LOCK
pthread_mutex_t posix_lock = PTHREAD_MUTEX_INITIALIZER;

// SPIN LOCK

_Atomic int spinlock = 0;

void spinlock_lock(_Atomic int *lock) {
  while (atomic_exchange(lock, 1) != 0) {
    /* spin */
  }
}

void spinlock_unlock(_Atomic int *lock) { atomic_store(lock, 0); }

// TICKET LOCK

typedef struct ticket_lock {
  _Atomic int ticket;
  _Atomic int screen;
} ticket_lock_t;

ticket_lock_t ticket_lock = {0, 0};

void ticketlock_lock(ticket_lock_t *lock) {
  int n = atomic_fetch_add(&lock->ticket, 1);
  while (atomic_load(&lock->screen) < n) {
    /* spin */
  }
}

void ticketlock_unlock(ticket_lock_t *lock) {
  atomic_fetch_add(&lock->screen, 1);
}

// MCS LOCK

typedef struct node {
  struct node *_Atomic next;
  _Atomic bool is_free;
} mcs_node_t;

_Thread_local mcs_node_t thread_mcs_node;
mcs_node_t *_Atomic mcs_lock;

void mcslock_lock() {
  thread_mcs_node.next = NULL;
  thread_mcs_node.is_free = false;

  mcs_node_t *end = atomic_exchange(&mcs_lock, &thread_mcs_node);
  if (end != NULL) {
    atomic_store(&end->next, &thread_mcs_node);
    while (atomic_load(&thread_mcs_node.is_free)) {
      /* spin */
    }
  }
}

void mcslock_unlock() {
  mcs_node_t *exp = &thread_mcs_node;

  if (!atomic_load(&thread_mcs_node.next) &&
      atomic_compare_exchange_strong(&mcs_lock, &exp, NULL)) {
    return;
  }

  // else
  while (!atomic_load(&thread_mcs_node.next)) {
    /* spin */
  }
  atomic_store(&thread_mcs_node.next->is_free, true);
}

typedef enum algo { NONE, POSIX, SPIN, TICKET, MCS } algo_t;

typedef struct thread_args {
  int tid;
  int it;
  int csd;
  int dcsd;

  long lock_time;
  long unlock_time;

  algo_t algo;
} thread_args;

void wait_dur(int dur) {
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  long start = time.tv_nsec;
  long cur = start;

  while (cur <= start + dur) {
    clock_gettime(CLOCK_MONOTONIC, &time);
    cur = time.tv_nsec;
  }
}

void *benchmark(void *arg) {
  thread_args *args = (thread_args *)arg;
  printf("  [%d]: thread starting\n", args->tid);

  struct timespec time;

  for (int i = 0; i < args->it; i++) {
    clock_gettime(CLOCK_MONOTONIC, &time);
    long pre = time.tv_nsec;
    // acquire lock
    switch (args->algo) {
    case NONE:
      break;
    case POSIX:
      pthread_mutex_lock(&posix_lock);
      break;
    case SPIN:
      spinlock_lock(&spinlock);
      break;
    case TICKET:
      ticketlock_lock(&ticket_lock);
      break;
    case MCS:
      mcslock_lock();
      break;
    }
    clock_gettime(CLOCK_MONOTONIC, &time);
    long post = time.tv_nsec;
    args->lock_time += (post - pre);

    printf("  [%d]: executing critical section (it = %d)\n", args->tid, i);
    wait_dur(args->csd);

    clock_gettime(CLOCK_MONOTONIC, &time);
    pre = time.tv_nsec;
    // release lock
    switch (args->algo) {
    case NONE:
      break;
    case POSIX:
      pthread_mutex_unlock(&posix_lock);
      break;
    case SPIN:
      spinlock_unlock(&spinlock);
      break;
    case TICKET:
      ticketlock_unlock(&ticket_lock);
      break;
    case MCS:
      mcslock_unlock();
      break;
    }
    clock_gettime(CLOCK_MONOTONIC, &time);
    post = time.tv_nsec;
    args->unlock_time += (post - pre);

    wait_dur(args->dcsd);
  }

  return NULL;
}

algo_t parse_algo(char *algo_str) {
  if (strcmp(algo_str, "posix") == 0) {
    return POSIX;
  }
  if (strcmp(algo_str, "spin") == 0) {
    return SPIN;
  }
  if (strcmp(algo_str, "ticket") == 0) {
    return TICKET;
  }
  if (strcmp(algo_str, "mcs") == 0) {
    return MCS;
  }

  return NONE;
}

void bench(int n, int it, int csd, int dcsd, char *algo) {
  printf(">> Executing: %s\n", algo);

  algo_t alg = parse_algo(algo);

  pthread_t threads[N];
  thread_args args[N];

  for (int i = 0; i < N; i++) {
    args[i] = (thread_args){i, it, csd, dcsd, 0, 0, alg};
    pthread_create(&threads[i], NULL, benchmark, &args[i]);
  }

  for (int i = 0; i < N; i++) {
    pthread_join(threads[i], NULL);
  }

  for (int i = 0; i < N; i++) {
    printf("Thread %d took on avg %ld ns to lock and %ld ns to unlock\n", i,
           args[i].lock_time / it, args[i].unlock_time / it);
  }
}

int main(int argc, char **argv) {
  printf("Testing Lock Algorithms\n");

  bench(N, IT, CSD, DCSD, "posix");

  return EXIT_SUCCESS;
}
