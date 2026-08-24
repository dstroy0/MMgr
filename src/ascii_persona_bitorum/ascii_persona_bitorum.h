#ifndef MMGR_ASCII_PERSONA_BITORUM_H
#define MMGR_ASCII_PERSONA_BITORUM_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


typedef struct
{
    uint8_t b[16];
} MmgrAsciiMask;

MMGR_STATIC_ASSERT(sizeof(MmgrAsciiMask) == 16u, "an ASCII class mask is exactly 128 bits");

typedef enum
{
    MMGR_ASCII_NUM = 0,
    MMGR_ASCII_ALPHA,
    MMGR_ASCII_ALNUM,
    MMGR_ASCII_UPPER,
    MMGR_ASCII_LOWER,
    MMGR_ASCII_HEX,
    MMGR_ASCII_PUNCT,
    MMGR_ASCII_SPACE,
    MMGR_ASCII_CTRL,
    MMGR_ASCII_PRINT,
    MMGR_ASCII_CLASSES
} MmgrAsciiClass;

typedef struct
{
    const MmgrAsciiClass k;     const uint8_t c;        } AsciiCfg;

typedef struct
{
    mmgr_bool (*in)(const AsciiCfg *c);
} AsciiPersonaBitorumNs;
MMGR_NS_LAYOUT(AsciiPersonaBitorumNs, in);

mmgr_bool mmgr_ascii_in(const AsciiCfg *c);

MMGR_NS AsciiPersonaBitorumNs ascii MMGR_UNUSED = {
    .in = mmgr_ascii_in,
};

MMGR_FINIS_DECLS

#endif
