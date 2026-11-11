import LocusCarcerum.Layout
import LocusCarcerum.Take

/-!
# LocusCarcerum, the spill and the growths

A persistent take the gap cannot meet (`carcer_spilled`) sweeps the tier. Then it lays the take over the first free
run that holds it. Where none does and the gap has room, it bumps the take at the end. The theorems: a take laid over a
run keeps the tier's bytes and raises the owner's count by what the cell carries. A bump at the end lays the cell's
footprint past the end and raises the count by the request. The other owners' counts do not move.

A growth (`carcer_grown`) grows the newest cell of its tier. A persistent cell grows in place: the end rises by
the bytes added and the owner's count by the same. A temporary cell moves down: its header at the new top, its
owner word where it stood, the tier's end where it stood.
-/

namespace LocusCarcerum.Spill

open LocusCarcerum.Layout
open LocusCarcerum.Take

variable (H A : Nat)

/-- The first free run that holds `want`: the cells below it, the run, and the cells above it. -/
def firstFit (want : Nat) : List Cell → Option (List Cell × Cell × List Cell)
  | [] => none
  | c :: rest =>
    if c.busy = false ∧ want ≤ c.size then some ([], c, rest)
    else match firstFit want rest with
      | none => none
      | some (low, run, high) => some (c :: low, run, high)

theorem firstFit_spec (want : Nat) (l low high : List Cell) (run : Cell)
    (h : firstFit want l = some (low, run, high)) :
    l = low ++ run :: high ∧ run.busy = false ∧ want ≤ run.size := by
  induction l generalizing low with
  | nil => simp [firstFit] at h
  | cons c rest ih =>
    simp only [firstFit] at h
    split at h
    · next hc => simp at h; obtain ⟨rfl, rfl, rfl⟩ := h; exact ⟨rfl, hc.1, hc.2⟩
    · split at h
      · simp at h
      · next low' run' high' hr =>
        simp at h
        obtain ⟨rfl, rfl, rfl⟩ := h
        obtain ⟨e, hb, hs⟩ := ih low' hr
        exact ⟨by rw [e]; rfl, hb, hs⟩

theorem len_append (a b : List Cell) : len H (a ++ b) = len H a + len H b := by
  induction a with
  | nil => simp [len]
  | cons c rest ih => simp [len, ih]; omega

/-- A spill over the first run that fits in the swept tier. -/
def spilled (want owner : Nat) (low : List Cell) (run : Cell) (high : List Cell) : List Cell :=
  low ++ laid H A want owner run ++ high

theorem spill_len (want owner : Nat) (l low high : List Cell) (run : Cell)
    (h : firstFit want (sweep H l) = some (low, run, high)) :
    len H (spilled H A want owner low run high) = len H (sweep H l) := by
  obtain ⟨e, _, _⟩ := firstFit_spec want _ low high run h
  rw [e]; unfold spilled
  simp [len_append, laid_len, len]

theorem spill_count (want owner o : Nat) (l low high : List Cell) (run : Cell)
    (h : firstFit want (sweep H l) = some (low, run, high)) :
    count o (spilled H A want owner low run high) = count o l + (if owner = o then carried H A want run else 0) := by
  obtain ⟨e, hb, _⟩ := firstFit_spec want _ low high run h
  have hs := sweep_count H o l
  rw [e] at hs
  unfold spilled
  rw [count_append, count_append, laid_count H A want owner o run hb, ← hs, count_append]
  simp [count, hb]
  omega

/-- A bump at the end: the cell laid past the last. -/
def bumped (want owner : Nat) (l : List Cell) : List Cell := l ++ [⟨want, true, owner⟩]

theorem bump_len (want owner : Nat) (l : List Cell) : len H (bumped want owner l) = len H l + (H + want) := by
  unfold bumped; simp [len_append, len, foot]

theorem bump_count (want owner o : Nat) (l : List Cell) :
    count o (bumped want owner l) = count o l + (if owner = o then want else 0) := by
  unfold bumped; rw [count_append]; by_cases ho : owner = o <;> simp [count, ho]

/-- A persistent growth of the newest cell to `want`. -/
def grownUp (want : Nat) (l : List Cell) (c : Cell) : List Cell := l ++ [{ c with size := want }]

theorem grownUp_len (want : Nat) (l : List Cell) (c : Cell) (h : c.size ≤ want) :
    len H (grownUp want l c) = len H (l ++ [c]) + (want - c.size) := by
  unfold grownUp; simp [len_append, len, foot]; omega

theorem grownUp_count (want o : Nat) (l : List Cell) (c : Cell) (hb : c.busy = true) (h : c.size ≤ want) :
    count o (grownUp want l c) = count o (l ++ [c]) + (if c.owner = o then want - c.size else 0) := by
  unfold grownUp; rw [count_append, count_append]
  by_cases ho : c.owner = o <;> simp [count, hb, ho] <;> omega

/-- A temporary growth of the newest cell, at `top` with `size` bytes, to `want`: the new top, and where its owner
word stands, the end of its footprint. -/
def grownDown (top size want : Nat) : Nat × Nat := (top - (want - size), top - (want - size) + H + want)

theorem grownDown_owner_word (top size want : Nat) (h : size ≤ want) (room : want - size ≤ top) :
    (grownDown H top size want).2 = top + H + size := by
  unfold grownDown; omega

end LocusCarcerum.Spill
