# Gomoku Front-End

This task is about creating the frontend for Gomoku game, that uses the backend on port 10000 (envoy proxy) and works similar to how gomoku-http-client works, EXCEPT instead of mirroring the JSON receivedfrom the server, the user will make a move, that move will be appended to the JSON move array, and sent back to the server until either the user wins or the AI.

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

The most important part is that the board looks like it's made from wood. Someething like https://uat.www.lysol.com/content/dam/lysol-us/article-detail-pages/hard-wood.jpg

Once the user presses the "Start", the game begins. If the user is the first player the game waits for the first move. Once the move is made, it sends the same JSON that gomoku-httpd expects to receive, and waits for the answer. Once the answer is received, it renders the last move, and waits for the player to move again.

If the player chooses white or O, the game immediately sends the JSON to the server so the server can make the first move.





