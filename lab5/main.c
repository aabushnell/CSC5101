#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

struct nvmm *nvmm;
_Atomic size_t head;

#include "nvmm.h"
#include "struct.h"

#define NVMM_SIZE 128 * 1024 * 1024

double time_now() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1e0 + t.tv_nsec * 1e-9;
}

int my_pwrite(int fd, const void *buf, size_t n, off_t off) {
  if (n > CACHE_LINE)
    return -1;

  while (head + 1 == nvmm->tail) {
    sleep(10);
  }

  // reserve entry
  size_t i = atomic_fetch_add(&head, 1);
  struct operation *cur_op = &nvmm->operation[i];

  // buf
  memcpy(cur_op->buf, buf, n);
  pwb(&cur_op->second_cache_line);

  // meta
  cur_op->fd = fd;
  cur_op->n = n;
  cur_op->off = off;
  pwb(&cur_op->first_cache_line);

  pfence();

  // commit
  cur_op->committed = true;
  pwb(&cur_op->first_cache_line);

  psync();

  return 0;
}

void *cleanup(void *arg) {
  while (true) {
    struct operation *tail_op = &nvmm->operation[nvmm->tail];
    if (!tail_op->committed) {
      sleep(10);
    }

    pwrite(tail_op->fd, tail_op->buf, tail_op->n, tail_op->off);

    int ret = fsync(tail_op->fd);
    if (ret == -1) {
      fprintf(stderr, "[ERROR]: fsync failed\n");
      exit(EXIT_FAILURE);
    }

    nvmm->tail++;
  }
}

int main(void) {
  printf("NVMM!\n");

  // -- setup --

  int fd_m, fd_o, ret;
  void *addr;

  fd_o = open("out.dat", O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
  if (fd_o == -1)
    err(EXIT_FAILURE, "open out");

  fd_m = open("nvmm.dat", O_RDWR | O_CREAT, S_IRWXU);
  if (fd_m == -1)
    err(EXIT_FAILURE, "open nvmm");
  ret = ftruncate(fd_m, NVMM_SIZE);
  if (ret == -1)
    err(EXIT_FAILURE, "ftruncate nvmm");
  addr = mmap(NULL, NVMM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_m, 0);
  if (addr == MAP_FAILED)
    err(EXIT_FAILURE, "mmap nvmm");

  nvmm = (struct nvmm *)addr;

  pthread_t tid;
  pthread_create(&tid, NULL, cleanup, NULL);

  // -- pwrite --

  double t0, t1, delta_t;

  t0 = time_now();

  for (size_t i = 0; i < 1024; i++) {
    pwrite(fd_o, "hello", 5, 5 * i);
  }

  t1 = time_now();

  delta_t = (t1 - t0) * 1e6;

  printf("[PWRITE]: %f ms per op\n", delta_t / 1024);

  t0 = time_now();

  for (size_t i = 0; i < 1024; i++) {
    pwrite(fd_o, "hello", 5, 5 * i);
    fsync(fd_o);
  }

  t1 = time_now();

  delta_t = (t1 - t0) * 1e6;

  printf("[PWRITE]: %f ms per op\n", delta_t / 1024);

  t0 = time_now();

  for (size_t i = 0; i < 1024; i++) {
    my_pwrite(fd_o, "hello", 5, 5 * i);
  }

  t1 = time_now();

  delta_t = (t1 - t0) * 1e6;

  printf("[MY_PWRITE]: %f ms per op\n", delta_t / 1024);

  return EXIT_SUCCESS;
}
