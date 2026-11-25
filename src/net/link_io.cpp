#include "net/link_io.h"

#include "bn_core.h"
#include "bn_memory.h"
#include "../../lib/butano/butano/hw/include/bn_hw_irq.h"

#include "LinkCable.hpp"

LinkCable* linkCable = nullptr;

namespace
{
    BN_CODE_IWRAM void isr_vblank()
    {
        LINK_CABLE_ISR_VBLANK();
    }

    BN_CODE_IWRAM void isr_serial()
    {
        LINK_CABLE_ISR_SERIAL();
    }

    BN_CODE_IWRAM void isr_timer()
    {
        LINK_CABLE_ISR_TIMER();
    }
}

namespace link_io
{

void init()
{
    // Timer 1: Butano's own timers use 2 and 3; the Butano + gba-link-connection example uses 1 too.
    linkCable = new LinkCable(LinkCable::BaudRate::BAUD_RATE_1, 10, LINK_CABLE_DEFAULT_INTERVAL, 1);

    // DMA blocks the CPU and can make the serial interrupt late (packet loss).
    bn::memory::set_dma_enabled(false);

    bn::core::set_vblank_callback(isr_vblank);
    bn::hw::irq::set_isr(bn::hw::irq::id::SERIAL, isr_serial);
    bn::hw::irq::enable(bn::hw::irq::id::SERIAL);
    bn::hw::irq::set_isr(bn::hw::irq::id::TIMER1, isr_timer);
    bn::hw::irq::enable(bn::hw::irq::id::TIMER1);
}

void activate()
{
    if(! linkCable->isActive())
    {
        linkCable->activate();
    }
}

void deactivate()
{
    if(linkCable->isActive())
    {
        linkCable->deactivate();
    }
}

void sync()
{
    linkCable->sync();
}

bool connected()
{
    return linkCable->isConnected();
}

int player_count()
{
    return linkCable->isConnected() ? linkCable->playerCount() : 1;
}

int my_id()
{
    return linkCable->isConnected() ? linkCable->currentPlayerId() : 0;
}

bool can_read(int player)
{
    return linkCable->canRead(uint8_t(player));
}

uint16_t read(int player)
{
    return linkCable->read(uint8_t(player));
}

bool can_send()
{
    return linkCable->canSend();
}

bool send(uint16_t data)
{
    return linkCable->send(data);
}

void drain()
{
    linkCable->sync();

    for(int p = 0; p < 4; ++p)
    {
        while(linkCable->canRead(uint8_t(p)))
        {
            (void) linkCable->read(uint8_t(p));
        }
    }
}

}
