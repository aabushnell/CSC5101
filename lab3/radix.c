#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

typedef struct node_s {
  _Atomic int* value;
  _Atomic struct node_s* nexts[256];
} node_t;

_Atomic node_t* root;
pthread_mutex_t lock;

char* words[10] = {
  "peter",
  "piper",
  "picked",
  "pet",
  "pip",
  "pitcher",
  "pillow",
  "pit",
  "pick",
  "lmao"
};

node_t* get_next(_Atomic(node_t*)* pa) {
  _Atomic(node_t*) p = atomic_load(pa);
  if (p == NULL) {
    node_t* new = malloc(sizeof(*new));
    new->value = NULL;
    for (int i = 0; i < 256; i++)
      new->nexts[i] = NULL;

    node_t* exp = NULL;
    if (atomic_compare_exchange_strong(&p, &exp, new)) {
      return new;
    } else {
      p = atomic_load(pa);
      return p;
    }
  } else {
    return p;
  }
}

void* insert(char* key, void* value) {
  node_t* cur = atomic_load(&root);
  while (*key != '\0') {
    node_t* next = cur->nexts[*key];
    next = get_next(&next);
    cur = &next;
    key++;
  }
  void* old = cur->value;
  (*cur)->value = value;
  return old;
}

void* get(char* key) {
  node_t** cur = &root;
  while (*key != '\0') {
    node_t* next = (*cur)->nexts[*key];
    next = get_next(&next);
    cur = &next;
    key++;
  }
  return (*cur)->value;
}

void* thread_main(void* arg) {
  int* values = (int *)arg;

  for (size_t i = 0; i < 10; i++) {
    insert(words[i], &values[i]);
  }

  for (size_t i = 0; i < 10; i++) {
    get(words[i]);
  }

  return EXIT_SUCCESS;
}

int main(void) {
  printf("Radix!\n");
  root = get_next(&root);

  pthread_mutex_init(&lock, NULL);

  pthread_t threads[10];
  int** values = malloc(sizeof(*values) * 10);
  for (size_t i = 0; i < 10; i++) {
    values[i] = malloc(sizeof(int) * 10);
  }

  for (size_t i = 0; i < 10; i++) {
    pthread_create(&threads[i], NULL, thread_main, values[10]);
  }

  for (size_t i = 0; i < 10; i++) {
    pthread_join(threads[i], NULL);
  }

  return EXIT_SUCCESS;
}
