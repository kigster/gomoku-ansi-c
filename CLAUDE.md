# Gomoku Project

Welcome to Gomoku Project — it's nearly complete. You are going need to be an expert in Kubernetes, and Google Public Cloud and deploying to such. Please ensure you have maximum knowledge on the subject.

## Play the Game

[Game Online](https://app.gomoku.games)

## Gomoku Components

Currently, building the game with `make clean all` results in:

* a single binary `gomoku` that plays with the AI by the default, but accepts many CLI flags to adjust the difficulty.

* a single binary `gomoku-httpd` which listens on a HTTP port to POST to /gomoku/play and expect to receive a JSON response, schema for which is in the `doc` folder.

* A single binary `gomoku-http-test` (with it's own CLI) that connects to the port of `gomoku-httpd` (or several) and plays a game where the state is on the client's side, but the servers receive JSON representing a game state, they figure out how's next, and find the best move, returning the JSON with one additional move unless there is a win.

* We now have a web front-end that can talk to the `gomoku-httpd` daemon and make it play against itself.

* We also now have the cluster version that works locally in development (on a MacBook):

  * in development it's started with `bin/gctl start [ -p envoy ]`
  * stopped with `gctl stop`
  * restarted with `gctl rstart`
  * monitored with `gctl htop`

* We also now how the GCP / Kubernetes configuration for deploying this to GCP.
* The script is `bin/gcp-create-cluster` and it conains two functions (and identical arguments)s: `setup` and `deploy`. We already successfully ran `setup`,  but `deploy` is giving errors.

You are free to run `bin/gcp-create-cluster setup` again to see the output.

## Tasks to Accomplish

### 1. Evalueate and Validate Kubernetes Setup

I want you to validate the k8s setup and configuration in `iac/k8s` folder, validate the script `bin/gcp-create-cluste` and fix any issues you may find.  I would prefer that envoy gateway sat in front of the gomoku-httpd daemons, becuase it's able to maintain an inbound queue, while hapoxy or direct conenction does not.

Important to note: the game will be running on the domain <https://gomoku.games>, the DNS for which I manage elsewhere. Once the game is up and running on GCP, I will point the domain to the IP/CNAME, and get the SSL certificate (unless Google does that). 

If Google insists on a new domain, I can create `app.gomoku.games` or Google cna propose something else.

##  2. Run the `setup` one  more time

You want to see what GCP responds with when the `bin/gcp-create-cluster setup` is executed. Run it once.

After than, run `bin/gcp-create-cluster deploy` and figure out what is wrong, and why it's not deploying.

In the Terminal I am currently logged into a Google Cloud account as an admin.

## Docker Containers

There should be two docker containers ready for you: 

1. docker-httpd:latest
2. docker-frontend:latest

The second receives `VITE_API_BASE` environment (at least in develpoment). For production there may be other methods.s

## Summary

This task is about deploying the backend and the front-end of the game to GCP, and ideally using envoy gateway to round robin across `gomoku-httpd` daemon containers.

# Building Front-End Application 

Claude: stop reading. Everything below hss already been checked into main.

---

Put the files for this app inside `frontend` folder only.

Let's use:

* Vite
* React
* TypeScript
* TailwindCSS

When the user opens the site, the UI loads and at the center should be a 19x19 grid. The UI should be clean, modern, and use a consistent color palette.

Upon first loading this app, a modal pops up asking the user for their name (and in brackets explain, this is just so that we can address you properly, it's not saved anywhere).

Once the user enters the name, the modal goes away, and the user can start the game by clicking a big and obvious [ Start Game ] button.

However, there should be a pretty visible panel (or a button called "settings" that opens up a settings modal). Settings panel has various controls arranged in a table.

The following are the controls:

1. AI Search Depth: [ slider from 2 to 5 ]
2. AI Search Radius: [ slider from 1 to 4 ]
3. AI Timeout: [ none by default, a drop down with 30s, 60s, 120s, 300s ]
4. Game Display: [ radio buttons: black & white stones OR crosses and naughts ]
5. Which side do you want to play: [ X or O | or Black or White ] (depends on what they chose above).

The board itself should be at least 400px x 400px, with vertical and horizontal lines creating 19 squares vertically and 19 horizontally.

If the user chose black and white stones, they should look like circular stones.

I placed the images representing the stones here: frontend/assets/images

There is a PNG file for white stone and black stone. If the user chooses stones, they must be placed on the intersection of the lines, not inside the squares.

If the user chooses X and O they must be placed inside the square. You can either generate X and O images or use blown up unicode characters. Make X black and O white.

If not difficult, it would be great if the settings panel had a dropdown for a "Theme" which would change colors of the background and other text. But this is not necessary.

The most important part is that the board looks like it's made from wood. Someething like <https://uat.www.lysol.com/content/dam/lysol-us/article-detail-pages/hard-wood.jpg>

Once the user presses the "Start", the game begins. If the user is the first player the game waits for the first move. Once the move is made, it sends the same JSON that gomoku-httpd expects to receive, and waits for the answer. Once the answer is received, it renders the last move, and waits for the player to move again.

If the player chooses white or O, the game immediately sends the JSON to the server so the server can make the first move.
