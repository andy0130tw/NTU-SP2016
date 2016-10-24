# 2016 System Programming Assignment 1

This is a simple read/write server that allows multiple clients to operate concurrrently. Operations should be ACCEPTed if readable (reading) or writable (writing) by testing if an appropriate lock can be put. If an appropriate lock can't be obtained, the request should be REJECTed.

Besides the spec, additional flags can be added when compiling:

  1. `COLOR`
    Add some color to the server message. Useful and make debugging delightful.

  2. `HEARTBEAT`
    A timeout (0.8sec default) will be added to `select()`, and active file descriptors are reported at the beginning of each iteration. If the flag is not turned on, the `select()` may block forever.

The program is still in compliant with the spec if any of these flags are set.

## Technical Details

  * Because the lock mechanism is work per-process instead of per-file descriptor, one can always obtain (replace) a write lock as long as the lock is previously put by the same process. Thus, we traverse the list of open file descriptors and use `fstat()` to check if a lock is hold on the same file.

