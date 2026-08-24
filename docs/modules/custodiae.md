# Custodia soluta — the plaintext pool {#mod_clarus_guide}

A tenant over static storage.

## When to reach for it

- You want pool storage without threading a `CarcerCtx *` through every call.
- You want a region without declaring the buffer yourself.
- The lifetime is "until this operation finishes", and you want one call to reclaim it.

## What it composes with

It is one tenant over one static buffer. @ref mod_spat_guide views what it hands out;
@ref mod_occult_guide is the same shape for secrets.

## Worked example

```c
CarcerCtx *const pool = MMGR_CARCER_POOL(g_ram, g_plain);

uint8_t *const p = MMGR_CALL(soluta.init, SolutaCfg, .pool = pool, .bytes = 256u);
if (p == NULL) {
    return -1;
}

/* ... use p ... */

MMGR_CALL(soluta.release, SolutaCfg, .pool = pool, .bytes = 256u);
```

Three entries: `init` takes, `release` gives back, `used` reports what is outstanding. There is no
`alloc`, no `span`, no `mark` and no `reset` — the mark and rewind pair belongs to the region's
interim end, in @ref mod_confin_guide.

## Sizing it

```c
const size_t out  = MMGR_CALL(soluta.used, SolutaCfg, .pool = pool);
const size_t left = MMGR_CALL(carcer.octas_praesto, CarcerCfg, .pool = pool);
const size_t peak = pool->hw;   /* MMGR_ENABLE_HW_MEM_CAPACITY_CB only */
```

`used` is what is outstanding now. The peak is the pool's `hw` field, the hardware heap and stack
cap, and it is only maintained when `MMGR_ENABLE_HW_MEM_CAPACITY_CB` is on — off by default, so a
run without it leaves `hw` at zero. Size against the peak after a real workload and set
`MMGR_PLAINTEXT_CONFIN_SIZE` to it plus margin. See @ref ref_configuration.

## Gotchas

**One tenant, and it does not ask who is calling.** A pool is one region over one static buffer.
If two execution contexts must not share, declare two regions with @ref mod_confin_guide and hand
each context its own - the region never learns there were two.

**A pointer outlives nothing.** `carcer.owns` says whether a pointer is inside the pool at all. It
exists for asserts and debugging, not for control flow.

**These entries are the region's persist end.** `soluta.init` is `carcer.persist_capio` with the
pool's policy on top; the interim end is reached through @ref mod_confin_guide directly.

**Release does not clear.** For anything sensitive use @ref mod_occult_guide.

## Reference

@ref mod_clarus "Generated reference for custodia_soluta"

---

# Custodia secura — the secure pool {#mod_occult_guide}

The same surface as the plaintext pool, plus a clear on release.

## When to reach for it

Anything you would be unhappy to find in a core dump: keys, tokens, credentials, decrypted
plaintext with a short useful life.

## The only real difference

```c
CarcerCtx *const pool = MMGR_CARCER_POOL(g_ram, g_secret);

uint8_t *const key = MMGR_CALL(secura.init, SecuraCfg, .pool = pool, .bytes = 32u);

/* ... use key ... */

MMGR_CALL(secura.release, SecuraCfg, .pool = pool, .bytes = 32u);   /* clears, then returns */
```

`release` clears the bytes before returning them, which is the whole difference from `soluta`. It
costs a pass over them, which is why both modules exist.

`secura.wipe` clears in place without releasing, for when a secret is finished with but the buffer
is not.

Everything else — `init`, `release`, `used` — matches `soluta` entry for entry, deliberately, so
moving a buffer from plaintext to secure storage is a change of namespace and not a rewrite.

Its storage is `MMGR_SECURE_CONFIN_SIZE`.

@warning The clear is an unrolled word-at-a-time store loop and the stores are **not** `volatile`.
A compiler able to prove the region is dead afterwards would be entitled to drop them; it survives
because the storage comes from a pool in another translation unit, which is a property of this
build rather than a guarantee. It also clears whole words only, so a length that is not a multiple
of `sizeof(uintptr_t)` leaves the trailing bytes alone.

## Gotchas

**`soluta.release` on secure storage does not clear.** The clearing is `secura.release`. Reaching
past the namespace to the region's `carcer.persist_reddo` skips it too — it only moves the cursor.

**A wipe is not a guarantee about the rest of the machine.** It clears this buffer. It does not clear
copies the compiler made in registers or on the stack, and it does nothing about swap, DMA buffers,
or a debugger. See @ref proj_security.

## Reference

@ref mod_occult "Generated reference for custodia_secura"
