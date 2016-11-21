# 2016 System Programming Assignment 2

This is a simple game featuring subprocesses.

You can simply type `make` to compile all the required executables. But before running it, you need at least 4 players, properly named under the current directory. **Please ensure `player_[num]` exist when you are going to run `big_judge` with `[num]` players**.

For convenince, `make fullplayer` duplicates 4 players from `./player` while `make moreplayer` duplicates 9. You can use `make fifoclean` to sweep out strayed FIFOs **before each fresh run**.

Besides the spec, additional flags can be added when compiling:

  1. `COLOR`
    Add some color to the server message. Useful and make debugging delightful.

  2. `DEBUG`
    Trivially.

  3. `VERBOSE`
    Be verbose on processes' creation / exit.

  3. `PLAYER_SLEEP`
    A timeout (5sec) will be added to `player` before each round, making run out of time when waiting for the response.

The program is still in compliant with the spec if any of these flags are set.

