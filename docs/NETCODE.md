# Netcode

## Transport

[gba-link-connection](https://github.com/afska/gba-link-connection)'s `LinkCable` (Multi-Play mode), 38400 bps, the default 50-tick send interval (about 5 transfers per frame), timer 1, timeout 10 frames, queue size 30.

Interrupts are hooked the way the library author's Butano example ([gba-link-universal-real](https://github.com/afska/gba-link-universal-real)) does it, see `src/net/link_io.cpp`:

- `bn::core::set_vblank_callback(LINK_CABLE_ISR_VBLANK)`: Butano owns the V-Blank IRQ, so the library's V-Blank work runs from Butano's callback.
- `bn::hw::irq::set_isr` + `enable` for `SERIAL` and `TIMER1` (Butano's own timers are 2 and 3).
- `bn::memory::set_dma_enabled(false)`: DMA stalls the CPU and makes the serial IRQ late.

Butano's built-in `bn::link` (an old LinkCable that may drop messages) is not used.

## Packets

LinkCable reserves `0x0000` (no data) and `0xFFFF` (disconnected).

| Packet | Layout | Range |
| --- | --- | --- |
| Game | `1 0 ffff hhhh bbbbbb`: frame mod 16, hash slice, buttons (L, R, A, B, Start, spare) | `0x8000`–`0xBFFF` |
| GO (lobby, from P1) | `1101 00pp ssssssss`: player count − 1, seed | `0xD000`–`0xD3FF` |

Bit 15 is always 1, so neither reserved value can occur. Game and GO packets are told apart by bit 14.

## Lockstep

- Input delay `D = 2` (`net::INPUT_DELAY`). Frames 0..D−1 have empty input for everyone and no packets.
- To simulate frame N a console needs every player's input for N. When it has them it first samples its own keys and sends them for frame N + D, then steps.
- The packet for frame N + D carries 4 bits of the sender's hash of the state after frame N − 1 (that is, frame (N + D) − D − 1). When frame N + D is simulated, every player's slice is compared with the local hash for that frame.
- Each player's packets must arrive in frame order. A gap or a duplicate is reported as `SEQUENCE_ERROR` (the LINK ERROR screen) so lost messages in the app's link emulation show up right away instead of as a later desync.
- Pause (Start) is an ordinary input, so every console pauses on the same frame.
- Start of match: P1 sends GO with the player count and an 8-bit seed. Everyone, P1 included, starts the sim at frame 0 from that seed. Packets are read in player-id order each frame, so GO from P1 is always processed before game packets from the same transfer.
- Disconnect: if the cable reports a different player count for 20 frames, "LINK LOST". Start returns to the title, as it does from DESYNC, LINK ERROR and a WAITING stall longer than 3 s.

## Desync hash

FNV-1a over the frame number, ball position, velocity and spin, every blob's position and velocity, scores, phase, pause, rematch mask, RNG state and last hitter (`sim::hash`).

## Determinism rules for `src/sim/`

Integer / Q8 fixed point only, an integer square root (`sim::isqrt`), no floats, no libm, no hardware calls. `grep -nE "float|double" src/sim` must stay empty.
