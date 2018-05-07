#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  int fd_child1_child2[2]/*, fd_child2_outfile[2]*/;
  int child_pid1, child_pid2, status;
  int outfile = open("outfile", O_CREAT | O_TRUNC | O_WRONLY, 0666);

  if (outfile == -1) {
    perror("open");
    exit(1);
  }

  pipe(fd_child1_child2);
  //pipe(fd_child2_outfile);

  /* Fork and check if fork failed */
  if ((child_pid1 = fork()) == -1) {
    perror("fork");
    exit(1);
  }

  /* Parent process */
  if (!child_pid1) {
    /* Fork and check if fork failed */
    if ((child_pid2 = fork()) == -1) {
      perror("fork");
      exit(1);
    }

    /* Parent process */
    if (!child_pid2) {
      close(fd_child1_child2[0]);
      close(fd_child1_child2[1]);
      //close(fd_child2_outfile[0]);
      //close(fd_child2_outfile[1]);

      wait(&status);
      if (WEXITSTATUS(status)) {
        perror("first process fail");
        exit(status);
      }

      wait (&status);
      if (WEXITSTATUS(status)) {
        perror("second process fail");
        exit(status);
      }
/*
      waitpid(child_pid1, &status, 0);
*/
      /* check if status is non zero */
      /*if (WEXITSTATUS(status)) {
        perror("ls error: ");
        exit(status);
      }*/
/*
      waitpid(child_pid2, &status, 0);
*/
      /* check if status is non zero */
      /*if (WEXITSTATUS(status)) {
        perror("sort");
        exit(status);
      }*/
    }

    /* Child 2 process */
    else {
      close(fd_child1_child2[1]);
      //close(fd_child2_outfile[0]);
      dup2(fd_child1_child2[0], 0);
      dup2(outfile, 1);
      close(fd_child1_child2[0]);
      //close(fd_child2_outfile[1]);

      execlp("sort", "sort", "-r", NULL);
    }
  }

  /* Child 1 process */
  else {
    close(fd_child1_child2[0]);
    //close(fd_child2_outfile[0]);
    //close(fd_child2_outfile[1]);
    dup2(fd_child1_child2[1], 1);
    close(fd_child1_child2[1]);

    execlp("ls", "ls", NULL);
  }

  return 0;
}

