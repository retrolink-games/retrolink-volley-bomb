// Lockstep input exchange. Pure logic: no hardware calls, testable on desktop.
//
// Game packet (16 bits, one per player per frame):
//   bit 15      1
//   bit 14      0          -> game packet (never 0x0000 or 0xFFFF, the LinkCable reserved values)
//   bits 13..10 frame mod 16 of the input
//   bits 9..6   low 4 bits of the sender's state hash for frame (N - D - 1)
//   bits 5..0   buttons
// Lobby packets have bits 15 and 14 set; their low 12 bits never reach 0xFFF, so they can't be 0xFFFF:
//   0xD000 | (player_count - 1) << 8 | seed8   "GO" from player 0
#ifndef LV_LOCKSTEP_H
#define LV_LOCKSTEP_H

#include <stdint.h>

namespace net
{

constexpr int INPUT_DELAY = 2;       // D: tunable to 3
constexpr int RING = 32;
constexpr int MAX_PLAYERS = 4;

constexpr uint16_t make_packet(uint32_t frame, uint32_t hash, uint8_t buttons)
{
    return uint16_t(0x8000 | ((frame & 15) << 10) | ((hash & 15) << 6) | (buttons & 63));
}

constexpr bool is_game_packet(uint16_t p) { return (p & 0xC000) == 0x8000; }
constexpr bool is_go_packet(uint16_t p) { return (p & 0xF000) == 0xD000; }
constexpr uint16_t make_go(int player_count, uint8_t seed) { return uint16_t(0xD000 | ((player_count - 1) << 8) | seed); }
constexpr int go_players(uint16_t p) { return ((p >> 8) & 3) + 1; }
constexpr uint8_t go_seed(uint16_t p) { return uint8_t(p & 0xFF); }

enum class status : uint8_t
{
    OK,
    DESYNC,          // a hash slice did not match
    SEQUENCE_ERROR   // a packet arrived with the wrong frame number (lost or duplicated message)
};

class lockstep
{
public:
    void start(int my_id, int player_count);

    // Feed one received game packet from a remote player.
    void receive(int player, uint16_t packet);

    // True when every player's input for the next frame is here.
    [[nodiscard]] bool ready() const;

    // Call when ready(): stores the local input for frame + D and returns the packet to send.
    uint16_t local_input(uint8_t buttons);

    // Inputs for the frame about to be simulated (indexed by player id).
    void inputs(uint8_t out[MAX_PLAYERS]) const;

    // Call after simulating the frame with the resulting state hash. Checks remote hash slices.
    void finish_frame(uint32_t state_hash);

    [[nodiscard]] uint32_t frame() const { return _frame; }
    [[nodiscard]] status state() const { return _status; }
    [[nodiscard]] int player_count() const { return _count; }

    // Desync report: the frame whose hash differed, my full hash, and each player's 4-bit slice.
    uint32_t bad_frame = 0;
    uint32_t my_hash = 0;
    uint8_t slices[MAX_PLAYERS] = {};

private:
    int _me = 0;
    int _count = 1;
    uint32_t _frame = 0;                       // next frame to simulate
    uint32_t _next_recv[MAX_PLAYERS] = {};     // next frame expected from each player
    uint8_t _inputs[RING][MAX_PLAYERS] = {};
    uint8_t _have[RING] = {};                  // bitmask of players whose input for that slot is here
    uint8_t _slice[RING][MAX_PLAYERS] = {};
    uint32_t _own_hash[RING] = {};
    status _status = status::OK;
};

}

#endif
