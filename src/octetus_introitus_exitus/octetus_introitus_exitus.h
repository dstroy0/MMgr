#ifndef MMGR_OCTETUS_INTROITUS_EXITUS_H
#define MMGR_OCTETUS_INTROITUS_EXITUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


typedef struct
{
    uint8_t *const at;          const uint8_t *const from;  uint64_t *const out;        const uint64_t val;         const size_t n;         } OctetusCfg;

typedef struct
{
    void (*put)(const OctetusCfg *c);
    void (*take)(const OctetusCfg *c);
} OctetusIntroitusExitusNs;
MMGR_NS_LAYOUT(OctetusIntroitusExitusNs, put, take);

void mmgr_octet_put(const OctetusCfg *c);
void mmgr_octet_take(const OctetusCfg *c);

#define MMGR_BYTEIO_IS_AT(x_) ((void)_Generic((x_), uint8_t *: 0))
#define MMGR_BYTEIO_IS_FROM(x_) ((void)_Generic((x_), const uint8_t *: 0, uint8_t *: 0))
#define MMGR_BYTEIO_IS_VALUE(x_) ((void)_Generic((x_), uint64_t: 0))
#define MMGR_BYTEIO_IS_SIZE(x_) ((void)_Generic((x_), size_t: 0))

#define mmgr_octet_put(at_, val_, n_)                                                                                  \
    (MMGR_BYTEIO_IS_AT(at_), MMGR_BYTEIO_IS_VALUE(val_), MMGR_BYTEIO_IS_SIZE(n_),                                      \
     byteio.put(&(OctetusCfg){.at = (at_), .val = (val_), .n = (n_)}))

#define mmgr_octet_take(from_, out_, n_)                                                                               \
    (MMGR_BYTEIO_IS_FROM(from_), MMGR_BYTEIO_IS_VALUE(out_), MMGR_BYTEIO_IS_SIZE(n_),                                  \
     byteio.take(&(OctetusCfg){.from = (from_), .out = &(out_), .n = (n_)}))

MMGR_NS OctetusIntroitusExitusNs byteio MMGR_UNUSED = {
    .put = mmgr_octet_put,
    .take = mmgr_octet_take,
};

MMGR_FINIS_DECLS

#endif
