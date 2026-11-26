# MMgr on a GPU {#guide_cuda}

Each module builds for a CUDA device as well as for the host. A kernel calls MMgr through the same tables and the
same declarations as host code.

## What the build needs

nvcc compiles each source of `src/` as CUDA C++, in two passes: one for the host and one for the device.

| need                    | why                                                                                  |
| ----------------------- | ------------------------------------------------------------------------------------ |
| `-x cu`                 | Each `.c` of the tree compiles as CUDA, and each function has a body for each pass.  |
| `-std=c++20`            | `EMBED_CALL` builds its argument struct with designated initializers.                |
| `-rdc=true`             | One module calls another from device code, and the device link joins them.          |
| `/Zc:preprocessor` (cl) | The dispatch tables count `__VA_ARGS__`, and only the new preprocessor of cl counts them right. |

With CMake, `-DMMGR_DEVICE=ON` builds `mmgr_<module>_device` for each module and the aggregate `mmgr_device`. Each
device target takes these settings itself.

On Windows, nvcc compiles its host pass with cl. Run CMake and the build in the environment that `vcvarsall.bat x64`
sets.

## What a program defines

The device pass has no weak default. A program that calls MMgr from a kernel defines each hook below for both passes,
with `EMBED_HOST_DEVICE`. The device link names each hook that the program does not define.

| hook                                                                     | from                 |
| ------------------------------------------------------------------------ | -------------------- |
| `embed_word_wait`, `embed_word_wake`, `embed_clock_ns`, `embed_thread_place` | `embed_turnstile.h`  |
| `mmgr_carcer_hw_clock_ns`, `mmgr_carcer_hw_misnamed`                     | `locus_carcerum.h`   |
| the owner table: `MMGR_OWNERS` defines it for both passes                | `locus_carcerum.h`   |
| `praet_hw_progress` and the `mmgr_praet_hw_*` hooks, in a DMA build      | `memoriam_praetereo` |

## Memory

Each pool that MMgr works with comes in aligned to `MMGR_ALIGN_BYTES`. Pass each buffer aligned.

Each table, pool, cellblock and context has one copy in host memory and one in device memory. Host code reads the host
copy, and a kernel reads the device copy. Both start from the same initializer. A take of a cell in a kernel changes
the device copy alone.

## The stack of each thread

CUDA gives each device thread a stack of 1024 bytes, and that is too small. The frame of `cellul.find` alone is 1344
bytes on sm_86. The device link cannot size the stack of a kernel for you, since a cellblock calls its entries through
pointers. Set the stack before the first launch:

```c
cudaDeviceSetLimit(cudaLimitStackSize, stack_bytes);
```

## The knobs of the DMA schedule

cl has no `#warning`, and so a knob of `memoriam_praetereo` that takes its default stops a build under cl. Set each
knob.
