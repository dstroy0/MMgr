/-!
# Linea, the walks of a copy

A copy walks its bytes in blocks. Each block reads all of its source bytes before it stores any of its
destination bytes, as a word load and a word store do. The upward walk takes the blocks from the low end,
and the downward walk takes them from the high end. The width of each block is free, and so the theorems
hold for words, for bytes, and for any mix of the two.

The theorems. The upward walk leaves the destination equal to the source as it was, where the destination
lies at or below the source or wholly past it. The downward walk does the same where the destination lies
at or above the source or wholly past it. A move takes the upward walk where the destination is at or below
the source, and the downward walk otherwise. A move is then right for every overlap.
-/

namespace Linea.Walk

/-- Memory: the byte at each address. -/
abbrev Mem := Nat → Nat

/-- One block: `w` bytes from `src + p` to `dst + p`. Every byte comes from `m`, the memory before any store
of the block. -/
def block (dst src p w : Nat) (m : Mem) : Mem :=
  fun a => if dst + p ≤ a ∧ a < dst + p + w then m (src + (a - dst)) else m a

/-- The upward walk: blocks of the widths in the list, from offset `p` up. -/
def walkUp (dst src : Nat) : List Nat → Nat → Mem → Mem
  | [], _, m => m
  | w :: ws, p, m => walkUp dst src ws (p + w) (block dst src p w m)

/-- The downward walk: blocks of the widths in the list, each one below offset `q`, from the top down. -/
def walkDown (dst src : Nat) : List Nat → Nat → Mem → Mem
  | [], _, m => m
  | w :: ws, q, m => walkDown dst src ws (q - w) (block dst src (q - w) w m)

/-- The bytes from offset `p` to offset `n` copied, and every other address as `old` holds it. -/
def span (dst src p n : Nat) (old : Mem) : Mem :=
  fun a => if dst + p ≤ a ∧ a < dst + n then old (src + (a - dst)) else old a

/-- What a copy of `n` bytes leaves: the destination holds the source as `old` held it, and every other address
is unchanged. -/
def moved (dst src n : Nat) (old : Mem) : Mem := span dst src 0 n old

theorem span_empty (dst src p : Nat) (old : Mem) : span dst src p p old = old := by
  funext a
  simp only [span]
  split <;> first | rfl | omega

/-- One upward block extends the copied span by its width, where its reads miss the span copied so far. -/
theorem block_up (dst src p w : Nat) (old : Mem)
    (safe : ∀ j, p ≤ j → j < p + w → ¬ (dst ≤ src + j ∧ src + j < dst + p)) :
    block dst src p w (span dst src 0 p old) = span dst src 0 (p + w) old := by
  funext a
  unfold block span
  by_cases hin : dst + p ≤ a ∧ a < dst + p + w
  · have hr := safe (a - dst) (by omega) (by omega)
    have hx : ¬ (dst + 0 ≤ src + (a - dst) ∧ src + (a - dst) < dst + p) := by omega
    have hy : dst + 0 ≤ a ∧ a < dst + (p + w) := by omega
    rw [ite_eq_left hin, ite_eq_right hx, ite_eq_left hy]
  · rw [ite_eq_right hin]
    by_cases hlow : dst + 0 ≤ a ∧ a < dst + p
    · have hy : dst + 0 ≤ a ∧ a < dst + (p + w) := by omega
      rw [ite_eq_left hlow, ite_eq_left hy]
    · have hy : ¬ (dst + 0 ≤ a ∧ a < dst + (p + w)) := by omega
      rw [ite_eq_right hlow, ite_eq_right hy]

/-- The upward walk copies every byte it covers, where no read of a block falls in the span copied before it. -/
theorem walkUp_span (dst src : Nat) (old : Mem) :
    ∀ (ws : List Nat) (p : Nat),
      (∀ j, p ≤ j → j < p + ws.sum → ¬ (dst ≤ src + j ∧ src + j < dst + j)) →
      walkUp dst src ws p (span dst src 0 p old) = span dst src 0 (p + ws.sum) old
  | [], p, _ => by simp [walkUp]
  | w :: ws, p, safe => by
    simp only [walkUp, List.sum_cons] at safe ⊢
    have hb : block dst src p w (span dst src 0 p old) = span dst src 0 (p + w) old :=
      block_up dst src p w old (fun j h1 h2 => by
        have := safe j h1 (by omega)
        omega)
    rw [hb, walkUp_span dst src old ws (p + w) (fun j h1 h2 => safe j (by omega) (by omega))]
    rw [Nat.add_assoc]

/-- The upward walk is a copy where the destination lies at or below the source, or wholly before it. -/
theorem walkUp_moved (dst src n : Nat) (ws : List Nat) (old : Mem) (cover : ws.sum = n)
    (apart : dst ≤ src ∨ src + n ≤ dst) :
    walkUp dst src ws 0 old = moved dst src n old := by
  have h := walkUp_span dst src old ws 0 (fun j _ h2 => by omega)
  rw [span_empty] at h
  simpa [moved, cover] using h

/-- One downward block extends the copied span down by its width, where its reads miss the span copied so far.
The block lies inside the copy: its top `q` is at most `n`, and its width is at most `q`. -/
theorem block_down (dst src q w n : Nat) (old : Mem) (hw : w ≤ q) (hq : q ≤ n)
    (safe : ∀ j, q - w ≤ j → j < q → ¬ (dst + q ≤ src + j ∧ src + j < dst + n)) :
    block dst src (q - w) w (span dst src q n old) = span dst src (q - w) n old := by
  funext a
  unfold block span
  by_cases hin : dst + (q - w) ≤ a ∧ a < dst + (q - w) + w
  · have hr := safe (a - dst) (by omega) (by omega)
    have hx : ¬ (dst + q ≤ src + (a - dst) ∧ src + (a - dst) < dst + n) := by omega
    have hy : dst + (q - w) ≤ a ∧ a < dst + n := by omega
    rw [ite_eq_left hin, ite_eq_right hx, ite_eq_left hy]
  · rw [ite_eq_right hin]
    by_cases hhigh : dst + q ≤ a ∧ a < dst + n
    · have hy : dst + (q - w) ≤ a ∧ a < dst + n := by omega
      rw [ite_eq_left hhigh, ite_eq_left hy]
    · have hy : ¬ (dst + (q - w) ≤ a ∧ a < dst + n) := by omega
      rw [ite_eq_right hhigh, ite_eq_right hy]

/-- The downward walk copies every byte it covers, from its top `q` down to the bottom, where no read of a
block falls in the span copied before it. -/
theorem walkDown_span (dst src n : Nat) (old : Mem)
    (apart : src ≤ dst ∨ dst + n ≤ src) :
    ∀ (ws : List Nat) (q : Nat), ws.sum = q → q ≤ n →
      walkDown dst src ws q (span dst src q n old) = span dst src 0 n old
  | [], q, cover, _ => by
    simp only [List.sum_nil] at cover
    subst cover
    simp [walkDown]
  | w :: ws, q, cover, hq => by
    simp only [List.sum_cons] at cover
    simp only [walkDown]
    have hb : block dst src (q - w) w (span dst src q n old) = span dst src (q - w) n old :=
      block_down dst src q w n old (by omega) hq (fun j _ h2 => by omega)
    rw [hb]
    exact walkDown_span dst src n old apart ws (q - w) (by omega) (by omega)

/-- The downward walk is a copy where the destination lies at or above the source, or wholly before it. -/
theorem walkDown_moved (dst src n : Nat) (ws : List Nat) (old : Mem) (cover : ws.sum = n)
    (apart : src ≤ dst ∨ dst + n ≤ src) :
    walkDown dst src ws n old = moved dst src n old := by
  have h := walkDown_span dst src n old apart ws n cover (Nat.le_refl n)
  rw [span_empty] at h
  simpa [moved] using h

/-- A move: the upward walk where the destination is at or below the source, the downward walk otherwise. -/
def move (dst src n : Nat) (ws : List Nat) (m : Mem) : Mem :=
  if dst ≤ src then walkUp dst src ws 0 m else walkDown dst src ws n m

/-- A move is a copy at every overlap, with blocks of any widths that cover the bytes. -/
theorem move_moved (dst src n : Nat) (ws : List Nat) (old : Mem) (cover : ws.sum = n) :
    move dst src n ws old = moved dst src n old := by
  unfold move
  split
  · exact walkUp_moved dst src n ws old cover (Or.inl (by assumption))
  · exact walkDown_moved dst src n ws old cover (Or.inl (by omega))

end Linea.Walk
