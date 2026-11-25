# RetroLink Volley Bomb

<p align="center"><img src="docs/art/cover-square.png" alt="RetroLink Volley Bomb cover" width="480"></p>

An original 1–4 player Game Boy Advance game for the link cable (Multi-Play mode). Packaged with [RetroLink](https://retrolink.games).

The court is up to four GBA screens laid out like phones on a table:

```
        floor P2        floor P4
+-----------------+-----------------+
|  P2 (sits       |  P4 (sits       |   P2/P4 sit across the table
|  opposite P1)   |  opposite P3)   |
+-----------------+-----------------+
|  P1             |  P3             |
+-----------------+-----------------+
        floor P1        floor P3
```

In the shared world P2 and P4 stand upside down on a floor along the top edge. They sit on the other side of the table, so their consoles show their quarter rotated 180 degrees: on every console your own floor is at the bottom, your blob is upright and the HUD is along the top. D-pad left/right always means left/right on your own screen.

Each console shows only its own screen. The ball flies off one screen and onto the next at exactly the coordinate it left. A screen edge bounces the ball unless another player's screen is on the other side. The ball's gravity pulls toward the floor of the row it is in, so it flips at the middle line. Hit the ball with your blob: your movement and where it touches you set the spin and angle, and holding B smashes it.

- **The ball is a bomb.** When it touches your floor it explodes and you lose one of your **3 lives** (shown in your colour along the top).
- At 0 lives you're out: your blob disappears and your floor turns into a trampoline, so the others keep playing across your screen. **Last player standing wins.** On the game-over screen everyone presses **A** for a rematch.
- **2 players:** P1 and P2 head to head, phones top to top across the table. The right half is walled off.
- **3 players:** P1, P2 and P3 (bottom-right). The empty top-right screen is walled off.
- **4 players:** the full 2×2.
- **1 player (no cable):** practice against a CPU in the P2 slot above you.

A minimap in the top-right corner of every screen shows the whole court: each player's quarter in their colour, a dot for every player still in, and the ball. On P2/P4's consoles it is rotated with the view.

Controls: D-pad left/right to move, **A** to jump, **B** tap to fire your **rocket** (3 per match, dots under your lives): it works out where it can meet the ball and flies straight there, then falls back bouncing off the screen edges. If no meeting point is reachable at rocket speed it still fires, and you're too late. **B** held while hitting smashes, **Start** to pause (pauses every console). In practice, Select+Start quits.

## Build

Requirements: devkitPro with `gba-dev` (devkitARM), Python 3.

```bash
git clone --recursive <this repo> && cd link-volley
```

```bash
tools/build.sh
```

This regenerates the art (`tools/make_assets.py`) and writes three ROMs:

| ROM | What it is |
| --- | --- |
| `dist/retro-link-volley-bomb.gba` | The game. |
| `dist/retro-link-volley-bomb-debug.gba` | Same, plus **Select+L** nudges the ball on that console only, which forces a desync and shows the DESYNC screen within a few frames. |
| `dist/retro-link-volley-bomb-autoplay.gba` | Every console plays itself (the AI drives its own slot), the title and lobby advance by themselves, and the host starts once the player count has been stable for 3 s. Use it for linked soak tests and for recording attract footage. Pressing any button takes over from the AI. |

`make` on its own builds the plain game to `link-volley.gba` in the repo root.

Desktop tests for the simulation and the netcode (no GBA needed):

```bash
c++ -std=c++17 -O2 -o /tmp/sim_test tests/sim_test.cpp src/sim/sim.cpp && /tmp/sim_test
```

```bash
c++ -std=c++17 -O2 -o /tmp/ls_test tests/lockstep_test.cpp src/sim/sim.cpp src/net/lockstep.cpp && /tmp/ls_test
```

`lockstep_test` runs 2, 3 and 4 virtual consoles for 10 simulated minutes each over a link with random latency, checks every console computed identical state hashes on every frame, then forces a desync and checks it is detected (it is, 4 frames later).

## Run a 4-player session in mGBA

mGBA (0.10 or newer) can link up to four instances inside one app:

1. Open `dist/retro-link-volley-bomb.gba` in mGBA.
2. **File → New multiplayer window**, then in the new window **File → Load ROM…** (or **File → Recent**) and pick the same ROM. Repeat for players 3 and 4. Window titles say "Player N of 4".
3. In each window press Start (Return by default) to reach the lobby. Every window should show `P1 P2 P3 P4` lit and "YOU ARE Pn".
4. In player 1's window press **Start** to begin. Each window shows its own quarter of the court.

mGBA's default keys: arrows = D-pad, X = A, Z = B, Return = Start, Backspace = Select, A = L, S = R. Only the focused window gets keyboard input, so to watch a full match without four people use `dist/retro-link-volley-bomb-autoplay.gba`: open the windows as above and it starts and plays by itself.

For 2 or 3 players, open 2 or 3 windows instead.

## Netcode in one paragraph

Lockstep: consoles only ever exchange inputs, never game state. Each console sends one 16-bit packet per frame carrying its buttons for frame N + 2 (input delay `net::INPUT_DELAY`, tunable to 3), the frame number mod 16, and 4 bits of its state hash for frame N − 3. A console only simulates frame N when every player's input for N has arrived; otherwise it redraws the last frame and, after 30 frames, shows "WAITING…". Every received hash slice is compared with the local hash for the same frame; a mismatch freezes the game on the **DESYNC** screen (frame, local hash, every player's slice). A packet with an unexpected frame number shows **LINK ERROR**. On DESYNC, LINK ERROR, LINK LOST, or after 3 s of WAITING, Start returns to the title. See [docs/NETCODE.md](docs/NETCODE.md).

## Layout

```
src/sim/      deterministic simulation + AI (pure integer C++, no Butano, builds on desktop)
src/net/      lockstep (pure) and the LinkCable wrapper
src/render/   court view and text
src/scenes/   title, lobby, game
src/sound/    DMG sound effects
tools/        make_assets.py (all art), build.sh
tests/        desktop tests
lib/          Butano and gba-link-connection (git submodules)
docs/         NETCODE.md, CREDITS.md
```
