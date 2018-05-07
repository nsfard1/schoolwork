#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>

#ifndef NUM_PHILOSOPHERS
  #define NUM_PHILOSOPHERS 5
#endif

#define STATUS_NUM_CHAR (NUM_PHILOSOPHERS + 8)
#define EATING 0
#define THINKING 1
#define CHANGING 2
#define FIRST_PHIL 'A'

typedef struct philosopher {
  char name;
  int position;
  int status;
  int fork1;
  int fork2;
  int num_forks;
  int cycles_left;
} philosopher;

philosopher philosophers[NUM_PHILOSOPHERS];
pthread_mutex_t fork_mutexes[NUM_PHILOSOPHERS];
pthread_mutex_t status_mutex;
int exit_ok = 0;

/*
* Dr. Phillip Nico's random sleep function from the Assignment 3 specification
*
* sleep for a random amount of time between 0 and 999
* milliseconds. This routine is somewhat unreliable, since it
* doesn’t take into account the possiblity that the nanosleep
* could be interrupted for some legitimate reason.
*
* nanosleep() is part of the realtime library and must be linked
* with –lrt
*/
void dawdle() {
  struct timespec tv;
  int msec = (int)(((double)random() / INT_MAX) * 1000);
  tv.tv_sec = 0;
  tv.tv_nsec = 1000000 * msec;
  if (-1 == nanosleep(&tv, NULL)) {
    perror("nanosleep");
  }
}

void print_status() {
  int i, j;

  printf("\n|");

  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    printf(" ");
    for (j = 0; j < NUM_PHILOSOPHERS; j++) {
      if (philosophers[i].num_forks == 0 || (philosophers[i].fork1 != j &&
       philosophers[i].fork2 != j)) {
        printf("-");
      }
      else if (philosophers[i].num_forks == 1) {
        if ((philosophers[i].position % 2 == 0 && philosophers[i].fork2 == j) ||
         (philosophers[i].position % 2 != 0 && philosophers[i].fork1 == j)) {
          printf("%d", j);
        }
        else {
          printf("-");
        }
      }
      else {
        if (philosophers[i].fork1 == j || philosophers[i].fork2 == j) {
          printf("%d", j);
        }
        else {
          printf("-");
        }
      }
    }

    if (philosophers[i].status == EATING) {
      printf(" Eat   ");
    }
    else if (philosophers[i].status == THINKING) {
      printf(" Think ");
    }
    else {
      printf("       ");
    }

    printf("|");
  }
}

void print_header() {
  int i, j;

  printf("|");
  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    for (j = 0; j < STATUS_NUM_CHAR; j++) {
      printf("=");
    }

    printf("|");
  }

  printf("\n|");
  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    for (j = 0; j < STATUS_NUM_CHAR/2; j++) {
      printf(" ");
    }

    printf("%c", philosophers[i].name);

    for (j = 0; j < STATUS_NUM_CHAR/2; j++) {
      printf(" ");
    }

    printf("|");
  }

  printf("\n|");
  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    for (j = 0; j < STATUS_NUM_CHAR; j++) {
      printf("=");
    }

    printf("|");
  }

  printf("\n|");
  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    printf(" ");
    for (j = 0; j < NUM_PHILOSOPHERS; j++) {
      printf("-");
    }

    for (j = 0; j < STATUS_NUM_CHAR - NUM_PHILOSOPHERS - 1; j++) {
      printf(" ");
    }

    printf("|");
  }
}

void print_footer() {
  int i, j;

  printf("\n|");
  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    printf(" ");
    for (j = 0; j < NUM_PHILOSOPHERS; j++) {
      printf("-");
    }

    for (j = 0; j < STATUS_NUM_CHAR - NUM_PHILOSOPHERS - 1; j++) {
      printf(" ");
    }

    printf("|");
  }

  printf("\n|");
  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    for (j = 0; j < STATUS_NUM_CHAR; j++) {
      printf("=");
    }

    printf("|");
  }
  printf("\n");
}

void *cycle(void *phil) {
  philosopher *p = (philosopher *) phil;
  int grab_first, grab_second;

  if (p->position % 2) {
    grab_first = p->fork1;
    grab_second = p->fork2;
  }
  else {
    grab_first = p->fork2;
    grab_second = p->fork1;
  }

  p->cycles_left--;

  /* pick up first fork */
  pthread_mutex_lock(&fork_mutexes[grab_first]);
  pthread_mutex_lock(&status_mutex);
  p->num_forks = 1;

  /* print status */
  print_status();
  pthread_mutex_unlock(&status_mutex);

  /* pick up second fork */
  pthread_mutex_lock(&fork_mutexes[grab_second]);
  pthread_mutex_lock(&status_mutex);
  p->num_forks = 2;

  /* print status */
  print_status();
  pthread_mutex_unlock(&status_mutex);

  /* change philosopher's status to eating */
  pthread_mutex_lock(&status_mutex);
  p->status = EATING;

  /* print status */
  print_status();
  pthread_mutex_unlock(&status_mutex);

  /* wait some time for philosopher to eat */
  dawdle();

  /* stop eating */
  /* change philosopher's status to changing */
  pthread_mutex_lock(&status_mutex);
  p->status = CHANGING;

  /* print status */
  print_status();
  pthread_mutex_unlock(&status_mutex);

  /* release first fork */
  pthread_mutex_lock(&status_mutex);
  p->num_forks = 1;
  pthread_mutex_unlock(&fork_mutexes[grab_first]);


  /* print status */
  print_status();
  pthread_mutex_unlock(&status_mutex);

  /* release second fork */
  pthread_mutex_lock(&status_mutex);
  p->num_forks = 0;
  pthread_mutex_unlock(&fork_mutexes[grab_second]);


  /* print status */
  print_status();
  pthread_mutex_unlock(&status_mutex);

  /* start thinking */
  /* change philosopher's status to thinking */
  pthread_mutex_lock(&status_mutex);
  p->status = THINKING;

  /* print status */
  print_status();
  pthread_mutex_unlock(&status_mutex);

  /* change status to changing */
  pthread_mutex_lock(&status_mutex);
  p->status = CHANGING;

  /* print status */
  print_status();
  pthread_mutex_unlock(&status_mutex);


  if (p->cycles_left > 0) {
    cycle(phil);
  }

  return (void *) &exit_ok;
}

int main(int argc, char *argv[]) {
  pthread_t threads[NUM_PHILOSOPHERS];
  pthread_attr_t attr;
  int numCycles = 1;
  int i, status;
  void *ex_status;

  if (argc > 1) {
    numCycles = atoi(argv[1]);
  }

  if (numCycles < 1) {
    exit(-1);
  }

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  pthread_mutex_init(&status_mutex, NULL);
  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    pthread_mutex_init(&fork_mutexes[i], NULL);
  }

  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    philosophers[i].name = FIRST_PHIL + i;
    philosophers[i].position = i;
    philosophers[i].num_forks = 0;
    philosophers[i].fork1 = i;

    if (i != NUM_PHILOSOPHERS - 1) {
      philosophers[i].fork2 = i + 1;
    }
    else {
      philosophers[i].fork2 = 0;
    }

    philosophers[i].status = CHANGING;
    philosophers[i].cycles_left = numCycles;
  }

  print_header();

  /* spawn NUM_PHILOSOPHERS threads and have them call eat() */
  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    status = pthread_create(&threads[i], &attr, cycle, (void *)
     &philosophers[i]);
    if (status) {
      fprintf(stderr, "ERROR; return code from pthread_create() is %d\n",
       status);
      exit(-1);
    }
  }

  /* wait for threads to finish */
  for (i = 0; i < NUM_PHILOSOPHERS; i++) {
    status = pthread_join(threads[i], &ex_status);
    if (status) {
      fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", status);
      exit(-1);
    }
    if (*((long *)ex_status)) {
      fprintf(stderr, "ERROR; return code from pthread function is %ld\n",
       *((long *)ex_status));
    }
  }

  print_footer();
  pthread_exit(NULL);

  return 0;
}
