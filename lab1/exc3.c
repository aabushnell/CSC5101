#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_THREADS 5

typedef struct node {
  char *line;
  struct node *next;
  _Atomic int count;
} node_t;

typedef struct queue {
  node_t *top;
  node_t *bottom;
} queue_t;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t new_line = PTHREAD_COND_INITIALIZER;
pthread_cond_t line_consumed = PTHREAD_COND_INITIALIZER;

node_t *walk_queue(queue_t *queue, node_t *last) {
  if (last == NULL) {
    return queue->bottom;
  } else {
    node_t *cursor = queue->bottom;
    while (cursor->next != NULL) {
      if (cursor == last) {
        return cursor->next;
      }
    }
    return queue->bottom;
  }
}

void *thread_main(void *arg) {

  queue_t *queue = (queue_t *)arg;

  pthread_t tid = pthread_self();
  printf("[%lu] worker started\n", (unsigned long)tid);

  node_t *last = NULL;

  while (true) {
    pthread_mutex_lock(&queue_lock);
    while (queue->bottom == NULL || last == queue->top) {
      pthread_cond_wait(&new_line, &queue_lock);
    }
    node_t *current = walk_queue(queue, last);
    pthread_mutex_unlock(&queue_lock);

    while (current != NULL) {
      printf("[%lu] worker reading node %p\n", (unsigned long)tid, current);

      printf("[%lu] %s", (unsigned long)tid, current->line);
      current->count += 1;

      node_t *next = current->next;
      last = current;

      if (current->count == N_THREADS) {
        printf("[%lu] last thread, count = %d\n", (unsigned long)tid,
               current->count);
        queue->bottom = current->next;
        free(current->line);
        free(current);
      }

      current = next;
    }
  }

  node_t *node = queue->bottom;
  while (node != NULL) {
    node->count += 1;

    if (node->count == N_THREADS) {
    }

    node = node->next;
  }
  pthread_mutex_unlock(&queue_lock);
}

int main(int argc, char **argv) {
  printf("~~Excercise 3~~\n");

  queue_t queue = {NULL, NULL};

  pthread_t threads[N_THREADS];

  for (int i = 0; i < N_THREADS; i++) {
    pthread_create(&threads[i], NULL, thread_main, &queue);
  }

  char buf[100];
  while (fgets(buf, 100, stdin) != NULL) {
    node_t *node = calloc(1, sizeof(node_t));

    char *line = calloc(strlen(buf) + 1, sizeof(char));
    strncpy(line, buf, 100);

    pthread_mutex_lock(&queue_lock);
    if (queue.top == NULL) {
      queue.top = node;
      queue.bottom = node;
    } else if (queue.bottom->next == NULL) {
      queue.bottom->next = node;
      queue.top = node;
    } else {
      queue.top->next = node;
      queue.top = node;
    }
    node->line = line;
    node->count = 0;
    printf("main adding node %p", node);
    pthread_cond_signal(&new_line);
    pthread_mutex_unlock(&queue_lock);
  }

  for (int i = 0; i < N_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  return EXIT_SUCCESS;
}
