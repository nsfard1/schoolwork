#define _XOPEN_SOURCE

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <shadow.h>
#include <fcntl.h>
#include <termios.h>

#define MY_UID 2651338
#define BUFFER 1000
#define PROMPT "Password: "
#define PROMPT_SIZE 11
#define ROOT_UID 0
#define PROJ_GID 95
#define NEW_PERMISSIONS (04550)
#define SECONDS_PER_MIN 60

int main() {
  char *pw;
  struct stat file_stat;
  int fd;
  struct termios old, new;
  time_t current_time;
  struct passwd *password;
  struct spwd *shadow_pw;
  char *encrypted, *correct;

  /* check if I'm the one running the program by comparing my UID with the real
   * UID of this process */
  if (getuid() != MY_UID) {
    perror("Error: incorrect UID, you are not authorized to run.\n");
    exit(-1);
  }

  /* get the password stored in the password file */
  password = getpwuid(MY_UID);

  if (!password) {
    perror("Error: user doesn't exist.\n");
    exit(-1);
  }

  shadow_pw = getspnam(password->pw_name);

  if (!shadow_pw) {
    perror("Error: shadow password doesn't exist.\n");
    exit(-1);
  }

  correct = shadow_pw->sp_pwdp;

  if (!correct) {
    perror("Error: shadow password cannot be accessed.\n");
    exit(-1);
  }

  /* stop echoing */
  if (tcgetattr(stdin, &old) != 0) {
    perror("Error: unable to get termios attributes.\n");
    exit(-1);
  }

  new = old;
  new.c_lflag &= ~ECHO;

  if (tcsetattr(stdin, TCSAFLUSH, &new) != 0) {
    perror("Error: unable to set new termios attributes.\n");
    exit(-1);
  }

  /* prompt the user for a password */
  if (write(stdin, PROMPT, PROMPT_SIZE) == -1) {
    perror("Error: writing prompt failed.\n");
    exit(-1);
  }

  /* read the password */
  if (read(stdin, pw, BUFFER) == -1) {
    perror("Error: reading password failed.\n");
    exit(-1);
  }

  /* restore terminal echoing */
  if (tcsetattr(stdin, TCSAFLUSH, &old) != 0) {
    perror("Error: unable to restore terminal echoing.\n");
    exit(-1);
  }

  encrypted = crypt(pw, correct);

  pw = NULL;

  if (!encrypted) {
    perror("Error: encryption failed.\n");
    exit(-1);
  }

  if (strcmp(encrypted, correct)) {
    perror("Error: incorrect password.\n");
    exit(-1);
  }

  /* get file descriptor for "sniff" */
  fd = open("./sniff", O_RDONLY);
  if (fd == -1) {
    perror("Error: unable to open file \"sniff\".\n");
    exit(-1);
  }

  /* check if current dir contains "sniff" */
  if (fstat(fd, &file_stat)) {
    perror("Error: sniff not found in current directory.\n");
    exit(-1);
  }

  /* check if sniff is an ordinary file */
  if ((file_stat.st_mode & S_IFMT) != S_IFREG) {
    perror("Error: sniff is not a regular file.\n");
    exit(-1);
  }

  /* check sniff's owner */
  if (file_stat.st_uid != MY_UID) {
    perror("Error: sniff is not owned by me.\n");
    exit(-1);
  }

  /* check sniff's permissions */
  if (!(file_stat.st_mode & S_IXUSR) || file_stat.st_mode & S_IRWXG ||
   file_stat.st_mode & S_IRWXO) {
    fprintf(stderr, "Error: sniff has incorrect permissions.\n");
    exit(-1);
  }

  /* check if sniff was modified over 1 minute ago */
  if (time(&current_time) == (time_t) -1) {
    perror("Error: unable to get current time.\n");
    exit(-1);
  }

  if (difftime(current_time, file_stat.st_mtime) > SECONDS_PER_MIN) {
    perror("Error: sniff was modified over one minute ago.\n");
    exit(-1);
  }

  /* change ownership of sniff */
  if (fchown(fd, ROOT_UID, PROJ_GID)) {
    perror("Error: unable to change ownership of sniff.\n");
    exit(-1);
  }

  /* change permissions of sniff */
  if (fchmod(fd, NEW_PERMISSIONS)) {
    perror("Error: unable to change permissions of sniff.\n");
    exit(-1);
  }

  return 0;
}
