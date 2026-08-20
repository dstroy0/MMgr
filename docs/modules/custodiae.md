# Clarus custodiae — the plaintext pool {#mod_clarus_guide}

Per-worker tenants over static storage.

## When to reach for it

- You want interim storage without threading a `mmgr_confin *` through every call.
- You have more than one worker and each needs its own space with no locking.
- The lifetime is "until this operation finishes", and you want one call to reclaim it.

## What it composes with

It carves static storage into one tenant per worker. @ref mod_spat_guide views what it hands out;
@ref mod_occult_guide is the same shape for secrets.

## Worked example

```c
/* No init, no buffer to supply - the storage is static and already there. */
uint8_t *p = clarus.alloc(256);
if (p == NULL) {
    return -1;                      /* this worker's tenant is full */
}

mmgr_spat s = clarus.span(256);     /* the same thing, as a span */

const size_t m = clarus.mark();
/* ... nested work that also allocates ... */
clarus.release(m);                  /* back to the mark */

clarus.reset();                     /* or: the whole tenant, at once */
```

## Sizing it

```c
size_t peak = clarus.high_water();  /* the largest it ever got */
size_t cap  = clarus.capacity();
```

`high_water` is the number to size against, and it only means something after a real workload. Set
`MMGR_PLAINTEXT_CONFIN_SIZE` to the peak plus margin. See @ref ref_configuration.

## Gotchas

**One tenant per worker, decided by `mmgr_worker_self()`.** At `MMGR_WORKER_COUNT == 1` that is a
compile-time constant. Above 1 you must supply `mmgr_platform_context_id()`.

**A pointer must not cross workers.** Nothing enforces it. `clarus.owns(p)` and `clarus.slot_of(p)`
exist for asserts and debugging — code that needs to ask which slot a pointer came from usually
wants to pass the slot instead.

**`persist` here is not the confinium's persist.** It is the pool's long-lived half of one tenant.

**It is not wiped on reset.** For anything sensitive use @ref mod_occult_guide.

## Reference

@ref mod_clarus "Generated reference for clarus_custodiae"

---

# Occultum custodiae — the secure pool {#mod_occult_guide}

The same surface as the plaintext pool, plus a wipe the optimizer may not delete.

## When to reach for it

Anything you would be unhappy to find in a core dump: keys, tokens, credentials, decrypted
plaintext with a short useful life.

## The only real difference

```c
uint8_t *key = occult.alloc(32);
/* ... use it ... */
occult.wipe(key, 32);              /* volatile, word at a time */
occult.reset();
```

`occult.wipe` writes through a `volatile` word pointer. A plain `memset` before a release can be
deleted by the compiler as a dead store — the value is never read again, so the store has no
observable effect under the abstract machine. `volatile` makes it observable, so it survives.

That is the whole difference. Everything else — `alloc`, `span`, `mark`, `release`, `reset`,
`used`, `high_water`, `capacity`, `owns`, `slot_of` — matches `clarus` entry for entry, deliberately,
so moving a buffer from plaintext to secure storage is a change of namespace and not a rewrite.

Its storage is `MMGR_SECURE_CONFIN_SIZE` per slot.

## Gotchas

**`reset` does not wipe.** Call `wipe` first. Reset only moves the bump pointer back.

**A wipe is not a guarantee about the rest of the machine.** It clears this buffer. It does not clear
copies the compiler made in registers or on the stack, and it does nothing about swap, DMA buffers,
or a debugger. See @ref proj_security.

## Reference

@ref mod_occult "Generated reference for occultum_custodiae"
