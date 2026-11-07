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

Thus $E$ is never more than $2^{H-1} + L\,(h + 2^{H-8})$, where $L$ is the number of bumps in progress at the same
time. $E$ does not carry into the half of $T$ while $2^{H-1} + L\,(h + 2^{H-8}) < 2^H$. This permits up to 127 bumps
in progress at $H = 32$, and up to 120 at $H = 16$.

A bump down subtracts $n \cdot 2^H$. It does not change the half of $E$. A borrow out of the half of $T$ goes out of
the word.

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

Each exclusive operation starts with Settle. The exclusive operations are spill, trim, growth, rewind and sort.
While an exclusive operation runs, no operation of §3.1 or §3.2 is in progress, and no such operation starts.

### 3.4 Free, cell $(o,p) \in P \cup Q$

The operation sets the state bit of the cell to free. $P$, $Q$, $E$ and $T$ do not change. If a free cell is at the
edge of $e_P$ or at the edge of $t_Q$, a Trim follows.

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

## 4. Lemmas

A **round** is a maximum interval between two exclusive operations. In a round, only the operations of §3.1, §3.2
and §3.4 run.

**Lemma 1** (monotone in a round). In a round, $E$ does not decrease and $T$ does not increase.

*Proof.* §3.1 adds $n > 0$ to $E$. §3.2 subtracts $n > 0$ from $T$. §3.4 does not change $E$ or $T$. No other
operation of the round changes them. $\square$

**Lemma 2** (a rejection is absorbing). In a round, a bump up $B_1$ is rejected. Then each bump up $B_2$ whose
read-modify-write follows the read-modify-write of $B_1$ is also rejected.

*Proof.* If $B_1$ is rejected and does not wrap, then $w_1 + n_1 > T_1$. $T_1$ is the value of $T$ that $B_1$
read. From Lemma 1, $w_2 \ge w_1 + n_1 > T_1 \ge T_2$.

$T_1 \ge T_2$ is true because $B_2$ reads $T$ in the same atomic step as its read-modify-write. This
read-modify-write follows the read-modify-write of $B_1$. Thus $w_2 + n_2 > T_2$, and $\tau_2$ is false.

If $B_1$ is rejected because it wraps, then $w_1 + n_1 < w_1$, and $E$ is past $2^{64} - n_1$. Then $w_2 \ge w_1 +
n_1$ modulo $2^{64}$ has one of these two results:

- It wraps, and the second conjunct rejects it.
- It is more than $S$. Because $T_2 \le S$, the first conjunct rejects it. $\square$

The mirror statement is true for bumps down, with $E$ in place of $T$.

All of Lemma 2 is the step $T_1 \ge T_2$. This step is true only if the first line of §3.1 is one atomic step on $E$
and $T$ together. Thus one word must contain the two boundaries. §6.2 shows the result with two words.

**Lemma 3** (accepted bumps are contiguous). In a round, $B_1, \dots, B_k$ are the accepted bumps up, in
read-modify-write order. Then $o_1 = e_P$ at the start of the round, and $o_{i+1} = o_i + n_i$.

*Proof.* From Lemma 2, each accepted bump is before each rejected bump in read-modify-write order. Thus nothing
changes $E$ between two consecutive accepted bumps, and $w_{i+1} = w_i + n_i$. The first accepted bump reads the
value of $E$ at the end of the previous round. Because $R$ is true at the start of the round, this value is $e_P$.
$\square$

**Lemma 4** ($I$ stays true in a round). $R(\sigma)$ is true at the start of a round. Then $I(\sigma)$ is true for
all of the round, after the two atomic operations of each bump are complete.

*Proof.* From Lemma 3, the accepted bumps extend $P$ without gaps. Thus $e_P$ increases by exactly the needs of the
accepted bumps. $E$ increases by each need, accepted or rejected. $O_E$ increases by each rejected need. Thus
$E - O_E = e_P$. The mirror is true for $T$.

For $e_P \le t_Q$: an accepted bump up has $w + n \le T \le t_Q$. Thus $e_P \le t_Q$ after it. The mirror is true for
a bump down. $\square$

**Lemma 5** (Settle puts the state at rest). If $I(\sigma)$ is true and no bump is in progress, Settle causes
$R(\sigma)$.

*Proof.* From $I$, $E - O_E = e_P$ and $T + O_T = t_Q$. Settle writes these values and sets the overruns to zero.
$\square$

## 5. Theorem

**Theorem.** Start at $\sigma_0$, and interleave §3.1, §3.2 and §3.4 in any sequence between exclusive operations.
Then these two statements are true:

1. Each exclusive operation finds $R(\sigma)$ after its Settle.
2. Each returned address $o + 8$ is in a cell of $P \cup Q$, and this cell does not overlap other cells.

*Proof.* $R(\sigma_0)$ is true. Use induction on rounds. If $R$ is true at the start of a round, $I$ is true for all
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

$X$ is accepted at $o = 6112$, but $e_P = 6088$. The chain has a gap, and the cell of $X$ is past $E$.

### 6.2 Two words

**Lemma 6** (two words are not absorbing). $E$ and $T$ are two atomic words. Thus a bump is two steps: first the
read-modify-write $w := E,\ E := E + n$, and later the read of $T$ that decides $\tau$.

Then Lemma 2 fails. A bump $A$ can be rejected while a bump $B$ is accepted, although the read-modify-write of $A$ is
before the read-modify-write of $B$. After the round, $I$ is false.

*Proof.* The example is an interleaving with $S = 2^{20}$. The round starts at rest with
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
$B$. Settle subtracts them from the top, and $E$ goes into the gap below a cell that is in use. The next bump up is
accepted over the cell of $B$. $\square$

Thus the two boundaries must be one word. The first line of §3.1 and the first line of §3.2 must each be one
read-modify-write on this word. $E$ is the low half of the word, and $T$ is the high half. A bump up adds $n$ to the
low half and reads the high half in the same step. Because of the order of the word, $T_1 \ge T_2$ is true.

Lean checks the two models in `theory/locus_carcerum`:

- `LocusCarcerum/Packed.lean` proves Lemmas 1 to 5 and the theorem for one word.
- `LocusCarcerum/Split.lean` writes the two-word steps and checks the example above.
- `LocusCarcerum/Word.lean` proves the word arithmetic of §3. It proves that a bump up does not carry under the
  limits. It proves that the borrow of a bump down does not change the low half. It also proves the limit on bumps
  in progress and the counts for the two widths.
