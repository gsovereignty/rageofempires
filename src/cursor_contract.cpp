#include "aoe/cursor_contract.hpp"

static_assert(aoe::cursor::resource_id == 0xc738);
static_assert(aoe::cursor::frame_count == 0x13);
static_assert(aoe::cursor::select(aoe::cursor::State::normal).frame == 0);
static_assert(aoe::cursor::select(aoe::cursor::State::modal_busy).frame == 6);
