#ifndef COLOR_H
#define COLOR_H

#ifdef COLOR
#define CTRLSEQ_RESET   "\x1b[0m"
#define CTRLSEQ_BOLD    "\x1b[01m"
#define CTRLSEQ_DIM     "\x1b[02m"
#define CTRLSEQ_ULINE   "\x1b[04m"
#define CTRLSEQ_RED     "\x1b[31m"
#define CTRLSEQ_GREEN   "\x1b[32m"
#define CTRLSEQ_ORANGE  "\x1b[33m"
#define CTRLSEQ_BLUE    "\x1b[34m"
#define CTRLSEQ_PURPLE  "\x1b[35m"
#define CTRLSEQ_CYAN    "\x1b[36m"
#define CTRLSEQ_GRAY    "\x1b[90m"
#define CTRLSEQ_LRED    "\x1b[91m"
#define CTRLSEQ_LGREEN  "\x1b[92m"
#define CTRLSEQ_YELLOW  "\x1b[93m"
#define CTRLSEQ_LBLUE   "\x1b[94m"
#define CTRLSEQ_PINK    "\x1b[95m"
#define CTRLSEQ_LCYAN   "\x1b[96m"
#else
#define CTRLSEQ_RESET   ""
#define CTRLSEQ_BOLD    ""
#define CTRLSEQ_DIM     ""
#define CTRLSEQ_ULINE   ""
#define CTRLSEQ_RED     ""
#define CTRLSEQ_GREEN   ""
#define CTRLSEQ_ORANGE  ""
#define CTRLSEQ_BLUE    ""
#define CTRLSEQ_PURPLE  ""
#define CTRLSEQ_CYAN    ""
#define CTRLSEQ_GRAY    ""
#define CTRLSEQ_LRED    ""
#define CTRLSEQ_LGREEN  ""
#define CTRLSEQ_YELLOW  ""
#define CTRLSEQ_LBLUE   ""
#define CTRLSEQ_PINK    ""
#define CTRLSEQ_LCYAN   ""
#endif  // COLOR

#endif  // COLOR_H
