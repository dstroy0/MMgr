# LocusCarcerum

This document uses ASD-STE100 Simplified Technical English. The mathematical notation does not change.

## 1. Definitions

$S \in \mathbb{N}$ and $8 \mid S$. The header size is $h = 16$.

A **cell** is a pair $(o, p) \in \mathbb{N}^2$ with $8 \mid o$ and $8 \mid p$. The cell occupies $[o,\ o + h + p)$.

A **chain from $a$ to $b$** is a list of cells $c_1, \dots, c_k$ with $o_1 = a$, $o_{i+1} = o_i + h + p_i$ and
$o_k + h + p_k = b$. A chain with no cells is a chain from $a$ to $a$.

The **state** is $\sigma = (E, T, O_E, O_T, P, Q)$:

- $E \in \mathbb{N}$ is the persistent end.
- $T \in \mathbb{N}$ is the temporary top.
- $O_E, O_T \in \mathbb{N}$ are the overruns.
- $P$ is a chain from $0$ to a number $e_P$. These are the persistent cells.
- $Q$ is a chain from a number $t_Q$ to $S$. These are the temporary cells.

The start state is $\sigma_0 = (0, S, 0, 0, \varnothing, \varnothing)$.

## 2. Invariant

$$
I(\sigma) \;\equiv\; E = e_P + O_E \;\wedge\; T = t_Q - O_T \;\wedge\; e_P \le t_Q
$$

The **rest** condition is $R(\sigma) \equiv I(\sigma) \wedge O_E = 0 \wedge O_T = 0$. At rest, $E = e_P$ and
$T = t_Q$.

## 3. Operations

Each operation is a function $\Sigma \to \Sigma \times \{\bot\} \cup \mathbb{N}$. The operations of §3.1 and §3.2
can interleave with each other. An operation of §3.3 runs only when no other operation is in progress.

$E$ and $T$ are the two halves of one atomic word of $2H$ bits, $W = T \cdot 2^H + E$. Each read of a boundary
reads $W$. Each move of a boundary is one read-modify-write on $W$. The old value of this read-modify-write contains
the two halves. Two limits prevent a move out of its half:

$$
S \le 2^{H-1},\qquad n \le h + 2^{H-8}
$$

A bump up does not add to $W$ and returns $\bot$ if one of these conditions is true:

- Its request is more than the second limit.
- It reads $E > 2^{H-1}$ before its read-modify-write.

This bump does not change the overrun. The exclusive path does the request.

A take or a growth with a request of more than $S$ bytes returns $\bot$ before it rounds the request. It does not
change the state. Thus the round-up of a request to a multiple of 8 does not wrap, and $h + p$ does not wrap. The
cell that a take returns is never smaller than its request.

Thus $E$ is never more than $2^{H-1} + L\,(h + 2^{H-8})$, where $L$ is the number of bumps in progress at the same
time. $E$ does not carry into the half of $T$ while $2^{H-1} + L\,(h + 2^{H-8}) < 2^H$. This permits up to 127 bumps
in progress at $H = 32$, and up to 120 at $H = 16$.

Each bump uses one lane of the turnstiles, and no other bump uses that lane at the same time. Thus $L$ is not more
than the number of lanes. There are 63 lanes at $H = 32$ and 31 lanes at $H = 16$. A static assert in the C stops
the build if $S + L\,(h + 2^{H-8}) \ge 2^H$ for this number of lanes.

A bump down subtracts $n \cdot 2^H$. It does not change the half of $E$. A borrow out of the half of $T$ goes out of
the word.

A bump down does not read $E$ before its read-modify-write. Thus the limit on bumps down in progress is the number
of lanes only. The bumps down in progress subtract not more than $L\,(h + 2^{H-8})$ from $T$. If they move $T$ below
0, the half of $T$ wraps to a value of more than $2^H - L\,(h + 2^{H-8})$. This value is more than $S$, because
$S + L\,(h + 2^{H-8}) < 2^H$. Thus the condition $w - n \le S$ of §3.2 rejects each bump that reads the wrapped top,
and no bump puts a cell below 0.

### 3.1 Bump up, need $n$ ($n = h + p$, $8 \mid p$)

$$
(w, T_w) := (E, T),\quad W := W + n
$$

$$
\tau \equiv (w + n \le T_w) \wedge (w + n \ge w) \wedge (T_w \le S)
$$

$$
\tau \Rightarrow P := P \cdot (w, p),\ \text{answer } w + 8
$$

$$
\neg\tau \Rightarrow O_E := O_E + n,\ \text{answer } \bot
$$

The first line is one atomic read-modify-write on $W$. $w$ and $T_w$ are the two halves of $W$ before the add. The
fourth line is one atomic add. No operation subtracts from $E$.

### 3.2 Bump down, need $n$

$$
(E_w, w) := (E, T),\quad W := W - n \cdot 2^H
$$

$$
\tau \equiv (w - n \ge E_w) \wedge (w - n \le w) \wedge (w - n \le S)
$$

$$
\tau \Rightarrow Q := (w - n, p) \cdot Q,\ \text{answer } w - n + 8
$$

$$
\neg\tau \Rightarrow O_T := O_T + n,\ \text{answer } \bot
$$

### 3.3 Settle

$$
E := E - O_E,\quad O_E := 0,\quad T := T + O_T,\quad O_T := 0
$$

Each exclusive operation starts with Settle. The exclusive operations are spill, trim, growth and sort.
While an exclusive operation runs, no operation of §3.1 or §3.2 is in progress, and no such operation starts.

### 3.4 Free, cell $(o,p) \in P \cup Q$

The operation sets the state bit of the cell to free. $P$, $Q$, $E$ and $T$ do not change. Then a full fence
follows, and the operation reads the closed bit and the two edges. If the cell is at the edge of $e_P$ or at the
edge of $t_Q$ after this read, or if the closed bit is 1, a Trim follows. A Trim that starts before the free reads
the cell as free, or stores an edge that this read sees.

The free operation does not wait for an exclusive operation. One read-modify-write sets the closed bit and the trim
bit of the tier of the cell. If the closed bit was 0, the free operation does the Trim. If the closed bit was 1,
the operation stops, and the trim bit stays set. Each exclusive operation ends with one open pass:

1. Clear the trim bits with one read-modify-write, and Trim each tier whose bit was 1.
2. Clear the closed bit and the sleeper bit with one read-modify-write. A trim bit that a free sets between the
   two steps stays set.
3. If the word had the sleeper bit before step 2, wake the sleepers on the word.

Each take and each read of the gap starts with a check of the trim bits. If the word holds a trim bit, the
operation sets the closed bit, does the open pass, and then continues. Thus, assume that the word holds no trim bit
and that no free operation is in progress. Then no free cell is at the edge of $e_P$ or at the edge of $t_Q$.

### 3.5 Trim (exclusive)

1. Settle.
2. Remove the longest free suffix of $P$, and set $E := e_P$.
3. Remove the longest free prefix of $Q$, and set $T := t_Q$.

### 3.6 Spill up, need $n$ (exclusive)

1. Settle.
2. Merge the consecutive free cells of $P$.
3. If a free cell $(o, p') \in P$ with $p' \ge p$ exists, replace the first of these cells with $(o, p)$.
4. If $p' - p \ge h + 8$, put a free cell $(o + h + p,\ p' - p - h)$ after it. Return $o + 8$.
5. If $P$ contains no such cell, Trim.
6. If $T - E \ge n$, set $P := P \cdot (E, p)$ and $E := E + n$. Return $E_{\text{old}} + 8$.
7. If not, return $\bot$.

### 3.7 Spill down, need $n$ (exclusive)

1. Settle.
2. Trim.
3. If $T - E \ge n$, set $Q := (T - n, p) \cdot Q$ and $T := T - n$. Return $T + 8$.
4. If not, return $\bot$.

### 3.8 Regions

Where the build declares $N > 0$ regions of $R$ bytes and a large region of $L$ bytes, the temporary tier is the
regions, and $T = S - N R - L$ never moves. The large region lies from $S - L$ to $S$. Region $i$ lies from
$S - L - (i + 1) R$ to $S - L - i R$. Each has a depth $D_i$, the bytes its stack holds from its end down.

A thread's place $k$, which the host gives, selects region $i = k \bmod N$ for a take within $R$. A take over $R$
goes to the large region, from any thread. Where $L = 0$, the cellblock refuses it. A take within $R$ that region $i$ refuses,
bumped and spilled, goes to the large region the same way. The large region refuses it the same way.

Bump in region $i$, need $n$: $D_i := D_i + n$ by one fetch-add. If $D_i \le R$ after it, the cell lies at
$S - i R - D_i$. If not, $D_i := D_i - n$ by one fetch-sub, before the pass ends, and the take goes to the spill.
So a writer that drains the passes reads every $D_i$ exact.

Spill in region $i$, need $n$ (exclusive): the region swept as §3.5 sweeps the tier, from $S - i R - D_i$ to
$S - i R$. The free run at its top returns to the region's gap. Then the last run that holds $n$, else the gap
where it holds $n$, else $\bot$ with no busy cell moved.

Free of a cell in region $i$: as §3.4, with the region's top $S - i R - D_i$ in place of $T$. The trim of §3.5
sweeps each region and sets each $D_i$ from the free run at its top.

The idle walk passes the program, with the cellblock closed, the gap $[E, T)$ and each region's room
$[S - L - (i + 1) R, S - L - i R - D_i)$, the large region's from $S - L$. `Idle.lean` proves that no byte of a
span lies in a cell. A cellblock over a hospes pool has $S = 0$ until the program places it, and a take of $n$
bytes permits nothing unless $n < S$. `Idle.lean` proves that an unplaced cellblock refuses every take.

`Regions.lean` proves that each $D_i$ stays within its region's bound after any sequence of takes and gives from
any thread. It proves that a refused take moves no busy cell, and that the regions together stay within
$N R + L$. `Stacks.lean` proves that one stack two threads share sinks past every depth. A thread's own region
has one thread, so it does not sink; the large region is the one stack threads share, and $L$ bounds it.

## 4. Lemmas

A **round** is a max interval between two exclusive operations. In a round, only the operations of §3.1, §3.2
and §3.4 run.

**Lemma 1** (monotone in a round). In a round, $E$ does not decrease and $T$ does not increase.

_Proof._ §3.1 adds $n > 0$ to $E$. §3.2 subtracts $n > 0$ from $T$. §3.4 does not change $E$ or $T$. No other
operation of the round changes them. $\square$

**Lemma 2** (a rejection is absorbing). In a round, §3.1 rejects a bump up $B_1$. Then §3.1 also rejects each
bump up $B_2$ whose read-modify-write follows the read-modify-write of $B_1$.

_Proof._ If §3.1 rejects $B_1$ and $B_1$ does not wrap, then $w_1 + n_1 > T_1$. $T_1$ is the value of $T$ that $B_1$
read. From Lemma 1, $w_2 \ge w_1 + n_1 > T_1 \ge T_2$.

$T_1 \ge T_2$ is true because $B_2$ reads $T$ in the same atomic step as its read-modify-write. This
read-modify-write follows the read-modify-write of $B_1$. Thus $w_2 + n_2 > T_2$, and $\tau_2$ is false.

If §3.1 rejects $B_1$ because it wraps, then $w_1 + n_1 < w_1$, and $E$ is past $2^{64} - n_1$. Then $w_2 \ge w_1 +
n_1$ modulo $2^{64}$ has one of these two results:

- It wraps, and the second conjunct rejects it.
- It is more than $S$. Because $T_2 \le S$, the first conjunct rejects it. $\square$

The mirror statement is true for bumps down, with $E$ in place of $T$.

All of Lemma 2 is the step $T_1 \ge T_2$. This step is true only if the first line of §3.1 is one atomic step on $E$
and $T$ together. Thus one word must contain the two boundaries. §6.2 shows the result with two words.

**Lemma 3** (accepted bumps are contiguous). In a round, $B_1, \dots, B_k$ are the accepted bumps up, in
read-modify-write order. Then $o_1 = e_P$ at the start of the round, and $o_{i+1} = o_i + n_i$.

_Proof._ From Lemma 2, each accepted bump is before each rejected bump in read-modify-write order. Thus nothing
changes $E$ between two consecutive accepted bumps, and $w_{i+1} = w_i + n_i$. The first accepted bump reads the
value of $E$ at the end of the previous round. Because $R$ is true at the start of the round, this value is $e_P$.
$\square$

**Lemma 4** ($I$ stays true in a round). $R(\sigma)$ is true at the start of a round. Then $I(\sigma)$ is true for
all of the round, after the two atomic operations of each bump are complete.

_Proof._ From Lemma 3, the accepted bumps extend $P$ without gaps. Thus $e_P$ increases by exactly the needs of the
accepted bumps. $E$ increases by each need, accepted or rejected, and $O_E$ increases by each rejected need. Thus
$E - O_E = e_P$. The mirror is true for $T$.

For $e_P \le t_Q$: an accepted bump up has $w + n \le T \le t_Q$. Thus $e_P \le t_Q$ after it. The mirror is true for
a bump down. $\square$

**Lemma 5** (Settle puts the state at rest). If $I(\sigma)$ is true and no bump is in progress, Settle causes
$R(\sigma)$.

_Proof._ From $I$, $E - O_E = e_P$ and $T + O_T = t_Q$. Settle writes these values and sets the overruns to zero.
$\square$

## 5. Theorem

**Theorem.** Start at $\sigma_0$, and interleave §3.1, §3.2 and §3.4 in any sequence between exclusive operations.
Then these two statements are true:

1. Each exclusive operation finds $R(\sigma)$ after its Settle.
2. Each returned address $o + 8$ is in a cell of $P \cup Q$, and this cell does not overlap other cells.

_Proof._ $R(\sigma_0)$ is true. Use induction on rounds. If $R$ is true at the start of a round, $I$ is true for all
of the round (Lemma 4). The exclusive operation at the end of the round finds no bump in progress, and its Settle
causes $R$ (Lemma 5).

Each exclusive operation of §3.5 to §3.7 changes a chain into a chain. It sets $E = e_P$ and $T = t_Q$, with
$e_P \le t_Q$. Thus $R$ is true at the start of the next round.

The cells do not overlap. Accepted cells are consecutive in a chain (Lemma 3), and $e_P \le t_Q$ separates $P$ from
$Q$. $\square$

## 6. Two ways in which Lemma 2 fails

### 6.1 The undo

Replace the rejected branch of §3.1 with $E := E - n$. Then $E$ is not monotone in a round, and Lemma 2 fails:

$$
\begin{array}{lll}
B:\ w = 6088,\ n = 64 & E \to 6152 & \neg\tau \\
C:\ w = 6152,\ n = 64 & E \to 6216 & \neg\tau,\ \text{undo}\ E \to 6152 \\
D:\ w = 6152,\ n = 24 & E \to 6176 & \neg\tau \\
B\ \text{undo} & E \to 6112 & \\
X:\ w = 6112,\ n = 32 & E \to 6144 & \tau,\quad 6144 \le T = 6144 \\
D\ \text{undo} & E \to 6120 &
\end{array}
$$

§3.1 accepts $X$ at $o = 6112$, but $e_P = 6088$. The chain has a gap, and the cell of $X$ is past $E$.

### 6.2 Two words

**Lemma 6** (two words are not absorbing). $E$ and $T$ are two atomic words. Thus a bump is two steps: first the
read-modify-write $w := E,\ E := E + n$, and later the read of $T$ that decides $\tau$.

Then Lemma 2 fails. The check can reject a bump $A$ and accept a bump $B$, although the read-modify-write of $A$ is
before the read-modify-write of $B$. After the round, $I$ is false.

_Proof._ The example is an interleaving with $S = 2^{20}$. The round starts at rest with
$E = e_P = \mathtt{fd4c0}$ and $T = t_Q = \mathtt{fd868}$:

$$
\begin{array}{llll}
A:\ w_A = \mathtt{fd4c0},\ n_A = \mathtt{1e8} & E \to \mathtt{fd6a8} & & \\
B:\ w_B = \mathtt{fd6a8},\ n_B = \mathtt{a8} & E \to \mathtt{fd750} & & \\
B\ \text{reads}\ T = \mathtt{fd868} & & \mathtt{fd750} \le \mathtt{fd868} & \tau_B,\ P := P \cdot (\mathtt{fd6a8}, \mathtt{98}) \\
C:\ w_C = \mathtt{fd868},\ n_C = \mathtt{210} & T \to \mathtt{fd658} & & \\
A\ \text{reads}\ T = \mathtt{fd658} & & \mathtt{fd6a8} > \mathtt{fd658} & \neg\tau_A,\ O_E := \mathtt{1e8} \\
C\ \text{reads}\ E = \mathtt{fd750} & & \mathtt{fd658} < \mathtt{fd750} & \neg\tau_C,\ O_T := \mathtt{210}
\end{array}
$$

$B$ reads $T$ before $C$ moves it, and $A$ reads $T$ after this move. Thus $T_B > T_A$, but the read-modify-write of
$A$ is before the read-modify-write of $B$. Here the step $T_1 \ge T_2$ of Lemma 2 fails.

After the round, $E - O_E = \mathtt{fd568} < e_P = \mathtt{fd750}$. The rejected bytes of $A$ are below the cell of
$B$. Settle subtracts them from the top, and $E$ goes into the gap below a cell that is in use. The check of §3.1
accepts the next bump up over the cell of $B$. $\square$

Thus the two boundaries must be one word. The first line of §3.1 and the first line of §3.2 must each be one
read-modify-write on this word. $E$ is the low half of the word, and $T$ is the high half. A bump up adds $n$ to the
low half and reads the high half in the same step. Because of the order of the word, $T_1 \ge T_2$ is true.

Lean checks the models in `theory/locus_carcerum`. The theory requires embedded_types' theory, and the files on the
turnstile import its Manifold proofs: `Order.lean` carries `Manifold.Open.passes_below_open` and
`Manifold.Drain.drain_lane_complete`, `Pass.lean` carries the drain's completion, and `Sleep.lean` carries
`Manifold.Backoff.limit_fits`.

- `LocusCarcerum/Packed.lean` proves Lemmas 1 to 5 and the theorem for one word.
- `LocusCarcerum/Split.lean` writes the two-word steps and checks the example above.
- `LocusCarcerum/Layout.lean` writes the persistent tier as its cells. It proves that the sweep retains every byte
  positioned, the busy cells in order and the count of each owner. It proves that no two free cells are together
  after the merge and that no free cell is at the end after the trim. It also proves that the end does not rise.
- `LocusCarcerum/Take.lean` proves that a take over a free run retains the bytes of the run. It proves that the cell
  carries at least the request, and less than the request plus a header and the alignment. It proves that the count
  of the owner rises by what the cell carries. It proves that the mover retains the count of each owner equal to the
  count the layout holds.
- `LocusCarcerum/Spill.lean` proves the spill over the first free run, the bump at the end, and both growths. For
  each, it proves the bytes positioned and the count of each owner after it. It also proves that a temporary growth
  retains the owner word in its place.
- `LocusCarcerum/Give.lean` proves that a give counts a cell off its owner once over any sequence of gives. Gives
  that name another owner do not count it off at all.
- `LocusCarcerum/Temporary.lean` writes the temporary tier as the persistent tier mirrored, its base at the
  cellblock's size. It proves that the sweep retains the busy cells and the counts and does not raise the tier's
  bytes. It also proves that the sweep leaves no free cell at the top. It proves that the last run that fits is free, holds the take and has no run
  that fits nearer the base. It proves that a take positioned at the run's high end retains the tier's bytes and
  raises its owner's count alone. It proves that the positions are the persistent tier's reversed.
- `LocusCarcerum/Fragment.lean` proves that the fragmentation reading is at max 100.
- `LocusCarcerum/Bounds.lean` proves that a span the test passes lies inside its cell and that a span it refuses
  does not: the test of a pointer. It also proves that a high-water mark is at or past each count that raised it.
- `LocusCarcerum/Entry.lean` proves that no two contexts name one movable entry, and the bytes a give zeroes.
- `LocusCarcerum/Boundary.lean` writes the free of §3.4 and the Trim of §3.5 on the cells of one tier. The walk
  reads one header at a time and stores the edge after it stops. It proves that a cellblock has no free cell at its edge
  when its word holds no trim bit and no operation is in progress. It proves that the next open pass walks from the
  edge when the word holds a trim bit.
- `LocusCarcerum/Futex.lean` writes the futex contract and the address wait that GenMC runs. It proves that the
  model reaches each state of the contract and no other state. It proves that a wait on the whole word has no state
  that the 32-bit wait of Linux does not have. It also checks that a wake of one waiter leaves a second waiter
  asleep.
- `LocusCarcerum/Pass.lean` proves that no pass works on the cellblock while a writer works alone.
- `LocusCarcerum/Order.lean` carries `Pass.lean` and `Trim.lean` to C11. The writes to the closed word are
  read-modify-writes in one modification order. The two sequentially consistent fences of a pass and a drain give
  the drain its disjunction under the fence rule of the standard. The fences of a free and a writer give the
  free the same disjunction. The free reads the closed bit set, or the walk reads the header free.
- `LocusCarcerum/OwnerWord.lean` writes the owner word as its fields: the owner's place, the completion bit, the
  signaling bits and the owner's sequence. It proves that the fields do not overlap and fill the word at 64 and 32
  bits. It proves that a word built from a place and a sequence gives both back. It also proves that an undo of one
  owner's takes after a sequence point gives back those takes alone and commutes with another owner's undo.
- `LocusCarcerum/Sleep.lean` writes the sleeper bit on the closed word for one writer and one pass. It proves that
  a pass sleeps only on a closed word, and that the open pass wakes each sleep. It also checks that a pass without
  this rule sleeps on an open word with no wake to come.
- `LocusCarcerum/Trim.lean` writes the steps of §3.4 on the closed word for any number of contexts. It proves that
  no tier has a free cell at its edge while the word holds no trim bit and no free operation runs. It
  proves that after the first step of an open pass, each free cell at an edge has a free operation in progress. It also checks one interleaving of a protocol without trim bits, where a free cell stays at the edge.
- `LocusCarcerum/Word.lean` proves the word arithmetic of §3. It proves that a bump up does not carry under the
  limits. It proves that the borrow of a bump down does not change the low half. It also proves the limit on bumps
  in progress and the counts for the two widths.
- `LocusCarcerum/Word.lean` also proves the request guard of §3. Without the guard, the largest request rounds to
  0 bytes. With the guard, the round-up does not wrap, the cell is not smaller than the request, and $h + p$ does
  not wrap.
- `LocusCarcerum/Word.lean` also proves the limit on bumps down in progress. A top that wraps below 0 is more than
  $S$, and §3.2 rejects it. It proves this limit for 63 lanes at $H = 32$ and for 31 lanes at $H = 16$.
- `LocusCarcerum/Ticket.lean` states the cellblock over the ticket ring of embedded_types. The completion bit of
  the owner word is a transition of the flag matrix, which lands once and retains every other field. A sequence of
  gives on one cell is a sequence of claims on one slot, and the bytes come off once at max. The sequence of
  an owner is a ticket: two takes fewer than $2^b$ apart have sequences of their own. A roll at 32 bits retains the
  field, and the claim bit of a movable entry is the push of a slot. The release of a movable cell inside a pass
  is a claim that the drain of a move waits for.
- `LocusCarcerum/Guard.lean` states the access method: the guard passes a token of the caller to its record, and
  the record decides if a call is legal. One CAS commits the call. A claim tests `~word & mask == mask` on the
  atomic word of the host, and two claims that commit hold no bit in common. `vec[n]` decays to
  `base + n * stride`, and the span test passes it only below the count. So a legal span stays in its cell and
  stops before the neighbor, and the spans of two elements do not overlap.
- `LocusCarcerum/Linearizable.lean` runs the C order of a call: a read of the claim word, the claim CAS, the work on
  the cell, and the release CAS. It runs any count of threads on any schedule, with no bound on the count. A claim
  that does not commit changes nothing, and no two threads hold one element. While a thread holds its element, no
  other thread changes it. A reader gives the value of the one step model at its claim, and with no claim held the
  cells are that model.
- `LocusCarcerum/Linearizable.lean` also proves the run linearizable, as Section 2.2 of Herlihy and Wing,
  _Linearizability: A Correctness Condition for Concurrent Objects_, ACM TOPLAS 12(3), 463–492, 1990,
  `doi:10.1145/78969.78972`, defines it. The order is the log of the claims. L1 is `linearizable`: the model is the calls of the log
  replayed, and a reader gives the replay before it. L2 is `realtime`: a call that ended before another claimed comes
  first in the log. `EndToEnd.linearizable` carries both conditions end to end.
- `LocusCarcerum/ConcurrentVector.lean` runs the calls of a vector on any count of threads. The record holds the
  count and a claim bit for each element. `at` and `set` claim their element, and then read the count, which is the
  span test. `push_back` claims the element at the count and commits with one CAS of the count, and `pop_back`
  claims the last element and commits the same way. The log is a legal history of the vector with one lock, and the
  order keeps real time.
- `LocusCarcerum/MPMCQueue.lean` ports to Lean 4 the Coq proof of Folly's MPMC queue. The source is Vindum, Frumin
  and Birkedal, _Mechanized Verification of a Fine-Grained Concurrent Queue from Meta's Folly Library_, CPP 2022,
  100–115, `doi:10.1145/3497775.3503689`. It keeps the algorithm, the points and the invariant of that proof. An
  enqueue takes effect at its FAA, and a dequeue that came first takes effect right after the FAA of its enqueue.
  The log is a legal history of the queue with one lock. Each dequeue gives the value of its ticket, and the order
  keeps real time.
- `LocusCarcerum/Hypotheses.lean` tests each hypothesis of these files. A fact of the build, such as an alignment,
  holds at each width that `embed_types.h` selects, as the C formula gives it. For each composite theorem, a state
  that the steps reach meets all of its hypotheses at once. So no theorem holds with no state to hold for.
- `LocusCarcerum/EndToEnd.lean` runs those calls on the integrated model of embedded_types. Workers grant and push
  slots, pop them and do each call inside a pass, each step one CAS, at any count of workers. A call whose job
  pointer fails its check never runs, and one worker at max runs each call. A drain that
  read the lane of a call in progress, and reads it over, finds the call done or its worker freed. Each lane step is
  a run of the futex model of embedded_types, so each sleeper on the lane gets its wake.
  `two_lanes_reorder` gives a run on two lanes where a later slot lands first.
- `LocusCarcerum/Vector.lean` states a vector behind the guard, with its elements in one movable cell. Each call
  leaves the elements that `std::vector` leaves. A change of one element and a growth both go through the stack.
  No operation carries the bytes of one cell into another, and no call gives back an identity or an address. Two
  sound guards with the same view through each token give the same answers to each run of calls. The mover and a
  relocation change no view.
- `LocusCarcerum/Vector.lean` also bounds the vector by its pool. The ceiling holds the two cells of a growth in the
  pool together, and a `push_back` at the ceiling changes nothing. The alignment of the pool bounds the alignment
  of each element. Odd persistent slabs leave holes that a later take does not fill. A temporary cell held past its
  call leaves a hole, and a give in stack order leaves none.
- `LocusCarcerum/Vector.lean` also fills the holes. Behind the guard, the persistent and temporary tiers give out
  tokens and not pointers. So the fill puts the busy cells end to end with no hole, and it changes no view through
  a token. Two layouts with the same pointers have the same holes, and so a tier that gives out pointers keeps
  each hole it has.
