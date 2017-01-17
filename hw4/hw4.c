#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // qsort_r
#endif  // _GNU_SOURCE

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define THREADS 4

#define MAX_TRAIN_DATA 25600
#define MAX_TEST_DATA 25600
#define MAX_DTREE_DEPTH 100
#define FOREST_SIZE 4
#define DATA_DIMENSON 33
#define PRINTED_DIMENSON 8

#define BAGGING_DATASET_TIME(setsize)  ((setsize) / 12)
#define BAGGING_FEATURES_TIME(dimen)  ((dimen) / 3)  // not used for now

// for simplicity, the leaves (having determined state) are stored as
// node with negative dimen.
// -1: verdict=0; -2: verdict=1
#define DETERMINED_STATE(verdict)  ((verdict) == 0 ? -1 : -2)
// less or equal to half; used in branching to determine major group
#define LEQ_HALF(a,b)  ((a) * 2 >= (b))

// some default location of data
const char* def_trainDataFname = "../data/training_data";
const char* def_testDataFname  = "../data/testing_data";
const char* def_outputFname    = "./submission.csv";

typedef struct _dtree {
  int dimen;
  double thershold;
  struct _dtree* left;
  struct _dtree* right;
} dtree;

typedef struct {
  dtree* tree;
  int score;
} dtreepair;

typedef struct {
  int id;
  double data[DATA_DIMENSON];
  char result;
} record;

typedef struct {
  int id;
  int n;
  int pickSize;
  int amount;
  record** dataset;
  record** pickedData;
  dtree** treeRootPtr;
} trainInstr;

pthread_mutex_t treeNodeMutex;

int rnd(int range) {
  return (int)(range * ((double)rand() / RAND_MAX));
}

dtree* getDTreeNode(int dimen) {
  // to avoid race condition, use it...
  // dtree* ret = (dtree*) malloc(sizeof(dtree));
  // ret->dimen = dimen;
  // return ret;

  static int counter;
  static dtree* allocated;
  // allocate 1M memory at a time
  static const int allocSize = 1048576 / sizeof(dtree);

  // ... or rely on thread-safe malloc in glibc
  // plus mutex to manage memory on one's own
  if (counter == 0) {
#ifdef VERBOSE
    fprintf(stderr, "INFO: allocated %d dtree nodes\n", allocSize);
#endif  // VERBOSE
    allocated = (dtree*) malloc(allocSize * sizeof(dtree));
    if (allocated == 0) {
      perror("calloc");
      abort();
    }
  }
  assert(counter < allocSize);
  pthread_mutex_lock(&treeNodeMutex);
  dtree* ret = &allocated[counter++];
  pthread_mutex_unlock(&treeNodeMutex);
  ret->dimen = dimen;
  if (counter == allocSize) {
    counter = 0;
    allocated = NULL;
  }
  return ret;
}

void printRow(FILE* outp, record* row) {
  for (int i = 0; i < PRINTED_DIMENSON; i++)
    fprintf(outp, "%2.0lf ", row->data[i]);
  fprintf(outp, "// %d\n", row->result);
}

void printDTree(FILE* outp, dtree* t) {
  static int dep;
  static int lr;

  for (int i = 0; i < dep * 2; i++) putc(' ', outp);
  if (!t) return;

  char ind = (lr == 0 ? 'x' : (lr == 1 ? '<' : '>'));

  // leaf
  if (t->dimen < 0) {
    fprintf(outp, "%c  [DECIDE %d]\n",
      ind, t->dimen == -1 ? 0 : 1);
    return;
  }

  fprintf(outp, "%c  dimen %d ther %.2lf\n",
    ind, t->dimen, t->thershold);

  dep++;
  lr = 1, printDTree(outp, t->left);
  lr = 2, printDTree(outp, t->right);
  dep--;
}

FILE* readFile(const char* filename) {
  FILE* fp = fopen(filename, "r");
  if (!fp) {
    perror("fopen");
    abort();
  }
  return fp;
}

int countZeros(record* rows[], int n) {
  int cnt = 0;
  for (int i = 0; i < n; i++)
    cnt += (rows[i]->result == 0);
  return cnt;
}

int optimalHalf(record* rows[], int n, int dimen, double* impurity) {
  assert(n > 1);

  int zeroTotal = countZeros(rows, n);

  int optiSplit = -1;
  double optiImpurity = 1.f;  // impurity should be <= 1/2

  double prev = rows[0]->data[dimen];
  int zeroCount = 0;

  // split to two half, [0, m) and [m, n)
  for (int m = 1; m < n; m++) {
    zeroCount += (rows[m - 1]->result == 0);

    // do not split at the half of the very same value
    if (rows[m]->data[dimen] == prev)
      continue;
    prev = rows[m]->data[dimen];

    // split at better position
    double splitMul = 1.f * m * (n - m) / (n * n);
    if (splitMul < 0.05f) continue;

    int mComp = n - m;
    int zeroComp = zeroTotal - zeroCount;

    double curImpurity =
      ((double)zeroCount) * (m     - zeroCount) / (m * m) +
      ((double)zeroComp)  * (mComp - zeroComp)  / (mComp * mComp);

#ifdef EVERY_DETAILS
    fprintf(stderr, "Split: %d/%d, 0: %d/%d, 1: %d/%d, impur=%.12lf\n",
      m, mComp,
      zeroCount, (int)zeroComp,
      (int)(m - zeroCount), (int)(mComp - zeroComp),
      curImpurity);
#endif  // EVERY_DETAILS

    if (curImpurity < optiImpurity) {
      optiSplit = m;
      optiImpurity = curImpurity;
    }
  }

  // it is possible that there is no way to split
  // negative value should be returned in this case

#ifdef EVERY_DETAILS
  if (optiSplit > 0) {
    fprintf(stderr, "Optimal split = %5d/%5d, impur=%.12lf\n",
      optiSplit, n - optiSplit, optiImpurity);
  } else {
    fprintf(stderr, "No way to split\n");
  }
#endif  // EVERY_DETAILS

  if (optiSplit > 0 && impurity != NULL)
    *impurity = optiImpurity;
  return optiSplit;
}

int cmpDimen(const void* _a, const void* _b, void* _x) {
  double* a = (*(record**)_a)->data;
  double* b = (*(record**)_b)->data;
  int dimen = *(int*)_x;
  return a[dimen] - b[dimen];
}

int cmpDT(const void* a, const void* b) {
  return ((dtreepair*) b)->score - ((dtreepair*) a)->score;
}

void sortByDimen(record* rows[], int n, int dimen) {
  if (dimen < 0 || dimen >= DATA_DIMENSON)
    fprintf(stderr, "%d\n", dimen);
  assert(dimen >= 0 && dimen < DATA_DIMENSON);
  qsort_r(rows, n, sizeof(record*), cmpDimen, &dimen);
}

dtree* dicisionTree(record* rows[], int n, int usedFeatures[], int depth) {
  double optiImpur = 1.f;

  dtree* node = getDTreeNode(0);

  // if the result set is too small, stop branching
  if (n <= 1) {
    int verd = LEQ_HALF(countZeros(rows, n), n) ? 0 : 1;
    node->dimen = DETERMINED_STATE(verd);
    return node;
  }

  int optiSplit = -1;

  for (int i = 0; i < DATA_DIMENSON; i++) {
  // for (int i = 0; i < BAGGING_DATASET_TIME(DATA_DIMENSON); i++) {
    int dimen = i;
    if (!usedFeatures[dimen]) continue;

    double impur;
    sortByDimen(rows, n, dimen);
    int split = optimalHalf(rows, n, dimen, &impur);
    if (split < 0) continue;
    if (impur < optiImpur) {
      optiImpur = impur;
      optiSplit = split;
      node->dimen = dimen;
      node->thershold = (rows[split - 1]->data[dimen] + rows[split]->data[dimen]) / 2;
    }
  }

  // cannot branch anymore; data is all the same on selected dimensions
  // (btw, the training data is not consistent. 4 records with the same features has 3 0's and 1 1's.
  // why can this happen?)
  if (optiSplit < 0) {
    // we count the majority of the result and determine a result
    int zeroVote = countZeros(rows, n);
    int verd = LEQ_HALF(zeroVote, n) ? 0 : 1;
    if (zeroVote != 0 && zeroVote != n) {
      double zeroRate = 1.f * zeroVote / n;
      double cert = verd == 0 ? zeroRate : (1 - zeroRate);
      fprintf(stderr, "Aggregate impurely at depth %d: verd=%d (size: %d, certainity: %.1lf%%)\n",
        depth, verd, n, 100.f * cert);
#ifdef VERBOSE
      for (int i = 0; i < n; i++) {
        fprintf(stderr, "[%5d]", rows[i]->id);
        printRow(stderr, rows[i]);
      }
#endif  // VERBOSE
    }
    node->dimen = DETERMINED_STATE(verd);
    // node->dimen = DETERMINED_STATE(1);
    return node;
  }

  // fprintf(stderr, "n=%d: branch dimension %d, thershold %.2lf"
  //   " (position %d, impurity %.12lf)\n",
  //   n, node->dimen, node->thershold, optiSplit, optiImpur);

  // okay to determine the result!
  if (optiImpur < 1e-10f || depth >= MAX_DTREE_DEPTH - 1) {
    // -1: verdict=0; -2: verdict=1
    int zeroVoteL = countZeros(rows, optiSplit);
    int zeroVoteR = countZeros(rows + optiSplit, n - optiSplit);
    int verdL = LEQ_HALF(zeroVoteL, optiSplit) ? 0 : 1;
    double zeroRateL = 1.f * zeroVoteL / optiSplit;
    double certL = verdL == 0 ? zeroRateL : (1 - zeroRateL);
    double certR = 1.f * zeroVoteR / (n - optiSplit);
    if (depth >= MAX_DTREE_DEPTH - 1) {
      fprintf(stderr, "Prune at depth %d: left=%d (size: %d/%d, certainity: %.1lf%%/%.1lf%%)\n",
        depth, verdL, optiSplit, n - optiSplit, 100.f * certL, 100.f * certR);
    }
    // assert(certainity > .4999f);
    node->left = getDTreeNode(DETERMINED_STATE(verdL));
    node->right = getDTreeNode(DETERMINED_STATE(!verdL));
    return node;
  }

  sortByDimen(rows, n, node->dimen);
  // fprintf(stderr, "Left  node (dep=%d): ", depth);
  node->left = dicisionTree(rows, optiSplit, usedFeatures, depth + 1);
  // fprintf(stderr, "Right node (dep=%d): ", depth);
  node->right = dicisionTree(rows + optiSplit, n - optiSplit, usedFeatures, depth + 1);
  return node;
}

char decide(dtree* tree, record* rec) {
  if (tree->dimen < 0) return tree->dimen == -1 ? 0 : 1;
  if (rec->data[tree->dimen] < tree->thershold) {
    return decide(tree->left, rec);
  } else {
    return decide(tree->right, rec);
  }
}

void outputVerdict(record* rows[], int n, FILE* outp) {
  // table header
  fprintf(outp, "id,label\n");
  for (int i = 0; i < n; i++) {
    fprintf(outp, "%d,%d\n", i, rows[i]->result);
  }
}

void* train(void* _instr) {
  trainInstr* instr = (trainInstr*) _instr;
  record** pickedData = (record**) malloc(instr->pickSize * sizeof(record*));

  for (int i = 0; i < instr->amount; i++) {
    fprintf(stderr, "#%d: Making decision tree (%d/%d)\n", instr->id, i + 1, instr->amount);
    for (int p = 0; p < instr->pickSize; p++) {
      pickedData[p] = instr->dataset[rnd(instr->n)];
    }

    int usedFeatures[DATA_DIMENSON] = {
#if DATA_DIMENSON == 33
      1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,
      0,0,0
#else
    #warning "No data is initialized in usedFeatures if DATA_DIMENSON is not 33 (TODO)."
#endif
    };
    instr->treeRootPtr[i] = dicisionTree(pickedData, instr->pickSize, usedFeatures, 0);

#ifdef EVERY_DETAILS
    printDTree(stderr, instr->treeRootPtr[i]);
#endif  // EVERY_DETAILS
  }

  free(pickedData);

  return (void*) &(instr->id);
}

int main(int argc, char* argv[]) {
  char* trainDataFname = (char*) def_trainDataFname;
  char* testDataFname  = (char*) def_testDataFname;
  char* outputFname    = (char*) def_outputFname;

  if (argc >= 2) {
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
      printf("Usage: %s [options...]\n", argv[0]);
      puts("Options: (TODO)");
      puts("  -data DIR:     Data directory where train_data and test_data are read");
      puts("  -output PATH:  The path to output the result");
      puts("  -tree NUM:     Number of trees to be trained");
      puts("  -thread NUM:   Max number of threads");
      puts("");
      return 0;
    }
    puts("Invalid option. Use -h or --help to read usage.");
    return 1;
  }

  /********************** TRAINING **********************/


#ifdef VERBOSE
  fprintf(stderr, "Reading training data [%s]...\n", trainDataFname);
#endif

  FILE* ftrain = readFile(trainDataFname);

  int n = 0, tmp;

  record* dataset[MAX_TRAIN_DATA];
  while (fscanf(ftrain, "%d", &tmp) != EOF) {
    record* row = (record*) malloc(sizeof(record));
    dataset[n] = row;
    row->id = n;
    for (int i = 0; i < DATA_DIMENSON; i++) {
      fscanf(ftrain, "%lf", &row->data[i]);
    }
    fscanf(ftrain, "%d", &tmp);
    row->result = (char) tmp;
    n++;
  }

#ifdef VERBOSE
  fprintf(stderr, "Training data read. Dataset size is %d.\n", n);
#endif

  srand(time(NULL));

  int pickSize = BAGGING_DATASET_TIME(n);
  dtree* forest[FOREST_SIZE];

  fprintf(stderr, "Using %d threads...\n", THREADS);

  pthread_mutex_init(&treeNodeMutex, NULL);

  trainInstr thrTrain[THREADS];
  pthread_t thrTrainHdl[THREADS];

  for (int i = 0; i < THREADS; i++) {
    int amountDiv = FOREST_SIZE / THREADS;
    int amountRem = FOREST_SIZE % THREADS;
    thrTrain[i] = (trainInstr) {
      .id = i,
      .n = n,
      .dataset = dataset,
      .pickSize = pickSize,
      .amount = amountDiv + (i < amountRem),
      .treeRootPtr = &forest[FOREST_SIZE / THREADS * i + (i < amountRem ? i : amountRem)]
    };
    pthread_create(&thrTrainHdl[i], NULL, train, (void*) &thrTrain[i]);
  }

  for (int i = 0; i < THREADS; i++) {
    int* ptrThrId;
    pthread_join(thrTrainHdl[i], (void**) &ptrThrId);
    fprintf(stderr, "Thread #%d had finished training.\n", *ptrThrId);
  }

  fprintf(stderr, "Refining trees...\n");

  // feedback; throwing away some poorly-trained trees
  // to subtly improve the accuracy when the test data is
  // similiar to the training data
  dtreepair treeScore[FOREST_SIZE] = {};
  for (int d = 0; d < FOREST_SIZE; d++) {
    treeScore->tree = forest[d];
    for (int i = 0; i < n; i++) {
      treeScore[d].score += (decide(forest[d], dataset[i]) == dataset[i]->result);
    }
  }

  qsort(&treeScore, FOREST_SIZE, sizeof(dtreepair), cmpDT);

#ifdef VERBOSE
  for (int d = 0; d < FOREST_SIZE; d++) {
    if (d % 5 == 0 && d != 0)
      fprintf(stderr, "\n");
    fprintf(stderr, "#%3d: %5d  ", d, treeScore[d].score);
  }
  fprintf(stderr, "\n");
#endif  // VERBOSE

  const int pickFSize = FOREST_SIZE * 7 / 10;

  /********************** TESTING **********************/


#ifdef VERBOSE
  fprintf(stderr, "Reading testing data [%s]...\n", testDataFname);
#endif

  FILE* ftest = readFile(testDataFname);

  int nn = 0;

  record* testdata[MAX_TEST_DATA];
  while (fscanf(ftest, "%d", &tmp) != EOF) {
    record* row = (record*) malloc(sizeof(record));
    testdata[nn] = row;
    for (int i = 0; i < DATA_DIMENSON; i++) {
      fscanf(ftest, "%lf", &row->data[i]);
    }
    fscanf(ftest, "%d", &tmp);
    nn++;
  }

#ifdef VERBOSE
  fprintf(stderr, "Testing data read. Dataset size is %d.\n", nn);
#endif

  fprintf(stderr, "Start testing... \n");

// to be percise; spread testing data instead of trees!
  for (int i = 0; i < nn; i++) {
    int zeroVote = 0;
    for (int d = 0; d < pickFSize; d++) {
      zeroVote += (decide(forest[d], testdata[i]) == 0);
    }

    // we let the result bias towards 0 :)
    int verdict = LEQ_HALF(zeroVote, pickFSize) ? 0 : 1;
    testdata[i]->result = verdict;

#ifdef VERBOSE
    fprintf(stderr, "Decide test data #%d: %d (%d/%d)\n",
      i, verdict, zeroVote, pickFSize - zeroVote);
#endif  // VERBOSE
  }

  FILE* fout = fopen(outputFname, "w");
  outputVerdict(testdata, nn, fout);

#ifdef STDOUT
  outputVerdict(testdata, nn, stdout);
#endif

}
