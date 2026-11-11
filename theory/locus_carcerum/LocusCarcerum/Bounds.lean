/-!
# LocusCarcerum, the bounds the lookups and the spans test

A span of a movable cell (`carcer_span_read`) fits under two conditions. Its offset is at most the cell's size, and
its bytes at most the size less the offset. The test never forms `offset + bytes`, which can wrap. The theorem is that a span it
passes lies inside the cell. A pointer is the cellblock's (`mmgr_who_owns_buf`) where it lies at or past the base and
below the base and the size. A high-water mark (`embed_atomic_usize_raise`) only rises, and stands at or past every
count it reached.
-/

namespace LocusCarcerum.Bounds

/-- The span test as the C writes it, in words of `W` values that wrap. -/
def fits (W size offset bytes : Nat) : Bool := decide (offset ≤ size) && decide (bytes ≤ (size - offset) % W)

/-- A span the test passes lies inside the cell, with no sum formed that could wrap. -/
theorem fits_inside (W size offset bytes : Nat) (hW : size < W) (h : fits W size offset bytes = true) :
    offset + bytes ≤ size := by
  simp [fits] at h
  obtain ⟨ho, hb⟩ := h
  rw [Nat.mod_eq_of_lt (by omega)] at hb
  omega

/-- A span the test refuses runs past the cell's end. -/
theorem refused_outside (W size offset bytes : Nat) (hW : size < W) (h : fits W size offset bytes = false) :
    size < offset + bytes := by
  simp [fits] at h
  by_cases ho : offset ≤ size
  · have hb := h ho
    rw [Nat.mod_eq_of_lt (by omega)] at hb
    omega
  · omega

/-- The cellblock's test of a pointer. -/
def owns (base size p : Nat) : Bool := decide (base ≤ p) && decide (p < base + size)

theorem owns_iff (base size p : Nat) : owns base size p = true ↔ base ≤ p ∧ p - base < size := by
  simp [owns]; omega

/-- A raise: the mark becomes the larger of itself and the count. -/
def raise (mark count : Nat) : Nat := if mark < count then count else mark

theorem raise_ge_count (mark count : Nat) : count ≤ raise mark count := by unfold raise; split <;> omega
theorem raise_ge_mark (mark count : Nat) : mark ≤ raise mark count := by unfold raise; split <;> omega

/-- Raises in turn: the mark after all of them stands at or past each count raised to. -/
def raises (mark : Nat) : List Nat → Nat
  | [] => mark
  | c :: rest => raises (raise mark c) rest

theorem raises_ge (mark : Nat) (counts : List Nat) : ∀ c ∈ counts, c ≤ raises mark counts := by
  induction counts generalizing mark with
  | nil => intro c hc; simp at hc
  | cons d rest ih =>
    intro c hc
    have mono : ∀ (m : Nat) (l : List Nat), m ≤ raises m l := by
      intro m l
      induction l generalizing m with
      | nil => exact Nat.le_refl _
      | cons e r ihr => exact Nat.le_trans (raise_ge_mark m e) (ihr _)
    simp at hc
    rcases hc with hc | hc
    · subst hc; exact Nat.le_trans (raise_ge_count mark c) (mono _ _)
    · exact ih _ c hc

end LocusCarcerum.Bounds
