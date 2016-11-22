#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/time.h>
#include<sys/select.h>
#include "color.h"

#define JUDGE_TABLE_SIZE(n)  ((n) * ((n) - 1) * ((n) - 2) * ((n) - 3) / 24)
#define JUDGE_EXEC_PATH  "./judge"

typedef struct _sort_pair {
  int id;
  int score;
} sort_pair;

int** build_player_table(int** table, int total) {
  static int arr[4], depth;
  if (depth >= 4) {
    memcpy(table, arr, sizeof(int) * 4);
    return (int**) ((int*) table + 4);
  }
  int i = 1 + (depth == 0 ? 0 : arr[depth - 1]);
  for (; i <= total; i++) {
    arr[depth] = i;
    depth++;
    table = build_player_table(table, total);
    depth--;
  }
  return table;
}

void wait_judges(int alive_child_num) {
  for (; alive_child_num; alive_child_num--) {
#ifdef VERBOSE
    int status;
    int pid = wait(&status);
    status = WEXITSTATUS(status);
    if (status == 0)
      fprintf(stderr, CTRLSEQ_GREEN "judge (pid=%d) exits normally." CTRLSEQ_RESET "\n",
        pid);
    else
      fprintf(stderr, CTRLSEQ_RED "judge (pid=%d) exits with code %d." CTRLSEQ_RESET "\n",
        pid, status);
#else
    wait(NULL);
#endif
  }
}

int sort_pair_cmp(const void* _a, const void* _b) {
  sort_pair* a = (sort_pair*) _a;
  sort_pair* b = (sort_pair*) _b;
  if (a->score != b->score) return b->score - a->score;
  return a->id - b->id;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <judge_num> <player_num>\n", argv[0]);
    exit(1);
  }

  int judge_num = atoi(argv[1]);
  int player_num = atoi(argv[2]);

  int curr_judge = 0, reported_judge = 0;
  int total_judges = JUDGE_TABLE_SIZE(player_num);
  int players_arr[total_judges][4];
  build_player_table((int**) players_arr, player_num);

#ifdef DEBUG
  puts("*** DEBUG: printing perm table ***");
  for (int i = 0; i < total_judges; i++) {
    printf("%d %d %d %d\n",
        players_arr[i][0],
        players_arr[i][1],
        players_arr[i][2],
        players_arr[i][3]);
  }
#endif

  fprintf(stderr, CTRLSEQ_GRAY
    "=== Big Judge (%d) ===\n"
    "# Judge number: %d\n"
    "# player number: %d\n"
    CTRLSEQ_RESET,
    getpid(), judge_num, player_num);

  int pids[judge_num];
  int child_fd_r[judge_num];
  int child_fd_w[judge_num];
  int child_fd_is_ready[FD_SETSIZE];

  fd_set fds_read, fds_read_ref;
  fd_set fds_write, fds_write_ref;
  FD_ZERO(&fds_read_ref);
  FD_ZERO(&fds_write_ref);

  for (int i = 0; i < judge_num; i++) {
    // prepare pipes
    //   w: from child to parent
    //   r: from parent to child
    int fd_w[2], fd_r[2];
    if (pipe(fd_w) < 0) perror("pipe fd_w");
    if (pipe(fd_r) < 0) perror("pipe fd_r");

    child_fd_w[i] = fd_w[1];
    child_fd_r[i] = fd_r[0];
    FD_SET(fd_w[1], &fds_write_ref);
    FD_SET(fd_r[0], &fds_read_ref);
    child_fd_is_ready[fd_w[1]] = 1;

    pids[i] = fork();
    if (pids[i] < 0) {
      perror("fork");
      exit(1);
    } else if (pids[i] == 0) {
      dup2(fd_w[0], STDIN_FILENO);
      dup2(fd_r[1], STDOUT_FILENO);

      char judge_id_str[24];
      sprintf(judge_id_str, "%d", i + 1);
      if (execlp(JUDGE_EXEC_PATH, JUDGE_EXEC_PATH, judge_id_str, NULL) < 0) {
        perror("execlp judge");
        exit(1);
      }
    }
  }

  fprintf(stderr, CTRLSEQ_GREEN "judges are forked, children: ");
  for (int i = 0; i < judge_num; i++) {
    fprintf(stderr, "%d (fd:w/r=%d/%d)  ", pids[i], child_fd_w[i], child_fd_r[i]);
  }
  fprintf(stderr, CTRLSEQ_RESET "\n");

  sort_pair players_stat[player_num];
  for (int i = 0; i < player_num; i++) {
    players_stat[i] = (sort_pair) { .id = (i + 1), .score = 0 };
  }

  while (reported_judge < total_judges) {
    fds_read = fds_read_ref;
    fds_write = fds_write_ref;
    int max_fd = child_fd_r[judge_num - 1];
    if (select(max_fd + 1, &fds_read, &fds_write, NULL, NULL) > 0) {
      for (int i = 0; i <= max_fd; i++) {
        if (FD_ISSET(i, &fds_read)) {
          char buf[1024];
          fprintf(stderr, "get judge result from fd_w=%d...\n", i);
          int read_bytes = read(i, buf, 1023);
          buf[read_bytes] = '\0';
          // fprintf(stderr, "data = [%s]\n", buf);

          int idx = 0, chars, offs = 0;
          sort_pair scores[4] = {};
          while (sscanf(buf + offs, "%d%d%n",
            &scores[idx].id, &scores[idx].score, &chars) == 2) {
            // players_stat[id - 1].score += score;
            offs += chars;
            idx++;
          }

          int assign[4] = {};
          for (int i = 0; i < 4; i++) {
            int j = i;
            while (scores[i].score == scores[j].score && j < 4) j++;
            for (int k = i; k < j; k++)
              assign[k] = j;
          }
          for (int i = 0; i < 4; i++) {
            fprintf(stderr, "#%d: player %d, score %d -> +%d\n",
              assign[i], scores[i].id, scores[i].score, 4 - assign[i]);
            players_stat[scores[i].id - 1].score += 4 - assign[i];
          }

          // XXX: this implementation is questionable
          child_fd_is_ready[i - 1] = 1;
          reported_judge++;
          if (reported_judge == total_judges) break;
        }
        // if it is ok and there are still tasks to assigned
        if (FD_ISSET(i, &fds_write) &&
          child_fd_is_ready[i] && curr_judge < total_judges) {
          fprintf(stderr, "assigning fd_w=%d with task %d/%d...\n", i, curr_judge + 1, total_judges);
          dprintf(i, "%d %d %d %d\n",
            players_arr[curr_judge][0],
            players_arr[curr_judge][1],
            players_arr[curr_judge][2],
            players_arr[curr_judge][3]);
          child_fd_is_ready[i] = 0;
          curr_judge++;
        }
      }
    }
  }

  fprintf(stderr, "\033[32m" "all round finished; tell all children" "\033[0m\n");
  for (int i = 0; i < judge_num; i++) {
    // fprintf(stderr, "tell fd=%d\n", child_fd_w[i]);
    dprintf(child_fd_w[i], "-1 -1 -1 -1\n");
  }

  // wait active child to prevent zombie processes
  // if execlp fails, should this value be changed?
  wait_judges(judge_num);
  fprintf(stderr, "all children finished\n");

  // final result
  qsort(players_stat, player_num, sizeof(sort_pair), sort_pair_cmp);

  for (int i = 0; i < player_num; i++)
    printf("%d %d\n", players_stat[i].id, players_stat[i].score);

}
