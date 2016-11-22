#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/select.h>
#include<sys/wait.h>
#include<fcntl.h>
#include "color.h"
#include "constants.h"

static const char* player_name_list[] = {"A", "B", "C", "D"};

typedef struct _player_data {
  int id;
  pid_t pid;
  int auth_key;
  int is_active;
  int score;
  int fifo;
  int _answer;
} player_data;

void create_fifos(int judge_id) {
  char fifoname[64] = {};
  sprintf(fifoname, "./judge%d.FIFO", judge_id);
  if (mkfifo(fifoname, 0644) < 0) {
    perror("mkfifo");
    exit(1);
  }

  for (int i = 0; i < 4; i++) {
    sprintf(fifoname, "./judge%d_%s.FIFO", judge_id, player_name_list[i]);
    if (mkfifo(fifoname, 0644) < 0) {
      perror("mkfifo");
      exit(1);
    }
  }
}

void clean_fifos(int judge_id) {
  char fifoname[64] = {};
  sprintf(fifoname, "./judge%d.FIFO", judge_id);
  unlink(fifoname);

  for (int i = 0; i < 4; i++) {
    sprintf(fifoname, "./judge%d_%s.FIFO", judge_id, player_name_list[i]);
    unlink(fifoname);
  }
}

void award_players(player_data players[4]) {
  int choices[3] = {1, 3, 5};
  for (int i = 0; i < 3; i++) {
    int award_player_id = -1, cnt = 0;
    for (int j = 0; j < 4; j++) {
      if (players[j]._answer == choices[i]) {
        award_player_id = j;
        cnt++;
      }
    }
    if (award_player_id >= 0 && cnt == 1) {
      fprintf(stderr, "%d: player %c get %d points\n",
        getpid(), award_player_id + 'A', choices[i]);
      players[award_player_id].score += choices[i];
    }
  }
}

void kill_players(player_data player_data[4]) {
  int alive_child_num = 0;
  for (int i = 0; i < 4; i++)
    if (player_data[i].pid > 0)
      alive_child_num++;

  for (; alive_child_num; alive_child_num--) {
#ifdef VERBOSE
    int status;
    int pid = wait(&status);
    status = WEXITSTATUS(status);
    if (status == 0)
      fprintf(stderr, CTRLSEQ_GREEN "%d: player (pid=%d) exits normally." CTRLSEQ_RESET "\n",
        getpid(), pid);
    else
      fprintf(stderr, CTRLSEQ_RED "%d: player (pid=%d) exits with code %d." CTRLSEQ_RESET "\n",
        getpid(), pid, status);
#else
    wait(NULL);
#endif
  }
}

int player_cmp(const void* _a, const void* _b) {
  player_data* a = (player_data*) _a;
  player_data* b = (player_data*) _b;
  if (a->score != b->score) return b->score - a->score;
  return a->id - b->id;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <judge_id>\n", argv[0]);
    exit(1);
  }

  int judge_id = atoi(argv[1]);

  fprintf(stderr, CTRLSEQ_GRAY
    "=== Judge (%d); judge ID = %d ===\n"
    CTRLSEQ_RESET,
    getpid(), judge_id);

  srand(time(NULL));

  while (1) {
    player_data players[4] = {};

    for (int i = 0; i < 4; i++)
      scanf("%d", &players[i].id);

    if (players[0].id < 0) {
      fprintf(stderr, "%d: receive negative player num, exiting...\n", getpid());
      break;
    }

    create_fifos(judge_id);

    fprintf(stderr, "\033[34m" "%d: judge get player id: %d, %d, %d, %d" "\033[0m\n",
      getpid(), players[0].id, players[1].id, players[2].id, players[3].id);

    for (int i = 0; i < 4; i++) {
      // initialize every player
      char random_key_str[8];
      players[i].auth_key = rand() % 0xffff;
      sprintf(random_key_str, "%d", players[i].auth_key);

      players[i].is_active = 1;

      players[i].pid = fork();
      if (players[i].pid < 0) {
        perror("fork");
        exit(1);
      } else if (players[i].pid == 0) {
        char exec_name[24], judge_id_str[24];
        sprintf(exec_name, "./player_%d", players[i].id);
        sprintf(judge_id_str, "%d", judge_id);

        if (execlp(exec_name, exec_name,
          judge_id_str, player_name_list[i], random_key_str,
          NULL) < 0) {
          perror("execlp player");
          exit(1);
        }
      }
    }

    char fifoname[24];
    sprintf(fifoname, "./judge%d.FIFO", judge_id);
    int fifo_report = open(fifoname, O_RDONLY);

    for (int i = 0; i < 4; i++) {
      sprintf(fifoname, "./judge%d_%s.FIFO", judge_id, player_name_list[i]);
      players[i].fifo = open(fifoname, O_WRONLY);
    }

    char prev_result_buf[128];

    for (int round = 0; round < ROUNDS; round++) {
      fprintf(stderr,
        CTRLSEQ_YELLOW "%d: Round #%d start - - - - - - - - - - -\n" CTRLSEQ_RESET,
        getpid(), round + 1);

      int player_remain_count = 0;
      for (int i = 0; i < 4; i++) {
        players[i]._answer = 0;
        if (players[i].is_active) {
          player_remain_count++;
        }
        if (round > 0 && players[i].pid > 0) {
          // send to the player about the previous result
          write(players[i].fifo, prev_result_buf, strlen(prev_result_buf));
        }
      }
      fprintf(stderr, "%d: remains %d player\n", getpid(), player_remain_count);

      struct timeval wait_time = { .tv_sec = 3, .tv_usec = 0 };

      fd_set fds_report;
      FD_ZERO(&fds_report);
      while (player_remain_count > 0) {
        FD_SET(fifo_report, &fds_report);
        int okfds = select(fifo_report + 1, &fds_report, NULL, NULL, &wait_time);
        if (okfds == 0) break;
        if (okfds < 0) perror("select");

        char buf[1024];
        int read_bytes = read(fifo_report, buf, 1023);
        buf[read_bytes] = '\0';

        char idx[8];
        int rand_key, choice;

        int offs = 0;
        int chars;
        while (sscanf(buf + offs, "%s%d%d%n", idx, &rand_key, &choice, &chars) == 3) {
          fprintf(stderr, "%d: JUDGE recv %c %d %d\n", getpid(), idx[0], rand_key, choice);
          int player_id = idx[0] - 'A';
          // check random key & is legible to respond
          if (!players[player_id].is_active) {
            fprintf(stderr, "%d: JUDGE ignored previous... (no longer active)\n", getpid());
          } else if (players[player_id].auth_key != rand_key) {
            fprintf(stderr, "%d: JUDGE ignored previous... (wrong auth key)\n", getpid());
          } else {
            players[player_id]._answer = choice;
            player_remain_count--;
          }
          offs += chars;
        }
      }

      // timeout; give unanswered players panelty by removing its fd from list
      for (int i = 0; i < 4; i++) {
        if (players[i].is_active && players[i]._answer == 0) {
          fprintf(stderr, "%d: player %c timeout, killing...\n", getpid(), i + 'A');
          players[i].is_active = 0;
          kill(players[i].pid, 15);
          players[i].pid = 0;
          close(players[i].fifo);
          players[i].fifo = 0;
        }
      }

      award_players(players);

      sprintf(prev_result_buf, "%d %d %d %d",
        players[0]._answer, players[1]._answer, players[2]._answer, players[3]._answer);

      fprintf(stderr, "%d: Round #%d end  " CTRLSEQ_LBLUE "result [%s]  score [%d %d %d %d]" CTRLSEQ_RESET "\n",
        getpid(), round + 1, prev_result_buf,
        players[0].score, players[1].score, players[2].score, players[3].score);

      // add a newline char AFTER printing to stderr
      strcat(prev_result_buf, "\n");
    }

    close(fifo_report);
    for (int i = 0; i < 4; i++) {
      if (players[i].fifo > 0)
        close(players[i].fifo);
    }
    // close all FIFOs
    clean_fifos(judge_id);

    // clear up all players
    kill_players(players);

    // send data back
    qsort(players, 4, sizeof(player_data), player_cmp);
    char buf[1024];
    int chars, offs = 0;
    for (int i = 0; i < 4; i++) {
      sprintf(buf + offs, "%d %d\n%n", players[i].id, players[i].score, &chars);
      offs += chars;
    }
    write(STDOUT_FILENO, buf, strlen(buf));
  }
}
