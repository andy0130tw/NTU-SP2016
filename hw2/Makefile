## Makefile is inspired from NTU-DSnP
CHDRS  = $(wildcard *.h)
ECHO   = /bin/echo -e
CFLAGS = -g -Wall -DCOLOR

% : %.c
	@$(ECHO) "> compiling: \033[34m$<\033[0m -> \033[01m\033[34m$@\033[0m"
	@$(CC) $(CFLAGS) -o $@ $<

all: big_judge judge player

debug: CFLAGS += -DDEBUG
debug: all

big_judge: big_judge.c $(CHDRS)
judge: judge.c $(CHDRS)
player: player.c $(CHDRS)

fullplayer: player
	cp player player_1
	cp player player_2
	cp player player_3
	cp player player_4

moreplayer: fullplayer
	cp player player_5
	cp player player_6
	cp player player_7
	cp player player_8
	cp player player_9

clean: fifoclean
	@rm -f big_judge judge player player_*

fifoclean:
	@rm -f *.FIFO
