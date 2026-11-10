import LocusCarcerum.Layout
import LocusCarcerum.Take
import LocusCarcerum.Spill

/-!
# LocusCarcerum, the temporary tier as the persistent tier mirrored

The temporary tier is its cells laid end to end from the top up to the cellblock's size, the same cells as the
persistent tier's, read in the same order. The two tiers differ in where their base stands: the persistent tier's at
offset 0, its boundary the end of its last cell; the temporary tier's at the cellblock's size, its boundary the start
of its first cell. Every operation of the persistent tier is an operation of the temporary tier with that one change.

The sweep merges every run of free cells into one free cell, as `Layout.merge` does for both tiers, and returns the
free run at the boundary to the gap: the run reaching the end for the persistent tier (`Layout.trimEnd`), the run
starting at the top for the temporary tier (`trimStart`). A take the gap cannot meet lays its cell over the fitting
free run farthest from the gap: the first from offset 0 for the persistent tier (`Spill.firstFit`), the last before
the size for the temporary tier (`lastFit`). The cell is laid at the run's end away from the gap and the rest left
free facing it: `Take.laid` for the persistent tier, `laidHigh` for the temporary tier, which is `Take.laid` reversed.

The theorems are the persistent tier's, mirrored: the sweep keeps every busy cell in order and every owner's count,
never raises the tier's bytes, and leaves no free cell at the boundary; the run `lastFit` finds is free, holds the
take, and no run nearer the base holds it; a take laid over the run keeps the tier's bytes and raises its owner's
count by what the cell carries, and no other owner's.
-/

namespace LocusCarcerum.Temporary

open LocusCarcerum.Layout
open LocusCarcerum.Take
open LocusCarcerum.Spill

variable (H A : Nat)

/-- The free run at the start, the tier's top, returned to the gap. -/
def trimStart : List Cell → List Cell
  | [] => []
  | c :: rest => if c.busy then c :: rest else trimStart rest

/-- The first cell, where there is one, is busy. -/
def startsBusy : List Cell → Prop
  | [] => True
  | c :: _ => c.busy = true

/-- The sweep of the temporary tier: the merge, then the trim at the top. -/
def sweepDown (l : List Cell) : List Cell := trimStart (merge H l)

theorem trimStart_starts (l : List Cell) : startsBusy (trimStart l) := by
  induction l with
  | nil => trivial
  | cons c rest ih =>
    simp only [trimStart]
    split
    · next h => simpa [startsBusy] using h
    · exact ih

theorem trimStart_busy (l : List Cell) : busyCells (trimStart l) = busyCells l := by
  induction l with
  | nil => rfl
  | cons c rest ih =>
    simp only [trimStart]
    split
    · rfl
    · next h => simp [busyCells, h, ih]

theorem trimStart_count (o : Nat) (l : List Cell) : count o (trimStart l) = count o l := by
  rw [count_of_busy, trimStart_busy, ← count_of_busy]

theorem trimStart_len (l : List Cell) : len H (trimStart l) ≤ len H l := by
  induction l with
  | nil => exact Nat.le_refl _
  | cons c rest ih =>
    simp only [trimStart]
    split
    · exact Nat.le_refl _
    · simp only [len]; omega

theorem sweepDown_busy (l : List Cell) : busyCells (sweepDown H l) = busyCells l := by
  unfold sweepDown; rw [trimStart_busy, merge_busy]

theorem sweepDown_count (o : Nat) (l : List Cell) : count o (sweepDown H l) = count o l := by
  rw [count_of_busy, sweepDown_busy, ← count_of_busy]

theorem sweepDown_len (l : List Cell) : len H (sweepDown H l) ≤ len H l := by
  unfold sweepDown
  exact Nat.le_trans (trimStart_len H _) (Nat.le_of_eq (merge_len H l))

theorem sweepDown_starts (l : List Cell) : startsBusy (sweepDown H l) := trimStart_starts _

/-- Whether a free cell holds `want`. -/
def holds (want : Nat) (c : Cell) : Prop := c.busy = false ∧ want ≤ c.size

instance (want : Nat) (c : Cell) : Decidable (holds want c) := by unfold holds; infer_instance

/-- The last free run holding `want`, the one nearest the tier's base at the size: the cells below it, the run, and
the cells above it. -/
def lastFit (want : Nat) : List Cell → Option (List Cell × Cell × List Cell)
  | [] => none
  | c :: rest =>
    match lastFit want rest with
    | some (low, run, high) => some (c :: low, run, high)
    | none => if holds want c then some ([], c, rest) else none

theorem lastFit_none (want : Nat) (l : List Cell) (h : lastFit want l = none) : ∀ c ∈ l, ¬ holds want c := by
  induction l with
  | nil => intro c hc; simp at hc
  | cons c rest ih =>
    simp only [lastFit] at h
    split at h
    · simp at h
    · next hr =>
      split at h
      · simp at h
      · next hc =>
        intro d hd
        simp only [List.mem_cons] at hd
        rcases hd with rfl | hd
        · exact hc
        · exact ih hr d hd

theorem lastFit_spec (want : Nat) (l low high : List Cell) (run : Cell)
    (h : lastFit want l = some (low, run, high)) :
    l = low ++ run :: high ∧ run.busy = false ∧ want ≤ run.size := by
  induction l generalizing low with
  | nil => simp [lastFit] at h
  | cons c rest ih =>
    simp only [lastFit] at h
    split at h
    · next low' run' high' hr =>
      simp at h
      obtain ⟨rfl, rfl, rfl⟩ := h
      obtain ⟨e, hb, hs⟩ := ih low' hr
      exact ⟨by rw [e]; rfl, hb, hs⟩
    · split at h
      · next hc => simp at h; obtain ⟨rfl, rfl, rfl⟩ := h; exact ⟨rfl, hc.1, hc.2⟩
      · simp at h

/-- No run above the one found holds the take: the run is the one nearest the base. -/
theorem lastFit_nearest (want : Nat) (l low high : List Cell) (run : Cell)
    (h : lastFit want l = some (low, run, high)) : ∀ c ∈ high, ¬ holds want c := by
  induction l generalizing low with
  | nil => simp [lastFit] at h
  | cons c rest ih =>
    simp only [lastFit] at h
    split at h
    · next low' run' high' hr =>
      simp at h
      obtain ⟨rfl, rfl, rfl⟩ := h
      exact ih low' hr
    · next hr =>
      split at h
      · simp at h
        obtain ⟨rfl, rfl, rfl⟩ := h
        exact lastFit_none want rest hr
      · simp at h

/-- A take of `want` by `owner` laid over the free run `run` at its end away from the gap, the rest free facing the
gap. -/
def laidHigh (want owner : Nat) (run : Cell) : List Cell :=
  if want + H + A ≤ run.size then [⟨run.size - want - H, false, 0⟩, ⟨want, true, owner⟩]
  else [⟨run.size, true, owner⟩]

/-- The temporary tier's laying is the persistent tier's, reversed. -/
theorem laidHigh_reverse (want owner : Nat) (run : Cell) :
    laidHigh H A want owner run = (laid H A want owner run).reverse := by
  unfold laidHigh laid
  split <;> simp

theorem laidHigh_len (want owner : Nat) (run : Cell) : len H (laidHigh H A want owner run) = foot H run := by
  unfold laidHigh
  split <;> simp [len, foot] <;> omega

theorem laidHigh_count (want owner o : Nat) (run : Cell) (hfree : run.busy = false) :
    count o (laidHigh H A want owner run) = count o [run] + (if owner = o then carried H A want run else 0) := by
  unfold laidHigh carried
  split <;> by_cases ho : owner = o <;> simp [count, hfree, ho]

/-- A spill over the last fitting run of the swept temporary tier. -/
def spilledDown (want owner : Nat) (low : List Cell) (run : Cell) (high : List Cell) : List Cell :=
  low ++ laidHigh H A want owner run ++ high

theorem spillDown_len (want owner : Nat) (l low high : List Cell) (run : Cell)
    (h : lastFit want (sweepDown H l) = some (low, run, high)) :
    len H (spilledDown H A want owner low run high) = len H (sweepDown H l) := by
  obtain ⟨e, _, _⟩ := lastFit_spec want _ low high run h
  rw [e]; unfold spilledDown
  simp [len_append, laidHigh_len, len]

theorem spillDown_count (want owner o : Nat) (l low high : List Cell) (run : Cell)
    (h : lastFit want (sweepDown H l) = some (low, run, high)) :
    count o (spilledDown H A want owner low run high) =
      count o l + (if owner = o then carried H A want run else 0) := by
  obtain ⟨e, hb, _⟩ := lastFit_spec want _ low high run h
  have hs := sweepDown_count H o l
  rw [e] at hs
  unfold spilledDown
  rw [count_append, count_append, laidHigh_count H A want owner o run hb, ← hs, count_append]
  simp [count, hb]
  omega

/-- A bump at the top: the cell laid before the first. -/
def bumpedDown (want owner : Nat) (l : List Cell) : List Cell := ⟨want, true, owner⟩ :: l

theorem bumpDown_len (want owner : Nat) (l : List Cell) :
    len H (bumpedDown want owner l) = len H l + (H + want) := by
  unfold bumpedDown; simp [len, foot]; omega

theorem bumpDown_count (want owner o : Nat) (l : List Cell) :
    count o (bumpedDown want owner l) = count o l + (if owner = o then want else 0) := by
  unfold bumpedDown; by_cases ho : owner = o <;> simp [count, ho] <;> omega

end LocusCarcerum.Temporary
