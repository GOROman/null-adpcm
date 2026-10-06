// SPDX-License-Identifier: MIT
#include "nulladpcm/msm6258_c.h"
#include "nulladpcm/msm6258.hpp"
#include <new>
namespace {
struct CChip {
    nulladpcm::Msm6258 chip;
    nulladpcm_data_request_cb cb = nullptr;
    void* user = nullptr;
    CChip(uint32_t clk, int div) : chip(clk, static_cast<nulladpcm::Divider>(div & 3)) {}
};
void tramp(void* u) {
    auto* c = static_cast<CChip*>(u);
    if (c->cb) c->cb(c->user);
}
void install(CChip* c) {
    nulladpcm::Callbacks cb;
    cb.data_request = tramp;
    cb.user = c;
    c->chip.set_callbacks(cb);
}
CChip* C(nulladpcm_t* p) { return reinterpret_cast<CChip*>(p); }
const CChip* C(const nulladpcm_t* p) { return reinterpret_cast<const CChip*>(p); }
nulladpcm::Divider D(int d) { return static_cast<nulladpcm::Divider>(d > 2 ? 2 : (d < 0 ? 0 : d)); }
} // namespace
extern "C" {
size_t nulladpcm_size(void) { return sizeof(CChip); }
nulladpcm_t* nulladpcm_create(uint32_t clk, int div) {
    auto* c = new (std::nothrow) CChip(clk, div);
    if (!c) return nullptr;
    install(c);
    return reinterpret_cast<nulladpcm_t*>(c);
}
nulladpcm_t* nulladpcm_create_in(void* buf, size_t len, uint32_t clk, int div) {
    if (!buf || len < sizeof(CChip) || reinterpret_cast<uintptr_t>(buf) % alignof(CChip)) return nullptr;
    auto* c = new (buf) CChip(clk, div);
    install(c);
    return reinterpret_cast<nulladpcm_t*>(c);
}
void nulladpcm_destroy(nulladpcm_t* c) { delete C(c); }
void nulladpcm_reset(nulladpcm_t* c) { C(c)->chip.reset(); }
void nulladpcm_set_clock(nulladpcm_t* c, uint32_t clk, int div) {
    C(c)->chip.set_clock(clk);
    C(c)->chip.set_divider(D(div));
}
uint32_t nulladpcm_sample_rate(const nulladpcm_t* c) { return C(c)->chip.sample_rate(); }
void nulladpcm_write_command(nulladpcm_t* c, uint8_t v) { C(c)->chip.write_command(v); }
void nulladpcm_write_data(nulladpcm_t* c, uint8_t v) { C(c)->chip.write_data(v); }
uint8_t nulladpcm_read_status(const nulladpcm_t* c) { return C(c)->chip.read_status(); }
int nulladpcm_data_requested(const nulladpcm_t* c) { return C(c)->chip.data_requested(); }
void nulladpcm_generate(nulladpcm_t* c, int16_t* out, size_t n) { C(c)->chip.generate(out, n); }
void nulladpcm_set_data_request_callback(nulladpcm_t* c, nulladpcm_data_request_cb cb, void* user) {
    C(c)->cb = cb;
    C(c)->user = user;
}
const char* nulladpcm_version(void) { return nulladpcm::version(); }
}
