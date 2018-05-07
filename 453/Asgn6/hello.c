#include <stdio.h>

#define HELLO_MESSAGE "Hello, world!\n"
#define MESSAGE_SIZE 14

int main() {
  /* save the result of printf() */
  int res = printf(HELLO_MESSAGE);

  /* check if result matches the message size */
  if (res != MESSAGE_SIZE) {
    /* uh oh, something went wrong */
    perror("Error: couldn't print message.\n");
    return 1;
  }

  /* success! */
  return 0;
}
