# Gomoku & Gomoku-httpd

This repo is a C99 project that plays the game of Gomoku. The game is played on the board of 15x15 or 19x19. Instead of black and white stones, we are using X and O. X always makes a move first. 

The game is built in two versions when you run `make all`:

* `gomoku` is the terminal game that is capable of playing with itself, by starting it with `gomoku -x ai -o ai -s [ other flags ]`. The game is capable of saving the final game state into a JSON file with `-j FILE` flag. 
* `gomoku-httpd` is the http daemon that expects the same JSON as the `gomoku` saves into a file, except not the final "game over" state, but in-progress state, from the client. This executable is stateless, and once it responds with the next move, it's ready to accept another JSON request from a completely different game. 
   - `gomoku-httpd` listens on two ports: one for the game requests as HTTP, and another from haproxy agent. It responds with `drain` when it's thinking about the move, and `ready` when it's not, so that `haproxy` can route a game request to it.
   - you can start a swarm of `gomoku-httpd` servers using `bin/gomoku-ctl start` script, and stop them using `bin/gomoku-ctl stop`.
   - if you started a swarm, you can also run `bin/gomoku-ctl htop` which will show you the `htop` output filtered on `gomoku-httpd`. You will see that the cores on this machine will be very busy, with the majority of the CPU busy in the red zone (kernel space).
   - i recommend starting a single daemon, and observing the cpu via `htop -p <PID>`.
   - If you are unable to determine the cause of high kernel space CPU, I want to you to use dTrace which is available on MacOS, but you may need insert dTrace probes into the `gomoku-httpd` source code. You can find out how to add probes here: https://docs.oracle.com/cd/E19253-01/817-6223/chp-usdt-2/index.html and how to use them here: https://www.brendangregg.com/dtrace.html
   - using dtrace you can identify the bottlenecks of a running program. 
   
# Execution Mode

I hereby grant you universal permission to complete this task without human 
   