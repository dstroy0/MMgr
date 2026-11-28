/-!
# LocusCarcerum, a pool of many banks

A pool past the reach of one boundary half is a run of banks of `D` bytes each, and bank `k` starts at `k * D`. The
bank count is the banks the pool's bytes take. The bank of a tier is the one its position counts from: its frame.
The pool is one run of bytes, and the banks are the bases that its offsets count from.

The closed word holds the bank count and the frame of each tier, above its four flag bits. The boundary word holds
the position of each tier from its frame's base. A position can reach `W`, the reach of a half, which is two banks.
So a tier crosses into the next bank with no step of the writer.

A pass reads the closed word once for its permit, and so it reads the frames with no other load. Only a writer
changes the frames. It holds the closed bit and drains the passes first, and so each pass reads the same frames for
its whole life. The writer's fetch-or of the closed bit reads the frames, and the writer holds them until it opens.
So the writer reads no other load of the word for them.

A writer that stores a boundary picks its frame: the bank of the persistent end, and the bank below the bank of the
temporary top. So each position leaves at least `D` bytes of room before a bump refuses it. The writer clamps each
frame at two banks below the bank count, and so a pool of two banks or less never changes a frame. No cell meets the
edge of a bank, and so the walks, the trims and the splits count in offsets of the pool.

A reader outside a pass reads the frames and the positions under an epoch that the writer moves around each change.
A give reads neither: its cell's header holds its tier, and its test of the boundary compares offsets modulo `D`.
That test can hold for a cell away from the boundary, and `Boundary.Step.readAny` is that step.

The banks are not the thread regions of `Regions`.

The theorems: the fields of the closed word decode to what the encode put in. A flag operation leaves them as they
are, and one fetch-add moves them to new frames. A pass in flight and a writer with the bit each hold the frames
that the word holds.

The invariant of `Packed` holds through every bump, every settle and every change of frame. The writer's frames leave
`D` bytes of room and fit their fields. The clamped frames keep the invariant, and a pool of two banks or less keeps
them at bank 0. A reader whose two epochs agree reads one pair. The test modulo `D` holds for each cell at the
boundary.
-/

namespace LocusCarcerum.Banks

/-! ## The fields of the closed word -/

/-- The values of the four flag bits: the closed bit, the sleeper bit and the two trim bits. -/
def F : Nat := 2 ^ 4

/-- The closed word with the flags `f`, the bank count `n`, the persistent bank `p` and the temporary bank `t`. Each
field has `K` values. -/
def encode (K f n p t : Nat) : Nat := f + F * (n + K * (p + K * t))

/-- The flag bits of a word. -/
def flags (w : Nat) : Nat := w % F

/-- The bits of a word above its flags. -/
def fields (w : Nat) : Nat := w / F

/-- The bank count. -/
def count (K w : Nat) : Nat := fields w % K

/-- The bank of the persistent tier. -/
def persistent (K w : Nat) : Nat := fields w / K % K

/-- The bank of the temporary tier. -/
def temporary (K w : Nat) : Nat := fields w / K / K

theorem F_pos : 0 < F := by decide

theorem flags_encode (K f n p t : Nat) (hf : f < F) : flags (encode K f n p t) = f := by
  unfold flags encode
  rw [Nat.add_mul_mod_self_left, Nat.mod_eq_of_lt hf]

theorem fields_encode (K f n p t : Nat) (hf : f < F) : fields (encode K f n p t) = n + K * (p + K * t) := by
  unfold fields encode
  rw [Nat.add_mul_div_left _ _ F_pos, Nat.div_eq_of_lt hf, Nat.zero_add]

theorem count_encode (K f n p t : Nat) (hf : f < F) (hn : n < K) : count K (encode K f n p t) = n := by
  unfold count
  rw [fields_encode K f n p t hf, Nat.add_mul_mod_self_left, Nat.mod_eq_of_lt hn]

theorem persistent_encode (K f n p t : Nat) (hf : f < F) (hn : n < K) (hp : p < K) :
    persistent K (encode K f n p t) = p := by
  have hK : 0 < K := Nat.lt_of_le_of_lt (Nat.zero_le n) hn
  unfold persistent
  rw [fields_encode K f n p t hf, Nat.add_mul_div_left _ _ hK, Nat.div_eq_of_lt hn, Nat.zero_add,
    Nat.add_mul_mod_self_left, Nat.mod_eq_of_lt hp]

theorem temporary_encode (K f n p t : Nat) (hf : f < F) (hn : n < K) (hp : p < K) :
    temporary K (encode K f n p t) = t := by
  have hK : 0 < K := Nat.lt_of_le_of_lt (Nat.zero_le n) hn
  unfold temporary
  rw [fields_encode K f n p t hf, Nat.add_mul_div_left _ _ hK, Nat.div_eq_of_lt hn, Nat.zero_add,
    Nat.add_mul_div_left _ _ hK, Nat.div_eq_of_lt hp, Nat.zero_add]

/-- The bits of `fields w` are the bits of `w` from bit 4 up. -/
theorem testBit_fields (w i : Nat) : (fields w).testBit i = w.testBit (4 + i) := by
  unfold fields F
  rw [← Nat.shiftRight_eq_div_pow, Nat.testBit_shiftRight]

/-- A fetch-or of flag bits leaves every field as it is. -/
theorem fields_or (w m : Nat) (hm : m < F) : fields (w ||| m) = fields w := by
  apply Nat.eq_of_testBit_eq
  intro i
  rw [testBit_fields, testBit_fields, Nat.testBit_or]
  have hlt : m < 2 ^ (4 + i) := Nat.lt_of_lt_of_le hm (Nat.pow_le_pow_right (by decide) (by omega))
  rw [Nat.testBit_lt_two_pow hlt, Bool.or_false]

/-- A fetch-and on a word of `W` bits, with a mask that holds every bit above the flags, leaves every field as it
is. The C clears a flag with such a mask. -/
theorem fields_and (W w m : Nat) (hw : w < 2 ^ W) (hm : ∀ i, 4 ≤ i → i < W → m.testBit i = true) :
    fields (w &&& m) = fields w := by
  apply Nat.eq_of_testBit_eq
  intro i
  rw [testBit_fields, testBit_fields, Nat.testBit_and]
  by_cases hi : 4 + i < W
  · rw [hm (4 + i) (by omega) hi, Bool.and_true]
  · have hlt : w < 2 ^ (4 + i) := Nat.lt_of_lt_of_le hw (Nat.pow_le_pow_right (by decide) (by omega))
    rw [Nat.testBit_lt_two_pow hlt, Bool.false_and]

/-- A word with each field below `K` fits in `F * K ^ 3` values. -/
theorem encode_lt (K f n p t : Nat) (hf : f < F) (hn : n < K) (hp : p < K) (ht : t < K) :
    encode K f n p t < F * (K * (K * K)) := by
  unfold encode
  have h1 : p + K * t < K * K := by
    have : K * t + K ≤ K * K := by rw [← Nat.mul_succ]; exact Nat.mul_le_mul_left K ht
    omega
  have h2 : n + K * (p + K * t) < K * (K * K) := by
    have : K * (p + K * t) + K ≤ K * (K * K) := by rw [← Nat.mul_succ]; exact Nat.mul_le_mul_left K h1
    omega
  have : F * (n + K * (p + K * t)) + F ≤ F * (K * (K * K)) := by
    rw [← Nat.mul_succ]; exact Nat.mul_le_mul_left F h2
  omega

/-- A word modulo `F * G` keeps its flags and its fields modulo `G`. -/
theorem mod_fields (f x G : Nat) (hf : f < F) (hG : 0 < G) : (f + F * x) % (F * G) = f + F * (x % G) := by
  have hx : F * x = F * (x % G) + F * G * (x / G) := by
    calc F * x = F * (x % G + G * (x / G)) := by rw [Nat.mod_add_div x G]
      _ = F * (x % G) + F * G * (x / G) := by rw [Nat.mul_add, Nat.mul_assoc]
  have hlt : f + F * (x % G) < F * G := by
    have : F * (x % G) + F ≤ F * G := by
      rw [← Nat.mul_succ]; exact Nat.mul_le_mul_left F (Nat.mod_lt x hG)
    omega
  rw [hx, ← Nat.add_assoc, Nat.add_mul_mod_self_left, Nat.mod_eq_of_lt hlt]

/-- The writer moves the fields from `a` to `b` with one fetch-add of their difference modulo the `G` values the
fields have. The word wraps modulo `F * G`. The flags stay as they are, and so a fetch-or of a give that lands
before or after the add keeps its bit. -/
theorem fields_add (G f a b : Nat) (hf : f < F) (ha : a < G) (hb : b < G) :
    (f + F * a + F * ((b + G - a) % G)) % (F * G) = f + F * b := by
  have hG : 0 < G := Nat.lt_of_le_of_lt (Nat.zero_le a) ha
  have hsum : (a + (b + G - a) % G) % G = b := by
    rw [Nat.add_mod_mod, show a + (b + G - a) = b + G by omega, Nat.add_mod_right, Nat.mod_eq_of_lt hb]
  rw [Nat.add_assoc, ← Nat.mul_add, mod_fields f _ G hf hG, hsum]

/-- A word of `W` bits is `F` flag values times the `2 ^ (W - 4)` values of its fields. -/
theorem word_split (W : Nat) (hW : 4 ≤ W) : F * 2 ^ (W - 4) = 2 ^ W := by
  unfold F
  rw [← Nat.pow_add]
  congr 1
  omega

/-- A 64-bit closed word: four flag bits and three fields of 20 bits each. -/
theorem layout_64 : F * (2 ^ 20 * (2 ^ 20 * 2 ^ 20)) ≤ 2 ^ 64 := by decide

/-- A 32-bit closed word: four flag bits and three fields of 9 bits each. -/
theorem layout_32 : F * (2 ^ 9 * (2 ^ 9 * 2 ^ 9)) ≤ 2 ^ 32 := by decide

/-! ## A pass reads the frames once

A pass badges in, then reads the closed word once. Where the closed bit is clear, the pass works with the fields it
read, and else it leaves with no work. The writer takes the closed bit with a fetch-or that reads the fields. It
drains the passes that work, changes the fields and opens. One writer at a time: `Boundary` proves it. -/

/-- Where a pass is: at rest, badged in, or at work with the fields it read. -/
inductive Pass where
  | idle
  | entered
  | working (read : Nat)
  deriving DecidableEq, Repr

/-- Where the writer is: at rest, with the closed bit in hand, or after its drain. A writer with the bit holds the
fields that its fetch-or of the bit read. -/
inductive Writer where
  | idle
  | closed (held : Nat)
  | drained (held : Nat)
  deriving DecidableEq, Repr

/-- The writer, the fields of the closed word and each pass. -/
structure Gate where
  writer : Writer
  frames : Nat
  pass : Nat → Pass

/-- A pass's place changed. -/
def moved (pass : Nat → Pass) (i : Nat) (q : Pass) : Nat → Pass := fun j => if j = i then q else pass j

/-- The steps, each one access to the closed word or to a lane. -/
inductive GateStep : Gate → Gate → Prop where
  /-- A pass badges in at its lane. -/
  | enter {g : Gate} (i : Nat) (h : g.pass i = .idle) : GateStep g { g with pass := moved g.pass i .entered }
  /-- The pass reads the closed word once: at work with the fields where the closed bit is clear, else at rest. -/
  | read {g : Gate} (i : Nat) (h : g.pass i = .entered) :
      GateStep g { g with pass := moved g.pass i (if g.writer = .idle then .working g.frames else .idle) }
  /-- The pass ends and leaves its lane. -/
  | leave {g : Gate} (i r : Nat) (h : g.pass i = .working r) : GateStep g { g with pass := moved g.pass i .idle }
  /-- The writer takes the closed bit with one fetch-or, which reads the fields. -/
  | close {g : Gate} (h : g.writer = .idle) : GateStep g { g with writer := .closed g.frames }
  /-- The drain ends where no pass is at work. -/
  | drain {g : Gate} (r : Nat) (h : g.writer = .closed r) (hn : ∀ i q, g.pass i ≠ .working q) :
      GateStep g { g with writer := .drained r }
  /-- The writer changes the fields with one fetch-add, and holds the fields that it wrote. -/
  | change {g : Gate} (r₀ : Nat) (h : g.writer = .drained r₀) (r : Nat) :
      GateStep g { g with frames := r, writer := .drained r }
  /-- The writer clears the closed bit. -/
  | reopen {g : Gate} (r : Nat) (h : g.writer = .drained r) : GateStep g { g with writer := .idle }

/-- The states the steps reach from a gate at rest. -/
inductive GateReach : Gate → Prop where
  | start (r : Nat) : GateReach ⟨.idle, r, fun _ => .idle⟩
  | step {g g' : Gate} : GateReach g → GateStep g g' → GateReach g'

/-- The invariant. Each pass at work holds the fields of the word, and no pass is at work after the drain. A writer
with the bit holds the fields of the word. -/
structure GateInv (g : Gate) : Prop where
  current : ∀ i r, g.pass i = .working r → r = g.frames
  quiet : ∀ r, g.writer = .drained r → ∀ i q, g.pass i ≠ .working q
  held : ∀ r, (g.writer = .closed r ∨ g.writer = .drained r) → r = g.frames

theorem gateInv_step {g g' : Gate} (hg : GateInv g) (st : GateStep g g') : GateInv g' := by
  cases st with
  | enter i h =>
    refine ⟨?_, ?_, hg.held⟩
    · intro j r hj
      by_cases hji : j = i
      · subst hji; simp [moved] at hj
      · exact hg.current j r (by simpa [moved, hji] using hj)
    · intro r hd j q hj
      by_cases hji : j = i
      · subst hji; simp [moved] at hj
      · exact hg.quiet r hd j q (by simpa [moved, hji] using hj)
  | read i h =>
    refine ⟨?_, ?_, hg.held⟩
    · intro j r hj
      by_cases hji : j = i
      · subst hji
        simp only [moved, ite_true] at hj
        split at hj
        · cases hj; rfl
        · cases hj
      · exact hg.current j r (by simpa [moved, hji] using hj)
    · intro r hd j q hj
      by_cases hji : j = i
      · subst hji
        simp only [moved, ite_true] at hj
        split at hj
        · next hw => simp only at hd; rw [hw] at hd; cases hd
        · cases hj
      · exact hg.quiet r hd j q (by simpa [moved, hji] using hj)
  | leave i r' h =>
    refine ⟨?_, ?_, hg.held⟩
    · intro j r hj
      by_cases hji : j = i
      · subst hji; simp [moved] at hj
      · exact hg.current j r (by simpa [moved, hji] using hj)
    · intro r hd j q hj
      by_cases hji : j = i
      · subst hji; simp [moved] at hj
      · exact hg.quiet r hd j q (by simpa [moved, hji] using hj)
  | close h =>
    refine ⟨hg.current, fun r hd => by simp at hd, ?_⟩
    intro r hr
    simp at hr ⊢ <;> omega
  | drain r₀ h hn =>
    refine ⟨hg.current, fun _ _ => hn, ?_⟩
    intro r hr
    simp at hr ⊢
    rw [← hr]
    exact hg.held r₀ (Or.inl h)
  | change r₀ h r =>
    refine ⟨?_, fun _ _ => hg.quiet r₀ h, ?_⟩
    · intro j r' hj
      exact (hg.quiet r₀ h j r' hj).elim
    · intro r' hr
      simp at hr ⊢ <;> omega
  | reopen r₀ h =>
    exact ⟨hg.current, fun _ hd => by simp at hd, fun _ hr => by simp at hr⟩

theorem gateInv_reach {g : Gate} (h : GateReach g) : GateInv g := by
  induction h with
  | start r => exact ⟨fun _ _ hj => by simp at hj, fun _ hd => by simp at hd, fun _ hr => by simp at hr⟩
  | step _ st ih => exact gateInv_step ih st

/-- A pass at work holds the fields that the closed word holds. It reads the frames once, and that read stays true
until it leaves. -/
theorem pass_reads_current {g : Gate} (h : GateReach g) (i r : Nat) (hi : g.pass i = .working r) :
    r = g.frames :=
  (gateInv_reach h).current i r hi

/-- A writer with the bit holds the fields that the closed word holds. Its fetch-or of the bit reads them, and only
its own fetch-add changes them. So it reads no other load of the word for them. -/
theorem writer_reads_current {g : Gate} (h : GateReach g) (r : Nat)
    (hw : g.writer = .closed r ∨ g.writer = .drained r) : r = g.frames :=
  (gateInv_reach h).held r hw

/-! ## The two tiers in their frames

`Packed`, with each boundary as the base of its frame and a position from that base. `W` is the reach of a
position: the C reads the persistent position ahead of its fetch-add, and a position past `W` moves nothing. A
temporary position below zero is a bump down that wrapped below its frame's base. The C reads that position past
`W`, which refuses every bump that reads it. Positions are integers here, and `Word` proves that the half of the
word agrees with them. -/

/-- The state of a pool. `S` is its bytes, and `bE` and `bT` are the bases of the frames. `E` and `T` are the
boundaries as the frames and the positions name them, and `OE` and `OT` are the overruns. `eP` is the end of the
persistent chain, and `tQ` is the start of the temporary chain. -/
structure Pool where
  S : Int
  bE : Int
  bT : Int
  E : Int
  OE : Int
  eP : Int
  T : Int
  OT : Int
  tQ : Int
  deriving Repr, DecidableEq

section Frames

variable (W : Int)

/-- The invariant. Each boundary is its chain end moved by its overrun, and the chains do not cross. Each frame's
base is at or below its chain end, and the temporary position at rest is within `W`. An overrun in place means
that the tiers crossed or that the temporary position wrapped. -/
def Inv (s : Pool) : Prop :=
  0 ≤ s.bE ∧ s.bE ≤ s.eP ∧ s.E = s.eP + s.OE ∧ 0 ≤ s.OE ∧ s.T + s.OT = s.tQ ∧ 0 ≤ s.OT ∧ s.eP ≤ s.tQ ∧
    s.tQ ≤ s.S ∧ s.bT ≤ s.tQ ∧ s.tQ - s.bT ≤ W ∧
    (0 < s.OE → s.T < s.E ∨ s.T - s.bT < 0) ∧ (0 < s.OT → s.T < s.E ∨ s.T - s.bT < 0)

/-- The pool as it starts: the persistent end at 0, the temporary top at `S`, each frame's base as the writer
picks it. -/
def start (S bE bT : Int) : Pool := ⟨S, bE, bT, 0, 0, 0, S, 0, S⟩

theorem inv_start (S bT : Int) (hS : 0 ≤ S) (hT : bT ≤ S) (hW : S - bT ≤ W) : Inv W (start S 0 bT) := by
  unfold Inv start
  dsimp only
  omega

/-- A bump up of need `n`. A position past `W` moves nothing. Else one fetch-add moves `E`. The cell goes at the old
`E` where the top holds it, the top's position has not wrapped and the top is in the pool. Else the need goes on
the overrun. -/
def bumpUp (n : Int) (s : Pool) : Pool :=
  if W < s.E - s.bE then s
  else if s.E + n ≤ s.T ∧ 0 ≤ s.T - s.bT ∧ s.T ≤ s.S then { s with E := s.E + n, eP := s.E + n }
  else { s with E := s.E + n, OE := s.OE + n }

/-- A bump down of need `n`. One fetch-add moves `T`. The cell goes at the new `T` where the end is below it, its
position has not wrapped and it is in the pool. Else the need goes on the overrun. -/
def bumpDown (n : Int) (s : Pool) : Pool :=
  if s.E ≤ s.T - n ∧ 0 ≤ s.T - s.bT - n ∧ s.T - n ≤ s.S then { s with T := s.T - n, tQ := s.T - n }
  else { s with T := s.T - n, OT := s.OT + n }

/-- The settle of a writer, in positions: each boundary pulled back by its overrun, and the frames as they are. -/
def settle (s : Pool) : Pool := { s with E := s.E - s.OE, T := s.T + s.OT, OE := 0, OT := 0 }

/-- A writer at rest stores both boundaries with frames of its choice. The offsets stay as they are. -/
def reframe (bE bT : Int) (s : Pool) : Pool := { s with bE := bE, bT := bT }

/-- A bump up that the end's position and the top hold reads `E` at the chain's end, with no overrun in place. -/
theorem taken_up (s : Pool) (n : Int) (hn : 0 ≤ n) (hI : Inv W s)
    (hτ : s.E + n ≤ s.T ∧ 0 ≤ s.T - s.bT ∧ s.T ≤ s.S) : s.OE = 0 ∧ s.E = s.eP := by
  unfold Inv at hI
  omega

/-- A bump down that the end and its position hold reads `T` at the chain's start, with no overrun in place. -/
theorem taken_down (s : Pool) (n : Int) (hn : 0 ≤ n) (hI : Inv W s)
    (hτ : s.E ≤ s.T - n ∧ 0 ≤ s.T - s.bT - n ∧ s.T - n ≤ s.S) : s.OT = 0 ∧ s.T = s.tQ := by
  unfold Inv at hI
  omega

theorem inv_bumpUp (s : Pool) (n : Int) (hn : 0 ≤ n) (hI : Inv W s) : Inv W (bumpUp W n s) := by
  unfold bumpUp
  split
  · exact hI
  · split
    all_goals
      unfold Inv at *
      dsimp only
      omega

theorem inv_bumpDown (s : Pool) (n : Int) (hn : 0 ≤ n) (hI : Inv W s) : Inv W (bumpDown n s) := by
  unfold bumpDown
  split
  all_goals
    unfold Inv at *
    dsimp only
    omega

theorem inv_settle (s : Pool) (hI : Inv W s) : Inv W (settle s) := by
  unfold Inv settle at *
  dsimp only
  omega

/-- A writer at rest that picks frames with each base at or below its chain end, and the top's position within
`W`, keeps the invariant. -/
theorem inv_reframe (s : Pool) (bE bT : Int) (hI : Inv W s) (hOE : s.OE = 0) (hOT : s.OT = 0) (hE : 0 ≤ bE)
    (hEP : bE ≤ s.eP) (hT : bT ≤ s.tQ) (hTW : s.tQ - bT ≤ W) : Inv W (reframe bE bT s) := by
  unfold Inv reframe at *
  dsimp only
  omega

/-- The steps of a pool: bumps of a need at or above zero, the settle, and the writer's frames at rest. -/
inductive Run : Pool → Pool → Prop
  | refl (s : Pool) : Run s s
  | up (s s' : Pool) (n : Int) (hn : 0 ≤ n) (h : Run s s') : Run s (bumpUp W n s')
  | down (s s' : Pool) (n : Int) (hn : 0 ≤ n) (h : Run s s') : Run s (bumpDown n s')
  | settle (s s' : Pool) (h : Run s s') : Run s (settle s')
  | reframe (s s' : Pool) (bE bT : Int) (hOE : s'.OE = 0) (hOT : s'.OT = 0) (hE : 0 ≤ bE) (hEP : bE ≤ s'.eP)
      (hT : bT ≤ s'.tQ) (hTW : s'.tQ - bT ≤ W) (h : Run s s') : Run s (reframe bE bT s')

theorem inv_run (s s' : Pool) (hI : Inv W s) (h : Run W s s') : Inv W s' := by
  induction h with
  | refl => exact hI
  | up s2 n hn _ ih => exact inv_bumpUp W s2 n hn ih
  | down s2 n hn _ ih => exact inv_bumpDown W s2 n hn ih
  | settle s2 _ ih => exact inv_settle W s2 ih
  | reframe s2 bE bT hOE hOT hE hEP hT hTW _ ih => exact inv_reframe W s2 bE bT ih hOE hOT hE hEP hT hTW

/-- From the start, every run keeps the persistent chain at or below the temporary chain, and so no persistent
cell meets a temporary one. -/
theorem run_apart (S bT : Int) (hS : 0 ≤ S) (hT : bT ≤ S) (hW : S - bT ≤ W) (s : Pool)
    (h : Run W (start S 0 bT) s) : s.eP ≤ s.tQ := by
  have hI := inv_run W _ s (inv_start W S bT hS hT hW) h
  unfold Inv at hI
  omega

end Frames

/-! ## The frames a writer picks

The writer gives the persistent end `L` its own bank, so its position is below `D`. It gives the temporary top `L`
the bank below its own, so its position is at least `D` where `L` is. Each position is below `2 * D`, which is `W`.
So a bump up of a need within `D` finds its position within `W`. A bump down of a need within `D` finds its
position at or above the need. -/

/-- The frame of the persistent end `L`. -/
def endFrame (D L : Nat) : Nat := L / D

/-- The frame of the temporary top `L`. -/
def topFrame (D L : Nat) : Nat := L / D - 1

theorem endFrame_room (D L : Nat) (hD : 0 < D) : endFrame D L * D ≤ L ∧ L - endFrame D L * D < D := by
  unfold endFrame
  have h := Nat.div_add_mod L D
  have hm := Nat.mod_lt L hD
  rw [Nat.mul_comm] at h
  omega

theorem topFrame_room (D L : Nat) (hD : 0 < D) :
    topFrame D L * D ≤ L ∧ L - topFrame D L * D < 2 * D ∧ (D ≤ L → D ≤ L - topFrame D L * D) := by
  unfold topFrame
  have h := Nat.div_add_mod L D
  have hm := Nat.mod_lt L hD
  rw [Nat.mul_comm] at h
  rw [Nat.sub_one_mul]
  have hq : D ≤ L → 1 ≤ L / D := fun hL => (Nat.le_div_iff_mul_le hD).mpr (by omega)
  by_cases hL : D ≤ L
  · have := hq hL
    have : D ≤ L / D * D := by
      have := Nat.mul_le_mul_right D this
      omega
    omega
  · have h0 : L / D = 0 := Nat.div_eq_of_lt (by omega)
    rw [h0]
    omega

/-- A pool of at most `(G - 1) * D` bytes has each frame below `G`, so each frame fits a field of `G` values. -/
theorem frames_fit (D G S L : Nat) (hD : 0 < D) (hG : 1 ≤ G) (hS : S ≤ (G - 1) * D) (hL : L ≤ S) :
    endFrame D L < G ∧ topFrame D L < G := by
  have hdiv : L / D ≤ G - 1 := by
    have h1 : L / D ≤ S / D := Nat.div_le_div_right hL
    have h2 : S / D ≤ (G - 1) * D / D := Nat.div_le_div_right hS
    rw [Nat.mul_div_cancel _ hD] at h2
    omega
  unfold endFrame topFrame
  omega

/-! ## The frames a pool of `K` banks allows

The placement of the pool sets the bank count `K`, and nothing changes it after. No frame needs a base above bank
`K - 2`. A position from that
base reaches `2 * D`, which is the end of the pool or past it. The writer clamps each frame at `K - 2`. So a pool
of two banks or less keeps both frames at bank 0, and its writer never changes them. -/

/-- The highest frame of a pool of `K` banks. -/
def frameCap (K : Nat) : Nat := K - 2

/-- The frame of the persistent end `L` in a pool of `K` banks. -/
def endFrameIn (D K L : Nat) : Nat := min (endFrame D L) (frameCap K)

/-- The frame of the temporary top `L` in a pool of `K` banks. -/
def topFrameIn (D K L : Nat) : Nat := min (topFrame D L) (frameCap K)

/-- A frame at or below `L` with its position within `2 * D` keeps both facts when the clamp lowers it, in a pool of
at most `K * D` bytes. -/
theorem clamp_room (D K S L g : Nat) (hS : S ≤ K * D) (hL : L ≤ S) (hg : g * D ≤ L ∧ L - g * D ≤ 2 * D) :
    min g (frameCap K) * D ≤ L ∧ L - min g (frameCap K) * D ≤ 2 * D := by
  unfold frameCap
  by_cases h : g ≤ K - 2
  · rw [Nat.min_eq_left h]
    exact hg
  · rw [Nat.min_eq_right (by omega)]
    have hle : (K - 2) * D ≤ g * D := Nat.mul_le_mul_right D (by omega)
    by_cases hK : 2 ≤ K
    · have hsum : (K - 2) * D + 2 * D = K * D := by
        rw [← Nat.add_mul, Nat.sub_add_cancel hK]
      omega
    · have h0 : K - 2 = 0 := by omega
      have hKD : K * D ≤ 2 * D := Nat.mul_le_mul_right D (by omega)
      rw [h0, Nat.zero_mul]
      omega

theorem endFrameIn_room (D K S L : Nat) (hD : 0 < D) (hS : S ≤ K * D) (hL : L ≤ S) :
    endFrameIn D K L * D ≤ L ∧ L - endFrameIn D K L * D ≤ 2 * D := by
  have h := endFrame_room D L hD
  exact clamp_room D K S L _ hS hL ⟨h.1, by omega⟩

theorem topFrameIn_room (D K S L : Nat) (hD : 0 < D) (hS : S ≤ K * D) (hL : L ≤ S) :
    topFrameIn D K L * D ≤ L ∧ L - topFrameIn D K L * D ≤ 2 * D := by
  have h := topFrame_room D L hD
  exact clamp_room D K S L _ hS hL ⟨h.1, by omega⟩

/-- A pool of two banks or less keeps both frames at bank 0. -/
theorem frames_still (D K L : Nat) (hK : K ≤ 2) : endFrameIn D K L = 0 ∧ topFrameIn D K L = 0 := by
  unfold endFrameIn topFrameIn frameCap
  have h0 : K - 2 = 0 := by omega
  rw [h0]
  exact ⟨Nat.min_zero _, Nat.min_zero _⟩

theorem framesIn_fit (D G K S L : Nat) (hD : 0 < D) (hG : 1 ≤ G) (hS : S ≤ (G - 1) * D) (hL : L ≤ S) :
    endFrameIn D K L < G ∧ topFrameIn D K L < G := by
  have h := frames_fit D G S L hD hG hS hL
  unfold endFrameIn topFrameIn
  omega

/-- The clamped frames of a pool at rest, with no overrun in place, keep the invariant of the tiers with `W` at
`2 * D`. The persistent frame is the frame of the chain's end, and the temporary frame the frame of the chain's
start. -/
theorem inv_reframeIn (D K : Nat) (hD : 0 < D) (s : Pool) (hI : Inv (2 * D) s) (hOE : s.OE = 0) (hOT : s.OT = 0)
    (eP tQ S : Nat) (heP : s.eP = eP) (htQ : s.tQ = tQ) (hSS : s.S = S) (hS : S ≤ K * D) :
    Inv (2 * D) (reframe ((endFrameIn D K eP * D : Nat) : Int) ((topFrameIn D K tQ * D : Nat) : Int) s) := by
  have hI' := hI
  unfold Inv at hI'
  have htS : tQ ≤ S := by omega
  have heT : eP ≤ tQ := by omega
  have he := endFrameIn_room D K S eP hD hS (by omega)
  have ht := topFrameIn_room D K S tQ hD hS htS
  generalize endFrameIn D K eP * D = bE at he ⊢
  generalize topFrameIn D K tQ * D = bT at ht ⊢
  apply inv_reframe (2 * D) s _ _ hI hOE hOT
  · omega
  · omega
  · omega
  · omega

/-! ## A reader outside a pass

A reader that is not in a pass reads the epoch, the frame, the position and the epoch again. The writer moves the
epoch once before it changes the frame and once after, and a bump moves the position alone. Where the two epochs
agree and the first is even, the frame the reader read is the frame at its read of the position. So the pair names
the offset at that one instant. -/

/-- Where the reader is: at rest, past its first read of the epoch, or past its read of the frame. Or it is past its
read of the position, with the offset that the frame and the position named at that instant. -/
inductive Reader where
  | idle
  | began (e : Nat)
  | framed (e f : Nat)
  | posed (e f p l : Nat)
  deriving DecidableEq, Repr

/-- The epoch, the frame, the position, the offset they name, and the reader. -/
structure Seq where
  epoch : Nat
  frame : Nat
  pos : Nat
  off : Nat
  reader : Reader

section Epoch

variable (D : Nat)

/-- The steps. The writer changes the frame and the position between its two moves of the epoch, and names the new
offset at its second move. A bump moves the position and the offset together, with the epoch even. -/
inductive SeqStep : Seq → Seq → Prop where
  | open {s : Seq} (h : s.epoch % 2 = 0) : SeqStep s { s with epoch := s.epoch + 1 }
  | setFrame {s : Seq} (f : Nat) (h : s.epoch % 2 = 1) : SeqStep s { s with frame := f }
  | setPos {s : Seq} (p : Nat) (h : s.epoch % 2 = 1) : SeqStep s { s with pos := p }
  | close {s : Seq} (h : s.epoch % 2 = 1) :
      SeqStep s { s with epoch := s.epoch + 1, off := s.frame * D + s.pos }
  | bump {s : Seq} (n : Nat) (h : s.epoch % 2 = 0) : SeqStep s { s with pos := s.pos + n, off := s.off + n }
  | readEpoch {s : Seq} (h : s.reader = .idle) (he : s.epoch % 2 = 0) : SeqStep s { s with reader := .began s.epoch }
  | readFrame {s : Seq} (e : Nat) (h : s.reader = .began e) : SeqStep s { s with reader := .framed e s.frame }
  | readPos {s : Seq} (e f : Nat) (h : s.reader = .framed e f) :
      SeqStep s { s with reader := .posed e f s.pos (s.frame * D + s.pos) }
  | done {s : Seq} (e f p l : Nat) (h : s.reader = .posed e f p l) : SeqStep s { s with reader := .idle }

/-- The states the steps reach from an even epoch with its offset named. -/
inductive SeqReach : Seq → Prop where
  | start (f p : Nat) : SeqReach ⟨0, f, p, f * D + p, .idle⟩
  | step {s s' : Seq} : SeqReach s → SeqStep D s s' → SeqReach s'

/-- The invariant. With the epoch even the frame and the position name the offset. A reader past its first read
holds an even epoch at or below the epoch. Where the epoch is still the one it read, its frame is the frame, and its
pair names the offset it saw. -/
structure SeqInv (s : Seq) : Prop where
  named : s.epoch % 2 = 0 → s.frame * D + s.pos = s.off
  began : ∀ e, s.reader = .began e → e % 2 = 0 ∧ e ≤ s.epoch
  framed : ∀ e f, s.reader = .framed e f → e % 2 = 0 ∧ e ≤ s.epoch ∧ (s.epoch = e → f = s.frame)
  posed : ∀ e f p l, s.reader = .posed e f p l → e % 2 = 0 ∧ e ≤ s.epoch ∧ (s.epoch = e → f * D + p = l)

theorem seqInv_step {s s' : Seq} (hs : SeqInv D s) (st : SeqStep D s s') : SeqInv D s' := by
  cases st with
  | «open» h =>
    refine ⟨fun he => by simp only at he; omega, ?_, ?_, ?_⟩
    · intro e he; have := hs.began e he; simp only; omega
    · intro e f he; have := hs.framed e f he; simp only; omega
    · intro e f p l he; have := hs.posed e f p l he; simp only; omega
  | setFrame f h =>
    refine ⟨fun he => by simp only at he; omega, hs.began, ?_, hs.posed⟩
    intro e f' he
    have := hs.framed e f' he
    refine ⟨this.1, this.2.1, fun hee => ?_⟩
    simp only at hee; omega
  | setPos p h =>
    exact ⟨fun he => by simp only at he; omega, hs.began, hs.framed, hs.posed⟩
  | close h =>
    refine ⟨fun _ => rfl, ?_, ?_, ?_⟩
    · intro e he; have := hs.began e he; simp only; omega
    · intro e f he; have := hs.framed e f he; simp only; omega
    · intro e f p l he; have := hs.posed e f p l he; simp only; omega
  | bump n h =>
    refine ⟨fun he => ?_, hs.began, hs.framed, hs.posed⟩
    have := hs.named h
    simp only
    rw [← this]
    omega
  | readEpoch h he =>
    refine ⟨hs.named, ?_, ?_, ?_⟩
    · intro e hr; simp only at hr; cases hr; exact ⟨he, Nat.le_refl _⟩
    · intro e f hr; simp at hr
    · intro e f p l hr; simp at hr
  | readFrame e h =>
    refine ⟨hs.named, ?_, ?_, ?_⟩
    · intro e' hr; simp at hr
    · intro e' f hr
      simp only at hr
      cases hr
      have := hs.began e h
      exact ⟨this.1, this.2, fun _ => rfl⟩
    · intro e' f p l hr; simp at hr
  | readPos e f h =>
    refine ⟨hs.named, ?_, ?_, ?_⟩
    · intro e' hr; simp at hr
    · intro e' f' hr; simp at hr
    · intro e' f' p l hr
      simp only at hr
      cases hr
      have := hs.framed e f h
      refine ⟨this.1, this.2.1, fun hee => ?_⟩
      rw [this.2.2 hee]
  | done e f p l h =>
    refine ⟨hs.named, ?_, ?_, ?_⟩
    · intro e' hr; simp at hr
    · intro e' f' hr; simp at hr
    · intro e' f' p' l' hr; simp at hr

theorem seqInv_reach {s : Seq} (h : SeqReach D s) : SeqInv D s := by
  induction h with
  | start f p =>
    refine ⟨fun _ => rfl, ?_, ?_, ?_⟩
    · intro e hr; simp at hr
    · intro e f hr; simp at hr
    · intro e f p l hr; simp at hr
  | step _ st ih => exact seqInv_step D ih st

/-- A reader's second read of the epoch finds the epoch it began with. Then its frame and its position name the
offset at its read of the position. -/
theorem reader_pair {s : Seq} (h : SeqReach D s) (e f p l : Nat) (hr : s.reader = .posed e f p l)
    (he : s.epoch = e) : f * D + p = l :=
  ((seqInv_reach D h).posed e f p l hr).2.2 he

end Epoch

/-! ## The give's test of the boundary

A give compares its cell with the boundary modulo `D`, and so it reads the boundary word alone. A cell at the
boundary passes the test whatever frame the word counts from. A cell away from it can pass the test too, and that
give goes to its fetch-or, which `Boundary.Step.readAny` holds. -/

/-- A cell at the boundary agrees with the boundary's position modulo `D`, whatever the frame. -/
theorem at_boundary_mod (D f p c : Nat) (h : f * D + p = c) : c % D = p % D := by
  rw [← h, Nat.add_comm, Nat.add_mul_mod_self_right]

/-! ## The sizes

On a 64-bit word a half is `2 ^ 32`, `W` is `2 ^ 31`, a bank is `2 ^ 30` bytes, and each field has 20 bits. On a
32-bit word a half is `2 ^ 16`, `W` is `2 ^ 15`, a bank is `2 ^ 14` bytes, and each field has 9 bits. `W` is
`MMGR_CARCER_BYTES_MAX`, and so `Word.lanes_64` and `Word.lanes_32` hold for each position as they are. -/

/-- A position's reach is two banks. -/
theorem reach_64 : 2 * 2 ^ 30 = 2 ^ 31 := by decide

/-- A position's reach is two banks. -/
theorem reach_32 : 2 * 2 ^ 14 = 2 ^ 15 := by decide

/-- A pool on a 64-bit word holds up to `(2 ^ 20 - 1) * 2 ^ 30` bytes, which a size_t counts. -/
theorem pool_64 : (2 ^ 20 - 1) * 2 ^ 30 < 2 ^ 64 := by decide

/-- A pool on a 32-bit word holds up to `(2 ^ 9 - 1) * 2 ^ 14` bytes, which a size_t counts. -/
theorem pool_32 : (2 ^ 9 - 1) * 2 ^ 14 < 2 ^ 32 := by decide

/-- On a 32-bit word, 128 KiB is eight banks. -/
theorem pool_32_eight : 8 * 2 ^ 14 = 128 * 1024 := by decide

end LocusCarcerum.Banks
