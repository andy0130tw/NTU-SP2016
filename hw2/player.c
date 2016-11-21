#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<time.h>
#include "color.h"
#include "constants.h"

int get_previous_result(int fifo, int result[]) {
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fifo, &read_fds);
  select(fifo + 1, &read_fds, NULL, NULL, NULL);

  static char buf[512];
  int read_bytes = read(fifo, buf, 511);
  buf[read_bytes] = '\0';
  return sscanf(buf, "%d%d%d%d", &result[0], &result[1], &result[2], &result[3]);
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage: %s <judge_id> <player_index> <random_key>\n", argv[0]);
    exit(1);
  }

  // naive seed involving pid
  // since child processes are created at almost the same time
  srand(time(NULL) + (unsigned long int) (getpid() * 10137));

  int judge_id = atoi(argv[1]);
  char player_index = argv[2][0];
  int random_key = atoi(argv[3]);

  fprintf(stderr, CTRLSEQ_GRAY
    "=== Player (%d); judge ID = %d, player = %c, random key = %d ===\n"
    CTRLSEQ_RESET,
    getpid(), judge_id, player_index, random_key);

  char fifoname[24];
  sprintf(fifoname, "./judge%d.FIFO", judge_id);
  int fifo_write = open(fifoname, O_WRONLY);

  sprintf(fifoname, "./judge%d_%c.FIFO", judge_id, player_index);
  int fifo = open(fifoname, O_RDONLY);

  int result[4] = {};

  for (int round = 0; round < ROUNDS; round++) {
    char write_buf[128];

    if (round != 0) {
      get_previous_result(fifo, result);
#ifdef DEBUG_PLAYER
      fprintf(stderr, "%d: prev status: %d %d %d %d\n",
        getpid(), result[0], result[1], result[2], result[3]);
#endif
    }

#ifdef PLAYER_SLEEP
    sleep(5);
#endif

    // TODO: logic
    int num = (rand() % 3) * 2 + 1;

    sprintf(write_buf, "%c %d %d\n", player_index, random_key, num);
    write(fifo_write, write_buf, strlen(write_buf));

  }

#ifdef DEBUG_PLAYER
  fprintf(stderr, "%d: player end, exiting...\n", getpid());
#endif
}
