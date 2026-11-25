#include "lockstep.h"

namespace net
{

void lockstep::start(int my_id, int player_count)
{
    *this = lockstep();
    _me = my_id;
    _count = player_count;

    // Frames 0..D-1 have no packets: everyone's input is empty.
    uint8_t all = uint8_t((1 << player_count) - 1);

    for(int f = 0; f < INPUT_DELAY; ++f)
    {
        _have[f] = all;
    }

    for(int p = 0; p < MAX_PLAYERS; ++p)
    {
        _next_recv[p] = INPUT_DELAY;
    }
}

void lockstep::receive(int player, uint16_t packet)
{
    if(player < 0 || player >= _count || player == _me || ! is_game_packet(packet))
    {
        return;
    }

    uint32_t f = _next_recv[player];

    if(((packet >> 10) & 15) != (f & 15) || f >= _frame + RING)
    {
        _status = status::SEQUENCE_ERROR;
        bad_frame = f;
        return;
    }

    int slot = int(f % RING);
    _inputs[slot][player] = uint8_t(packet & 63);
    _slice[slot][player] = uint8_t((packet >> 6) & 15);
    _have[slot] |= uint8_t(1 << player);
    _next_recv[player] = f + 1;
}

bool lockstep::ready() const
{
    uint8_t all = uint8_t((1 << _count) - 1);
    uint8_t need = uint8_t(all & ~(1 << _me));
    return (_have[_frame % RING] & need) == need;
}

uint16_t lockstep::local_input(uint8_t buttons)
{
    uint32_t target = _frame + INPUT_DELAY;
    int slot = int(target % RING);

    // Clear the slot we are about to reuse (it held frame target - RING, long done).
    for(int p = 0; p < MAX_PLAYERS; ++p)
    {
        if(p != _me && _next_recv[p] <= target)
        {
            _have[slot] &= uint8_t(~(1 << p));
        }
    }

    // Hash of frame (target - D - 1) = frame - 1: the state we just finished.
    uint32_t h = _frame >= 1 ? _own_hash[(_frame - 1) % RING] : 0;
    _inputs[slot][_me] = uint8_t(buttons & 63);
    _slice[slot][_me] = uint8_t(h & 15);
    _have[slot] |= uint8_t(1 << _me);
    return make_packet(target, h, buttons);
}

void lockstep::inputs(uint8_t out[MAX_PLAYERS]) const
{
    int slot = int(_frame % RING);

    for(int p = 0; p < MAX_PLAYERS; ++p)
    {
        out[p] = p < _count ? _inputs[slot][p] : 0;
    }
}

void lockstep::finish_frame(uint32_t state_hash)
{
    int slot = int(_frame % RING);

    // Packets for this frame carry hash slices of frame (_frame - D - 1).
    if(_frame >= uint32_t(INPUT_DELAY + 1) && _status == status::OK)
    {
        uint32_t checked = _frame - INPUT_DELAY - 1;
        uint32_t mine = _own_hash[checked % RING];

        for(int p = 0; p < _count; ++p)
        {
            if(_slice[slot][p] != (mine & 15))
            {
                _status = status::DESYNC;
                bad_frame = checked;
                my_hash = mine;

                for(int q = 0; q < MAX_PLAYERS; ++q)
                {
                    slices[q] = q < _count ? _slice[slot][q] : 0;
                }

                break;
            }
        }
    }

    _own_hash[slot] = state_hash;

    // This slot will be reused for frame + RING: drop the remote "have" bits that belonged to this frame.
    for(int p = 0; p < MAX_PLAYERS; ++p)
    {
        if(p != _me)
        {
            _have[slot] &= uint8_t(~(1 << p));
        }
    }

    _have[slot] &= uint8_t(~(1 << _me));
    ++_frame;
}

}
