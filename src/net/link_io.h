// Butano-side wrapper around gba-link-connection's LinkCable (Multi-Play mode).
#ifndef LV_LINK_IO_H
#define LV_LINK_IO_H

#include <stdint.h>

namespace link_io
{
    // Creates the LinkCable and installs its interrupt handlers the way the library author's
    // Butano example does: VBLANK through bn::core::set_vblank_callback, SERIAL and TIMER1 through bn::hw::irq.
    void init();

    void activate();
    void deactivate();
    void sync();

    [[nodiscard]] bool connected();
    [[nodiscard]] int player_count();   // 1 when alone
    [[nodiscard]] int my_id();

    [[nodiscard]] bool can_read(int player);
    [[nodiscard]] uint16_t read(int player);
    [[nodiscard]] bool can_send();
    bool send(uint16_t data);
    void drain();                       // discard everything queued
}

#endif
