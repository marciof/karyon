#pragma once
#include <stdarg.h>
#include <stdint.h>
#include "Err.h"

enum {K_LOG_TAB_SIZE_SPACES = 4};

// FIXME: prefix with `_Level` too? or use helper functions for each one?
enum k_Log_Level {K_LOG_ERROR, K_LOG_WARN, K_LOG_INFO};

// FIXME: use Ref? remove unused `val`?
// FIXME: add timestamps?
struct k_Log {
    void (*print_ch)(
        struct k_Log* log, enum k_Log_Level lvl, char ch, struct k_Err* err);
    void (*print_str)(
        struct k_Log* log, enum k_Log_Level lvl, char* str, struct k_Err* err);
    uint8_t val[];
};

/**
 * Description ends in a newline.
 */
void k_Log_err_details(struct k_Log* log, struct k_Err* err);

/**
 * Log format is a regular string with placeholders denoted by curly braces.
 *
 * Inside each curly brace is a conversion specifier:
 * - "c": character
 * - "s": string or `NULL`
 * - "i": integer
 *   - with "u": unsigned
 *   - with one of:
 *     - "b": base 2
 *     - "h": base 16
 *     - none: base 10
 *
 * To print a literal curly brace, repeat it twice.
 */
void k_Log_msg(
    struct k_Log* log,
    struct k_Err* err,
    enum k_Log_Level lvl,
    char* format,
    ...);

void k_Log_error(
    struct k_Log* log,
    struct k_Err* err,
    char* format,
    ...);

void k_Log_warning(
    struct k_Log* log,
    struct k_Err* err,
    char* format,
    ...);

void k_Log_info(
    struct k_Log* log,
    struct k_Err* err,
    char* format,
    ...);
