## Makefile is inspired from NTU-DSnP
CHDRS  = $(wildcard *.h)
ECHO   = /bin/echo -e
CFLAGS = -DNDEBUG -pthread

% : %.c
	@$(ECHO) "> compiling: \033[34m$<\033[0m -> \033[01m\033[34m$@\033[0m"
	@$(CC) $(CFLAGS) -o $@ $<

all: hw4

# the homework forbids optimization flags
opt: CFLAGS += -O2
opt: all

run: hw4
	@echo "########################################################################"
	@echo "########################################################################"
	@echo "##### NOTE: Change number of threads at the THREADS macro in hw4.c #####"
	@echo "########################################################################"
	@echo "########################################################################"
	@echo ""
	@./hw4

perf: hw4
	@perf stat -e instructions:u -v ./hw4

debug: CFLAGS += -DVERBOSE
debug: all

clean:
	rm hw4
