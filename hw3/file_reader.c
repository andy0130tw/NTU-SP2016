#include<stdio.h>
#define BUFSIZE 1024

int main() {
  char filename[1024];
  if (scanf("%s", filename) == EOF) {
    fprintf(stderr, "expecting <filename> from stdin.\n");
    return -1;
  }

  FILE* fp = fopen(filename, "r");
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
