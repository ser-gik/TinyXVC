
#pragma once

#define TXVC_UNUSED(param) ((void) (param))
#define TXVC_ALIGNED(factor) __attribute__((aligned(factor)))
#define TXVC_USED __attribute__((used))
#define TXVC_SECTION(name) __attribute__((section(#name)))
#define TXVC_PRINTF_LIKE(fmtIdx, varargIdx) __attribute__((format(printf, fmtIdx, varargIdx)))

