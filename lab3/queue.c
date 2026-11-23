#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <err.h>

#define N 10
#define K (1000 * 1000)

typedef struct node_s {
  struct node_s* _Atomic next;
  int value;
} node_t;

typedef struct queue_s {
  pthread_mutex_t lock;
  _Alignas(64) node_t* _Atomic head;
  _Alignas(64) node_t* _Atomic tail;
} queue_t;

queue_t queue;
int impl;

void init_queue() {
  pthread_mutex_init(&queue.lock, NULL);

  node_t *sentinel = malloc(sizeof(*sentinel));
  sentinel->next = NULL;
  sentinel->value = INT_MAX;

  queue.head = sentinel;
  queue.tail = sentinel;
}

void enqueue_lock(int value) {
  node_t *n = malloc(sizeof(*n));
  n->next = NULL;
  n->value = value;

  // crtitical section
  pthread_mutex_lock(&queue.lock);

  queue.tail->next = n;
  queue.tail = n;

  pthread_mutex_unlock(&queue.lock);
}

int dequeue_lock() {
  // crtitical section
  pthread_mutex_lock(&queue.lock);

  node_t *old_head = queue.head;
  node_t *real_head = queue.head->next;

  if (real_head == NULL) {
    pthread_mutex_unlock(&queue.lock);
    return INT_MAX;
  }

  queue.head = real_head;
  pthread_mutex_unlock(&queue.lock);

  free(old_head);
  return real_head->value;
}

void enqueue_lockfree(int value) {
  node_t *n = malloc(sizeof(*n));
  n->next = NULL;
  n->value = value;

retry:
  {}
  node_t *obv_tail = queue.tail;

  while (obv_tail->next != NULL) {
    atomic_compare_exchange_strong(&queue.tail, &obv_tail, obv_tail->next);
    obv_tail = queue.tail;
  }

  node_t *expected = NULL;
  if (atomic_compare_exchange_strong(&obv_tail->next, &expected, n)) {
    atomic_compare_exchange_strong(&queue.tail, &obv_tail, n);
  } else {
    goto retry;
  }
}

int dequeue_lockfree() {
retry:
  {}
  node_t *obv_head = queue.head;

  if (obv_head->next == NULL)
    return INT_MAX;

  if (atomic_compare_exchange_strong(&queue.head, &obv_head, obv_head->next)) {
    return obv_head->next->value;
  } else {
    goto retry;
  }
}

void *thread_main(void *arg) {
  int tid = *(int *)arg;
  printf("[T#%d] starting...\n", tid);

  if (impl == 0) {
    for (size_t i = 0; i < K; i++) {
      enqueue_lock(i);
      dequeue_lock();
    }
  } else {
    for (size_t i = 0; i < K; i++) {
      enqueue_lockfree(i);
      dequeue_lockfree();
    }
  }

  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  printf("Lock-free queue!\n");

  if (argc != 2)
    err(EXIT_FAILURE, ":<");

  impl = atoi(argv[1]);
  if (impl != 0 && impl != 1)
    err(EXIT_FAILURE, ":<");

  init_queue();

  pthread_t threads[N];
  int tid[N];

  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  for (size_t i = 0; i < N; i++) {
    tid[i] = i;
    pthread_create(&threads[i], NULL, thread_main, &tid[i]);
  }

  for (size_t i = 0; i < N; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("All threads joined!\n");
  clock_gettime(CLOCK_MONOTONIC, &t1);

  double dur_ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) * 1e-6;

  printf("Elapsed time: %f ms\n", dur_ms);

  if (queue.head->next == NULL) {
    printf("Queue verified!\n");
  } else {
    printf("Queue corrupted :<\n");
  }

  return EXIT_SUCCESS;
}
