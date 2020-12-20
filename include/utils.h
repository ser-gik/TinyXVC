
#pragma once

#define TXVC_UNUSED(param) ((void) (param))

#define TXVC_PRINTF_LIKE(fmtIdx, varargIdx) __attribute__((format(printf, fmtIdx, varargIdx)))

extern const char* byte_to_bitstring(unsigned char c);

extern unsigned long long current_time_micros(void);

