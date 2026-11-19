import LocusCarcerum.Layout
import LocusCarcerum.Temporary
import LocusCarcerum.Stacks

/-!
# LocusCarcerum, the temporary tier as regions of a declared size

The temporary tier is regions, declared with the cellblock: one of `R` bytes for each thread's place, modulo their
count, and one large region of `L` bytes that every take over a region's room goes to. A region is a stack of
cells from its end down, as `Temporary` lays the tier. A thread takes in the region its place selects, or in the
large region where the take does not fit its own or its own refuses it. A give goes to the region that holds the
cell, from any thread.

A take bumps the region's depth where the region holds the cell. Where it does not, the take sweeps the region
(`Temporary.sweepDown`), bumps where the sweep made room, and else takes the last free run that holds it
(`Temporary.lastFit`, `Temporary.spilledDown`). Where no path holds the take, the take refuses and adds no cell.
That is the take of the tier, inside one region, with the refusal in place of the gap.

The theorems: the depth of a region never passes its bound, after any interleaving of takes and gives from any
thread. A refused take moves no busy cell. No take and no give moves another owner's count. An operation in one
region leaves every other region as it was. The bytes of the regions together stay within the sum of their bounds,
so the tier's boundary never moves. `Stacks.shared_sinks` is the contrast: one stack that two threads share sinks
past every depth. A thread's own region is shared by no other thread, so `Stacks.run_depth` holds there, and the
large region is the one stack that threads share, bounded by `L`.
-/

namespace LocusCarcerum.Regions

open LocusCarcerum.Layout
open LocusCarcerum.Temporary
open LocusCarcerum.Stacks (giveNewest give freed)

variable (H A : Nat)

/-! ## One region, bounded by `R` -/

section Region

variable (R : Nat)

/-- Whether a region holds one more cell of `want`: its depth with the cell within `R`. -/
def fits (want : Nat) (region : List Cell) : Prop := len H region + (H + want) ≤ R

instance (want : Nat) (region : List Cell) : Decidable (fits H R want region) := by unfold fits; infer_instance

/-- A take of `want` by `owner` in one region. The bump where the region holds it. Else the region swept, and the
bump again where the sweep made room. Else the last free run that holds the take. Else the swept region with no
cell added: the take refused. -/
def take (want owner : Nat) (region : List Cell) : List Cell :=
  if fits H R want region then bumpedDown want owner region
  else if fits H R want (sweepDown H region) then bumpedDown want owner (sweepDown H region)
  else match lastFit want (sweepDown H region) with
    | some (low, run, high) => spilledDown H A want owner low run high
    | none => sweepDown H region

/-- A region within `R` stays within `R` over a take. -/
theorem take_len (want owner : Nat) (region : List Cell) (h : len H region ≤ R) :
    len H (take H A R want owner region) ≤ R := by
  unfold take
  split
  · next hf => rw [bumpDown_len]; exact hf
  · split
    · next hf => rw [bumpDown_len]; exact hf
    · split
      · next low run high hl =>
        rw [spillDown_len H A want owner region low high run hl]
        exact Nat.le_trans (sweepDown_len H region) h
      · exact Nat.le_trans (sweepDown_len H region) h

/-- A take refused leaves the busy cells as they were: a sweep moves no busy cell. -/
theorem take_refused_busy (want owner : Nat) (region : List Cell) (h1 : ¬ fits H R want region)
    (h2 : ¬ fits H R want (sweepDown H region)) (h3 : lastFit want (sweepDown H region) = none) :
    busyCells (take H A R want owner region) = busyCells region := by
  simp only [take, h1, h2, h3, ite_false]
  exact sweepDown_busy H region

/-- No take moves another owner's count. -/
theorem take_count_other (want owner o : Nat) (region : List Cell) (ho : owner ≠ o) :
    count o (take H A R want owner region) = count o region := by
  unfold take
  split
  · rw [bumpDown_count]; simp [ho]
  · split
    · rw [bumpDown_count, sweepDown_count]; simp [ho]
    · split
      · next low run high hl =>
        rw [spillDown_count H A want owner o region low high run hl]; simp [ho]
      · exact sweepDown_count H o region

/-- No take lowers a count. -/
theorem take_count_le (want owner o : Nat) (region : List Cell) :
    count o region ≤ count o (take H A R want owner region) := by
  unfold take
  split
  · rw [bumpDown_count]; omega
  · split
    · rw [bumpDown_count, sweepDown_count]; omega
    · split
      · next low run high hl => rw [spillDown_count H A want owner o region low high run hl]; omega
      · exact Nat.le_of_eq (sweepDown_count H o region).symm

end Region

/-! ## The give -/

theorem giveNewest_len (owner : Nat) (region : List Cell) : len H (giveNewest owner region) = len H region := by
  induction region with
  | nil => rfl
  | cons c rest ih =>
    simp only [giveNewest]
    split
    · simp [len, foot, freed]
    · simp [len, ih]

/-- A give never deepens a region. -/
theorem give_len (owner : Nat) (region : List Cell) : len H (give owner region) ≤ len H region := by
  unfold give
  exact Nat.le_trans (trimStart_len H _) (Nat.le_of_eq (giveNewest_len H owner region))

theorem giveNewest_count_other (owner o : Nat) (region : List Cell) (ho : owner ≠ o) :
    count o (giveNewest owner region) = count o region := by
  induction region with
  | nil => rfl
  | cons c rest ih =>
    simp only [giveNewest]
    split
    · next hc => simp [count, freed, hc.2, ho]
    · simp [count, ih]

/-- No give moves another owner's count. -/
theorem give_count_other (owner o : Nat) (region : List Cell) (ho : owner ≠ o) :
    count o (give owner region) = count o region := by
  unfold give
  rw [trimStart_count, giveNewest_count_other owner o region ho]

/-! ## The regions together -/

/-- The regions, one for each place modulo their count, and the large one after them. -/
abbrev Regions := Nat → List Cell

/-- The bound of each region: `R` for a thread's region, `L` for the large one. -/
abbrev Bound := Nat → Nat

variable (bound : Bound)

/-! `large` is the large region: the one after the threads' regions, that every thread falls through to. -/
variable (large : Nat)

/-- The regions with the region `place` replaced. -/
def put (regions : Regions) (place : Nat) (region : List Cell) : Regions :=
  fun other => if other = place then region else regions other

/-- An operation on one region. A take names the owner it counts to. A give names the owner of the cell. -/
inductive Op where
  | take (want owner : Nat)
  | give (owner : Nat)
  deriving DecidableEq, Repr

/-- Whether a region answers a take of `want` by some path of `take`: the bump, the bump after the sweep, or the
last free run that holds it. -/
def holds (R want : Nat) (region : List Cell) : Prop :=
  fits H R want region ∨ fits H R want (sweepDown H region) ∨ (lastFit want (sweepDown H region)).isSome

instance (R want : Nat) (region : List Cell) : Decidable (holds H R want region) := by
  unfold holds; infer_instance

/-- One operation in the region `place`: a take in the region the thread's place or its size selects where that
region answers it, else in the large region; a give in the region that holds the cell. -/
def step (regions : Regions) (place : Nat) : Op → Regions
  | .take want owner =>
    if holds H (bound place) want (regions place) then
      put regions place (take H A (bound place) want owner (regions place))
    else put regions large (take H A (bound large) want owner (regions large))
  | .give owner => put regions place (give owner (regions place))

/-- A run of operations, each in one region, in the order the threads make them. -/
def run (regions : Regions) : List (Nat × Op) → Regions
  | [] => regions
  | (place, op) :: rest => run (step H A bound large regions place op) rest

/-- An operation in one region leaves every region as it was but that one and the large one. -/
theorem step_other (regions : Regions) (place other : Nat) (op : Op) (h : other ≠ place) (hl : other ≠ large) :
    step H A bound large regions place op other = regions other := by
  cases op with
  | take want owner =>
    simp only [step]
    split <;> simp [put, h, hl]
  | give owner => simp [step, put, h]

/-- Every region is within its bound. -/
def Bounded (regions : Regions) : Prop := ∀ place, len H (regions place) ≤ bound place

theorem step_bounded (regions : Regions) (place : Nat) (op : Op) (h : Bounded H bound regions) :
    Bounded H bound (step H A bound large regions place op) := by
  intro other
  cases op with
  | take want owner =>
    simp only [step]
    split
    · by_cases ho : other = place
      · subst ho
        simp only [put, ite_true]
        exact take_len H A (bound other) want owner _ (h other)
      · simp only [put, ho, ite_false]
        exact h other
    · by_cases ho : other = large
      · subst ho
        simp only [put, ite_true]
        exact take_len H A (bound other) want owner _ (h other)
      · simp only [put, ho, ite_false]
        exact h other
  | give owner =>
    by_cases ho : other = place
    · subst ho
      simp only [step, put, ite_true]
      exact Nat.le_trans (give_len H owner _) (h other)
    · simp only [step, put, ho, ite_false]
      exact h other

/-- Every region stays within its bound after any interleaving of takes and gives, from any thread, through the
fall to the large region. -/
theorem run_bounded (regions : Regions) (ops : List (Nat × Op)) (h : Bounded H bound regions) :
    Bounded H bound (run H A bound large regions ops) := by
  induction ops generalizing regions with
  | nil => exact h
  | cons op rest ih => exact ih _ (step_bounded H A bound large regions op.1 op.2 h)

/-- From empty regions, no interleaving takes a region past its bound. -/
theorem run_from_empty (ops : List (Nat × Op)) : Bounded H bound (run H A bound large (fun _ => []) ops) :=
  run_bounded H A bound large _ ops (fun _ => by simp [len])

/-- The bytes the first `N` regions position together. -/
def extent (regions : Regions) : Nat → Nat
  | 0 => 0
  | n + 1 => extent regions n + len H (regions n)

/-- The bounds of the first `N` regions together: the bytes the regions take off the cellblock. -/
def area : Nat → Nat
  | 0 => 0
  | n + 1 => area n + bound n

/-- The regions together stay within their area: the tier's boundary never moves. -/
theorem extent_le (regions : Regions) (N : Nat) (h : Bounded H bound regions) :
    extent H regions N ≤ area bound N := by
  induction N with
  | zero => simp [extent, area]
  | succ n ih =>
    have hn := h n
    simp only [extent, area]
    omega

end LocusCarcerum.Regions
