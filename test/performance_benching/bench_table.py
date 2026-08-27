"""Matrix-table helpers for bench.py: locking, text splicing and verified writes.

Lifted from ProtoCore's test/harness.py, which carries the same machinery for its own matrices.
They sit beside bench.py rather than in MMgr's test/harness.py because bench.py is the only
caller, and that file has a different job.

The table is edited as text rather than reserialized, so a write is a minimal diff and a desc
that took an hour to word is not reflowed by a tool that only changed a flag.
"""

import errno
import json
import os
import re
import time

LOCK_TIMEOUT_S = 120.0  # a writer that cannot get in by then reports rather than racing
LOCK_STALE_S = 300.0  # a lock older than this belonged to a run that died
LOCK_POLL_S = 0.05


def lock_acquire(table):
    """Take the table's lock, or report why not. O_EXCL is the atomic part on both platforms."""
    lock = str(table) + ".lock"
    deadline = time.time() + LOCK_TIMEOUT_S
    while True:
        try:
            fd = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            os.write(fd, str(os.getpid()).encode())
            os.close(fd)
            return lock
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise
        try:
            if time.time() - os.path.getmtime(lock) > LOCK_STALE_S:
                os.unlink(lock)  # the holder is gone; take it on the next pass
                continue
        except OSError:
            continue  # it vanished between the test and the stat: retry
        if time.time() > deadline:
            return None
        time.sleep(LOCK_POLL_S)


def lock_release(lock):
    try:
        os.unlink(lock)
    except OSError:
        pass


def env_span(text, name):
    """(pad, key_start, close) for an env: the indent it sits at, the index of its opening quote,
    and the index of its object's matching close brace.

    The brace scan steps over string literals. A desc is free text and several carry a lone brace
    ("'[' vs '{'"), which a counter that reads every character would take for structure.
    """
    m = re.search(r'^([ \t]*)"%s"\s*:\s*\{' % re.escape(name), text, re.M)
    if not m:
        raise KeyError(name)
    depth = 0
    start = m.start() + len(m.group(1))
    i = text.index("{", m.start())
    while i < len(text):
        c = text[i]
        if c == '"':
            i += 1
            while i < len(text) and text[i] != '"':
                i += 2 if text[i] == "\\" else 1
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return m.group(1), start, i
        i += 1
    raise KeyError(name)


def reindent(block, pad):
    return "\n".join(pad + l[2:] if l.startswith("  ") else pad + l.strip() for l in block.split("\n"))


def splice_after(text, anchor, name, entry):
    """Insert entry as text directly after the anchor env's closing brace."""
    pad, _, close = env_span(text, anchor)
    block = json.dumps({name: entry}, indent=2)[1:-1].rstrip()
    return text[: close + 1] + ",\n" + reindent(block, pad) + text[close + 1 :]


def splice_replace(text, name, entry):
    """Replace an env's whole `"name": {...}` in place, rendered at the indent it already sits at.

    The same render as splice_after, so an updated env and a new one are indented identically. The
    leading pad is dropped because the text kept ahead of key_start already carries it.
    """
    pad, key_start, close = env_span(text, name)
    block = json.dumps({name: entry}, indent=2)[1:-1].rstrip()
    return text[:key_start] + reindent(block, pad).lstrip() + text[close + 1 :]


def splice_remove(text, name):
    """Cut an env's whole `"name": {...}` out, taking the one comma that joined it to its neighbours.

    The comma sits after the close brace for every env but the last, where it sits before the key.
    Removing the wrong one, or neither, leaves the table unparseable.
    """
    _, key_start, close = env_span(text, name)
    end = close + 1
    tail = text[end:]
    lead = len(tail) - len(tail.lstrip(" \t\r\n"))
    if tail[lead : lead + 1] == ",":
        end += lead + 1  # not the last env: take the comma that follows it
        while end < len(text) and text[end] in " \t":
            end += 1
        if text[end : end + 1] == "\n":
            end += 1
        return text[:key_start] + text[end:]
    head = text[:key_start]
    cut = len(head.rstrip(" \t\r\n"))
    if head[cut - 1 : cut] == ",":
        cut -= 1  # the last env: take the comma that preceded it
    return text[:cut] + text[end:]


def read_table(path):
    with open(path, "r", encoding="utf-8") as fh:
        text = fh.read()
    return text, json.loads(text)


def write_verified(path, text, before, changed, expect):
    """Write only if the reparsed table matches `expect` for `changed` and is untouched elsewhere."""
    after = json.loads(text)
    for name, want in expect.items():
        if after["envs"].get(name) != want:
            print("the spliced env did not round-trip:", name)
            return 1
    for k in before["envs"]:
        if k in changed:
            continue
        if before["envs"][k] != after["envs"].get(k):
            print("collateral change in", k)
            return 1
    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)
    return 0
