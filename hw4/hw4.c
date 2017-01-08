#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TRAIN_DATA 25600
#define DATA_DIMENSON 33

typedef struct _dtree {
  int dimen;
  int thershold;
  struct _dtree* left;
  struct _dtree* right;
} dtree;

double* dataset[MAX_TRAIN_DATA];
char result[MAX_TRAIN_DATA];

int main(int argc, char* argv[]) {
  if (argc >= 2) {
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
      printf("Usage: %s [options...]\n", argv[0]);
      puts("Options:");
      puts("  -data DIR:     Data directory where train_data and test_data are read");
      puts("  -output PATH:  The path to output the result");
      puts("  -tree NUM:     Number of trees to be trained");
      puts("  -thread NUM:   Max number of threads");
      return 0;
    }
    puts("Invalid option. Use -h or --help to read usage.");
    return 1;
  }

  const char* trainDataFname = "../data/training_data";

  FILE* ftrain = fopen(trainDataFname, "r");
  if (!ftrain) {
    perror("fopen");
    abort();
  }

  int n = 0, tmp;

  while (fscanf(ftrain, "%d", &tmp) != EOF) {
    double* row = (double*) malloc(DATA_DIMENSON * sizeof(double));
    dataset[n] = row;
    for (int i = 0; i < DATA_DIMENSON; i++) {
      fscanf(ftrain, "%lf", &row[i]);
    }
    fscanf(ftrain, "%d", &tmp);
    result[n] = (char) tmp;
    n++;
  }

  for (int i = 0; i < n; i++) {
    printf("[%d] ", i);
    for (int j = 0; j < DATA_DIMENSON; j++)
      printf("%.2lf ", dataset[i][j]);
    printf("// %d\n", result[i]);
  }

}
