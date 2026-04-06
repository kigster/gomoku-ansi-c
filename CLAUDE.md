# Gomoku Project

Welcome to Gomoku Project.

Gomoku, also called "five in a row", is an abstract strategy board game. It is traditionally played with Go pieces (black and white stones) on a 15×15 Go board while in the past a 19×19 board was standard. Because pieces are typically not moved or removed from the board, gomoku may also be played as a paper-and-pencil game. The game is known in several countries under different names, like "crosses and naughts", etc.

## Gomoku Components

Currently, building the game with `make clean all` results in:

* a single binary `gomoku` that plays with the AI by the default, but accepts many CLI flags to adjust the difficulty.

* a single binary `gomoku-httpd` which listens on a HTTP port to POST to /gomoku/play and expect to receive a JSON response, schema for which is in the `doc` folder.

* A single binary `gomoku-http-test` (with it's own CLI) that connects to the port of `gomoku-httpd` (or several) and plays a game where the state is on the client's side, but the servers receive JSON representing a game state, they figure out how's next, and find the best move, returning the JSON with one additional move unless there is a win.

* We now have a web front-end that can talk to the `gomoku-httpd` daemon and make it play against itself.

* We also now have the cluster version that works locally in development (on a MacBook):

  * in development, first run `bin/gctl setup` to get everything installed and setup. The bash setup function is an aggregation function which calls four more specific setups.
  * in the development we should be starting the game cluster with `bin/gctl start` (starts envoy reverse proxy, nginx, gomoku-httpd).
  * or `bin/gctl start [ -p haproxy ]` to use haproxy instead
  * stopped with `gctl stop`
  * restarted with `gctl restart`
  * monitored with `gctl observe [ htop | btop | ctop | btm ]`
  * monitored with `gctl ps` — prints all the processes related to the cluster using a custom format ps sequence: PID, PPID, %CPU, %MEM, ARGS

### Current Deploy

The primary deploy command is `just cr-update`, which builds Docker containers and deploys them to Google Cloud Run. For first-time setup use `just cr-init`.

Manual steps if needed:

```bash
gcloud auth login
cd iac/cloud_run
./update.sh
```
