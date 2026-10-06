/* null-adpcm C API.  SPDX-License-Identifier: MIT */
#ifndef NULLADPCM_MSM6258_C_H
#define NULLADPCM_MSM6258_C_H
#include <stddef.h>
#include <stdint.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define NULLADPCM_API EMSCRIPTEN_KEEPALIVE
#else
#define NULLADPCM_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nulladpcm_t nulladpcm_t;
typedef void (*nulladpcm_data_request_cb)(void* user);

NULLADPCM_API size_t nulladpcm_size(void);
NULLADPCM_API nulladpcm_t* nulladpcm_create(uint32_t clock_hz, int divider /*0:1024 1:768 2:512*/);
NULLADPCM_API nulladpcm_t* nulladpcm_create_in(void* buf, size_t len, uint32_t clock_hz, int divider);
NULLADPCM_API void nulladpcm_destroy(nulladpcm_t* c);
NULLADPCM_API void nulladpcm_reset(nulladpcm_t* c);
NULLADPCM_API void nulladpcm_set_clock(nulladpcm_t* c, uint32_t clock_hz, int divider);
NULLADPCM_API uint32_t nulladpcm_sample_rate(const nulladpcm_t* c);
NULLADPCM_API void nulladpcm_write_command(nulladpcm_t* c, uint8_t v);
NULLADPCM_API void nulladpcm_write_data(nulladpcm_t* c, uint8_t v);
NULLADPCM_API uint8_t nulladpcm_read_status(const nulladpcm_t* c);
NULLADPCM_API int nulladpcm_data_requested(const nulladpcm_t* c);
NULLADPCM_API void nulladpcm_generate(nulladpcm_t* c, int16_t* out, size_t n);
NULLADPCM_API void nulladpcm_set_data_request_callback(nulladpcm_t* c, nulladpcm_data_request_cb cb, void* user);
NULLADPCM_API const char* nulladpcm_version(void);
#ifdef __cplusplus
}
#endif
#endif
