#include<stdio.h>
#define BUFSIZE 1024

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s filename\n", argv[0]);
    return -1;
  }

  FILE* fp = fopen(argv[1], "r");
  if (!fp) {
    perror("fopen");
    return 1;
  }

  char buf[BUFSIZE];
  size_t nread;
  while ((nread = fread(buf, 1, BUFSIZE, fp)) > 0) {
    fwrite(buf, 1, nread, stdout);
  }
  if (ferror(fp)) {
    perror("fread");
    return 2;
  }
  fclose(fp);
}
