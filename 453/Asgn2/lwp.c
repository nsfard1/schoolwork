/* Lightweight Process Support */

#include <stdio.h>
#include <stdlib.h>
#include "lwp.h"
#include "fp.h"

static thread lib_head;
static thread scheduler_head;
static thread current_thread;
static rfile *saved_context;
static unsigned long counter = 0;

void lwp_exit2();

/* Round Robin scheduler functions */
void rr_init() {
  scheduler_head = NULL;
}

void rr_admit(thread new) {
  thread temp = scheduler_head;

  if (temp) {
    while (temp->sched_two) {
      temp = temp->sched_two;
    }

    temp->sched_two = new;
    new->sched_one = temp;
    new->sched_two = NULL;
  }
  else {
    scheduler_head = new;
    scheduler_head->sched_one = NULL;
    scheduler_head->sched_two = NULL;
  }
}

void rr_remove(thread victim) {
  if (victim == scheduler_head && !victim->sched_two) {
    scheduler_head = NULL;
    return;
  }

  thread temp = scheduler_head;

  while (temp) {
    if (temp == victim) {
      if (temp->sched_one) {
        temp->sched_one->sched_two = temp->sched_two;
      }
      if (temp->sched_two) {
        temp->sched_two->sched_one = temp->sched_one;
      }
      return;
    }
    temp = temp->sched_two;
  }
}

thread rr_next() {
  thread temp = scheduler_head;
  thread next = scheduler_head;

  if (temp && temp->sched_two) {
    while (temp->sched_two) {
      temp = temp->sched_two;
    }

    scheduler_head = next->sched_two;
    scheduler_head->sched_one = NULL;
    temp->sched_two = next;
    next->sched_one = temp;
    next->sched_two = NULL;
  }

  return next;
}

static struct scheduler round_robin = {rr_init, NULL, rr_admit, rr_remove,
 rr_next};
static scheduler default_scheduler = &round_robin;
static scheduler current_scheduler;

tid_t lwp_create(lwpfun func, void *argument, size_t stack_size) {
  /* 0. If first thread, init */
  if (!current_scheduler) {
    lwp_set_scheduler(NULL);
  }

  /* 1. create stack frame */
  unsigned long *stack_frame = malloc(stack_size * sizeof(unsigned long));
  if (!stack_frame) {
    return -1;
  }

  /* 2. fill in stack frame */
  unsigned long *stack_pointer = stack_frame + stack_size - 1;
  *stack_pointer-- = (unsigned long) lwp_exit;
  *stack_pointer-- = (unsigned long) func;
  *stack_pointer = 1;

  /* 3. create context (including floating point state init) */
  rfile thread_context = {0};
  thread_context.rdi = (unsigned long) argument;
  thread_context.rbp = (unsigned long) stack_pointer;
  thread_context.fxsave = FPU_INIT;

  /* 4. fill thread struct fields */
  thread lwp = malloc(sizeof(context));
  if (!lwp) {
    return -1;
  }

  lwp->tid = ++counter;
  lwp->stack = stack_frame;
  lwp->stacksize = stack_size;
  lwp->state = thread_context;
  lwp->lib_two = NULL;

  /* 5. add to lib_head list */
  thread temp = lib_head;
  if (temp) {
    while (temp->lib_two) {
      temp = temp->lib_two;
    }
    temp->lib_two = lwp;
    lwp->lib_one = temp;
  }
  else {
    lib_head = lwp;
    lwp->lib_one = NULL;
  }

  /* 6. admit to scheduler */
  current_scheduler->admit(lwp);

  return lwp->tid;
}

void  lwp_exit(void) {
  /* remove current thread from library linked list and scheduler */
  if (current_thread == lib_head && !current_thread->lib_two) {
    lib_head = NULL;
  }
  else {
    thread temp = lib_head;
    while (temp) {
      if (temp == current_thread) {
        if (temp->lib_one) {
          temp->lib_one->lib_two = temp->lib_two;
        }
        if (temp->lib_two) {
          temp->lib_two->lib_one = temp->lib_one;
        }
        break;
      }

      temp = temp->lib_two;
    }
  }

  current_scheduler->remove(current_thread);

  /* "Get to safe stack" */
  SetSP(saved_context->rsp);

  /* build a new stack frame */
  lwp_exit2();
}

void lwp_exit2() {
  /* free previously used data structures */
  if (current_thread->stack) {
    free(current_thread->stack);
  }
  if (current_thread) {
    free(current_thread);
  }

  /* get next thread from scheduler and run it if it isn't NULL, otherwise
   stop */
  current_thread = current_scheduler->next();
  if (current_thread) {
    load_context(&current_thread->state);
  }
  else {
    load_context(saved_context);
  }
}

tid_t lwp_gettid(void) {
  return current_thread ? current_thread->tid : NO_THREAD;
}

void  lwp_yield(void) {
  /* save current thread's context */
  thread next = current_scheduler->next();
  thread temp = current_thread;

  /* get next thread from scheduler and run it if it isn't NULL, otherwise
   stop */
  if (next) {
    current_thread = next;
    swap_rfiles(&temp->state, &current_thread->state);
  }
  else {
    load_context(saved_context);
  }
}

void  lwp_start(void) {
   saved_context = calloc(sizeof(rfile), 1);

   if (current_scheduler) {
    current_thread = current_scheduler->next();
    if (current_thread) {
      swap_rfiles(saved_context, &current_thread->state);
    }
  }
}

void  lwp_stop(void) {
  if (current_thread) {
    swap_rfiles(&current_thread->state, saved_context);
  }
  else {
    load_context(saved_context);
  }
}

void  lwp_set_scheduler(scheduler fun) {
  thread temp;

  /* set current scheduler to given scheduler, if given scheduler is NULL,
   set current scheduler to default scheduler */
  if (fun) {
    if (fun->init) {
      fun->init();
    }

    if (current_scheduler) {
      while ((temp = current_scheduler->next())) {
        current_scheduler->remove(temp);
        fun->admit(temp);
      }

      if (current_scheduler->shutdown) {
        current_scheduler->shutdown();
      }
    }

    current_scheduler = fun;
  }
  else {
    if (default_scheduler->init) {
      default_scheduler->init();
    }

    if (current_scheduler) {
      while ((temp = current_scheduler->next())) {
        current_scheduler->remove(temp);
        default_scheduler->admit(temp);
      }

      if (current_scheduler->shutdown) {
        current_scheduler->shutdown();
      }
    }

    current_scheduler = default_scheduler;
  }
}

scheduler lwp_get_scheduler(void) {
  return current_scheduler;
}

thread tid2thread(tid_t tid) {
  thread temp = lib_head;

  while (temp) {
    if (temp->tid == tid) {
      return temp;
    }
    temp = temp->lib_two;
  }

  return NULL;
}
