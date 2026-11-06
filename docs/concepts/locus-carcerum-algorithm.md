# LocusCarcerum

## 1. Definitions

Let $S \in \mathbb{N}$, $8 \mid S$. Let $h = 16$.

A **cell** is a pair $(o, p) \in \mathbb{N}^2$ with $8 \mid o$, $8 \mid p$, occupying $[o,\ o + h + p)$.

A **chain from $a$ to $b$** is a finite sequence of cells $c_1, \dots, c_k$ with
$o_1 = a$, $o_{i+1} = o_i + h + p_i$, $o_k + h + p_k = b$. The empty chain is a chain from $a$ to $a$.

**State** $\sigma = (E, T, O_E, O_T, P, Q)$ where

- $E \in \mathbb{N}$, the persistent end
- $T \in \mathbb{N}$, the temporary top
- $O_E, O_T \in \mathbb{N}$, the overruns
- $P$, a chain from $0$ to some $e_P$, the persistent cells
- $Q$, a chain from some $t_Q$ to $S$, the temporary cells

Initial state $\sigma_0 = (0, S, 0, 0, \varnothing, \varnothing)$.

## 2. Invariant

$$
I(\sigma) \;\equiv\; E = e_P + O_E \;\wedge\; T = t_Q - O_T \;\wedge\; e_P \le t_Q
$$

**Rest** $R(\sigma) \equiv I(\sigma) \wedge O_E = 0 \wedge O_T = 0$, i.e. $E = e_P$ and $T = t_Q$.

## 3. Operations

Each operation is a function $\Sigma \to \Sigma \times \{\bot\} \cup \mathbb{N}$. Operations of §3.1 and §3.2 may
interleave with one another; operations of §3.3 execute with no other operation in progress.

### 3.1 Bump up, need $n$ ($n = h + p$, $8 \mid p$)

$$
w := E,\quad E := E + n
$$
$$
\tau \equiv (w + n \le T) \wedge (w + n \ge w) \wedge (T \le S)
$$
$$
\tau \Rightarrow P := P \cdot (w, p),\ \text{answer } w + 8
$$
$$
\neg\tau \Rightarrow O_E := O_E + n,\ \text{answer } \bot
$$

The first line is one atomic read-modify-write. The fourth is one atomic add. Nothing subtracts from $E$.

### 3.2 Bump down, need $n$

$$
w := T,\quad T := T - n
$$
$$
\tau \equiv (w - n \ge E) \wedge (w - n \le w) \wedge (w - n \le S)
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

Every exclusive operation (spill, trim, growth, rewind, sort) begins with Settle, and while it runs no §3.1 or
§3.2 operation is in progress and none begins.

### 3.4 Free, cell $(o,p) \in P \cup Q$

The cell's state bit is set free. $P$, $Q$, $E$, $T$ are unchanged. A free cell at $e_P$'s edge or $t_Q$'s edge is
followed by Trim.

### 3.5 Trim (exclusive)

Settle. Then $P$ is shortened by its longest free suffix, $E := e_P$; $Q$ is shortened by its longest free prefix,
$T := t_Q$.

### 3.6 Spill up, need $n$ (exclusive)

Settle. Consecutive free cells of $P$ are merged. If a free cell $(o, p') \in P$ with $p' \ge p$ exists, the
first such is replaced by $(o, p)$ and, where $p' - p \ge h + 8$, a free $(o + h + p,\ p' - p - h)$; answer
$o + 8$. Else Trim; if $T - E \ge n$ then $P := P \cdot (E, p)$, $E := E + n$, answer $E_{\text{old}} + 8$; else
$\bot$.

### 3.7 Spill down, need $n$ (exclusive)

Settle. Trim. If $T - E \ge n$ then $Q := (T - n, p) \cdot Q$, $T := T - n$, answer $T + 8$; else $\bot$.

## 4. Lemmas

Let a **round** be a maximal interval between two exclusive operations. Within a round only §3.1, §3.2 and §3.4
operate.

**Lemma 1** (monotone within a round). Within a round, $E$ is non-decreasing and $T$ is non-increasing.

*Proof.* §3.1 adds $n > 0$ to $E$; §3.2 subtracts $n > 0$ from $T$; §3.4 changes neither; no operation of the round
does otherwise. $\square$

**Lemma 2** (refusal is absorbing). Within a round, if a bump up $B_1$ is refused, every bump up $B_2$ whose
read-modify-write follows $B_1$'s is refused.

*Proof.* $B_1$ refused and not wrapped means $w_1 + n_1 > T_1$ where $T_1$ is $T$ as $B_1$ read it. By Lemma 1,
$w_2 \ge w_1 + n_1 > T_1 \ge T_2$. Hence $w_2 + n_2 > T_2$ and $\tau_2$ is false. If $B_1$ was refused for a wrap,
$w_1 + n_1 < w_1$, then $E$ has passed $2^{64} - n_1$, and $w_2 \ge w_1 + n_1$ modulo $2^{64}$ is either wrapped,
refused by the second conjunct, or exceeds $S$ since $T_2 \le S$, refused by the first. $\square$

The mirror holds for bumps down with $E$ in place of $T$.

**Lemma 3** (taken bumps are contiguous). Within a round, let $B_1, \dots, B_k$ be the taken bumps up in
read-modify-write order. Then $o_1 = e_P$ at the round's start and $o_{i+1} = o_i + n_i$.

*Proof.* By Lemma 2 every taken bump precedes every refused bump in read-modify-write order. Hence between
consecutive taken bumps nothing changes $E$, and $w_{i+1} = w_i + n_i$. The first taken bump reads $E$ as the
round left it, which is $e_P$ by $R$ at the round's start. $\square$

**Lemma 4** ($I$ is preserved by a round). If $R(\sigma)$ at a round's start, then $I(\sigma)$ throughout the round
once every bump's two atomic operations have completed.

*Proof.* By Lemma 3 the taken bumps extend $P$ contiguously. Hence $e_P$ advances by exactly the taken needs. $E$
advances by every need, taken and refused; $O_E$ advances by every refused need. Hence $E - O_E = e_P$. Mirror
for $T$. $e_P \le t_Q$: a taken bump up has $w + n \le T \le t_Q$. Hence $e_P \le t_Q$ after it; mirror for down. $\square$

**Lemma 5** (Settle restores rest). If $I(\sigma)$ and no bump is in progress, Settle yields $R(\sigma)$.

*Proof.* $E - O_E = e_P$ and $T + O_T = t_Q$ by $I$; Settle stores these and zeroes the overruns. $\square$

## 5. Theorem

**Theorem.** From $\sigma_0$, under any interleaving of §3.1, §3.2 and §3.4 between exclusive operations, every
exclusive operation observes $R(\sigma)$ after its Settle, and every answered address $o + 8$ lies in a cell of
$P \cup Q$ disjoint from every other.

*Proof.* $R(\sigma_0)$. By induction on rounds: $R$ at a round's start gives $I$ throughout (Lemma 4), and the
round's closing exclusive operation sees no bump in progress and Settles to $R$ (Lemma 5). Each exclusive
operation of §3.5 to §3.7 maps a chain to a chain and leaves $E = e_P$, $T = t_Q$, $e_P \le t_Q$. Hence $R$ at the next
round's start. Disjointness: taken cells are consecutive in a chain (Lemma 3) and $e_P \le t_Q$ separates $P$
from $Q$. $\square$

## 6. The undo

Replace §3.1's refused branch by $E := E - n$. Then $E$ is not monotone within a round and Lemma 2 fails:

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

$X$ is taken at $o = 6112$ with $e_P = 6088$: the chain is broken, and $X$'s cell lies past $E$.
