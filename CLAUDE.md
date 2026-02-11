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

* We also now how the GCP / Kubernetes configuration for deploying this to GCP.
* The script is `bin/gcp-create-cluster` and it conains two functions (and identical arguments)s: `setup` and `deploy`. We already successfully ran `setup`,  but `deploy` is giving errors.

## Tasks to Accomplish Today

### 1. First Task for Claude

Please read in detail and understand the various nuanced versions of Gomoku Rules including Tournament rules, Renju, summarize this article: https://en.wikipedia.org/wiki/Gomoku into a markdown file RULES.md.

Let's give each variation a name (such as "renju") and list each name and what the rules are in the document at the root of the repo called RULES.md.

Finally, let's add a new CLI argument `-g | --game-type [gomoku|renju|...]` and JSON member `"game_type": [ "renju" | etc ]` and then modify the game engine to support variout game rules. The default game play should be exactly as it is right now. We'll call this "simplified" or "street" version.


### 2. We need to fix and improve `bin/gctl` script.

Right now the script relies on function `pids.matching()` from the bashmatic library that's located at the same level as this project, and you are permitted read only access. The functions are in ../bashmatic/lib/pids.sh

We need to add internal functions for detecting if processes are running (but excluding the current `grep`) and fix the overridden `pkill` function. It's currently not correctly detecting what's running and what's not.

Also, nginx is the only component that must be startee via sudo.

I also recommend that we don't just start nginx and haproxy and let them use their default config file, but point each at the file in this repo: `iac/config/haproxy.cfg` and `iac/config/nginx.conf`.

Nginx Config in particular references /Users/kig/.letsencrypt folder where the SSL certificates are stored for dev.gomoku.games. But something doesn't work in that config. The config must do two things: point the static assets to 

Right now, detecting running processes is not working, killing them is not working, and sometimes starting them is not working. The script has a lot of duplication.

