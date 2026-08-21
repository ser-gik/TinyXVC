/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#define TXVC_UNUSED(param) ((void) (param))
#define TXVC_USED __attribute__((used))
#define TXVC_PRINTF_LIKE(fmtIdx, varargIdx) __attribute__((format(printf, fmtIdx, varargIdx)))
#define TXVC_UNREACHABLE() __builtin_trap()

