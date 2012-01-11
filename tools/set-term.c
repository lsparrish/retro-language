#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define STDIN 0

int main(void)
{
  struct termios t;
  int fd;
  char fname[1024];
  if (tcgetattr(STDIN, &t))
    return 1;
  fname[0] = 0;
  strncat(fname, getenv("HOME"), 1000);
  strcat(fname, "/.term_settings");
  fd = open(fname, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  if (fd == -1)
    return 2;
  if (write(fd, &t, sizeof(struct termios)) != sizeof(struct termios))
    return 3;
  if (close(fd))
    return 4;
  t.c_lflag &= ~(ICANON | ECHO | ISIG);
  t.c_iflag = 0;
  if (tcsetattr(STDIN, TCSANOW, &t))
    return 5;
  return 0;
}
