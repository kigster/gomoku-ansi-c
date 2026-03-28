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

### Current Deploy 

Today the deploy is performed by building two Docker containers (one for the React frontend and one for gomoku binary), which are then deployed using Google Run to GCP. Deployment steps:

```bash
gcloud login auth
cd iac/cloud_run
./update.sh
```

### Game Rules

Please read in detail and understand the various nuanced versions of Gomoku Rules including Tournament rules, Renju, summarize this article: https://en.wikipedia.org/wiki/Gomoku into a markdown file RULES.md.

Let's give each variation a name (such as "renju") and list each name and what the rules are in the document at the root of the repo called `RULES.md`.

Finally, let's add a new CLI argument `-g | --game-type [gomoku|renju|...]` and JSON member `"game_type": [ "renju" | etc ]` and then modify the game engine to support variout game rules. The default game play should be exactly as it is right now. We'll call this "simplified" or "street" version.

Once you parse all the game rules, let's create a React Component — a pop-up with the coincise description of the game itself, various sub-types, and the first mover advantage. Let's also explain our AI settings that tune the algorithm. Add a section on the debug JSON widget, how it can be invoked and used to download the game to the disk. If not difficult, next do "Download Game" add "Upload Game to Review" button. When clicked, it opens a file dialog box and if the proper JSON game file is uploaded, the 

### 2. We need to fix and improve `bin/gctl` script.

Right now the script relies on function `pids.matching()` from the bashmatic library that's located at the same level as this project, and you are permitted read only access. The functions are in ../bashmatic/lib/pids.sh

We need to add internal functions for detecting if processes are running (but excluding the current `grep`) and fix the overridden `pkill` function. It's currently not correctly detecting what's running and what's not.

Also, nginx is the only component that must be startee via sudo.

I also recommend that we don't just start nginx and haproxy and let them use their default config file, but point each at the file in this repo: `iac/config/haproxy.cfg` and `iac/config/nginx.conf`.

Nginx Config in particular references /Users/kig/.letsencrypt folder where the SSL certificates are stored for dev.gomoku.games. But something doesn't work in that config. The config must do two things: point the static assets to the frontend/

Right now, detecting running processes is not working, killing them is not working, and sometimes starting them is not working. The script has a lot of duplication.

# Instructions to the LLM

Gomoku in the current version has become a fully networked game.There are very few games or any web sites that have a ReactJS talk to the C-backend. Therefore I would like for you go explore what additional stateful persistent technology we can employ that would be the easiest to build, and yet secure by all moderns stanards. That means at the very least "create account with Google", "Login with Google".

Once you have an account on our servder (we should offer unique usernames first come first serve), have user profildes with avatars etc. There shold be a server-wide chat bubble open in the bottom left. The user can minimize it, and reopen it later. This consists of the global chat between any live player and everyone else. For now the AI won't be chatting, but we might eventually make LLM contestant one of the game options, perhaps premium. 

So it's obvious we need a persistence mechanism to be added to Google Run, one that wouln'd tbe removed if there are zero active users at the moment, etc. Let's assume it's a relatinal table, Cloud SQL (PostgreSQL).

Then, we'd need users table obviously, maybe 1-1 profiles (for avatars and bios), and then one of the central tables called "games". 

A Game has a hex UUID, basic stats such as created_at, started_at, finished_at, errored_at.sd

This table should have an AASM state machine:



ubclassed by "ActiveGame", "FinishedGame", "TimeoutGame", and "AbandonedGame", kand nd permanent game history DB table which is going to have a reference to what/who is playing first, and who/what is the second. For this reason i suggest creating the polymorphic table "contestants" or "oppponents", where a single raw b

