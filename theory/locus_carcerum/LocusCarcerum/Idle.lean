import LocusCarcerum.Layout
import LocusCarcerum.Regions

/-!
# LocusCarcerum, the idle spans and the placed cellblock

The idle walk passes the program the spans no cell holds, with the cellblock closed: the gap between the tiers, from
the persistent end to the temporary top, and each region's room, from its start up to its depth. A tier positions
its cells end to end: the persistent tier up from offset 0, a temporary stack down from its top. The theorems: a
cell positioned up from a base ends at or below the base plus the tier's bytes, a cell positioned down from a top
starts at or above the top less the stack's bytes, and so no byte of the gap and no byte of a region's room lies in
a cell. `Regions.Bounded` gives each region's bytes within its bound, which places its room inside the region.

A cellblock over a hospes pool has size 0 until the program places it, and a take permits nothing where its size is
not below the cellblock's. The theorems: an unplaced cellblock refuses every take, and a placed one permits a take
below its size and refuses one at or over it.
-/

namespace LocusCarcerum.Idle

open LocusCarcerum.Layout

variable (H : Nat)

/-- The spans of a tier's cells positioned up from `base`: each cell's start and end. -/
def spansUp (base : Nat) : List Cell → List (Nat × Nat)
  | [] => []
  | c :: rest => (base, base + foot H c) :: spansUp (base + foot H c) rest

/-- Every cell positioned up from `base` ends at or below `base` plus the tier's bytes. -/
theorem spansUp_below (base : Nat) (l : List Cell) :
    ∀ s ∈ spansUp H base l, s.2 ≤ base + len H l := by
  induction l generalizing base with
  | nil =>
    intro s hs
    simp [spansUp] at hs
  | cons c rest ih =>
    intro s hs
    simp only [spansUp, List.mem_cons] at hs
    rcases hs with rfl | hs
    · simp only [len]
      omega
    · have := ih (base + foot H c) s hs
      simp only [len]
      omega

/-- The spans of a stack's cells positioned down from `top`: the first cell just under `top`, each next below. -/
def spansDown (top : Nat) : List Cell → List (Nat × Nat)
  | [] => []
  | c :: rest => (top - foot H c, top) :: spansDown (top - foot H c) rest

/-- Every cell positioned down from `top` starts at or above `top` less the stack's bytes. -/
theorem spansDown_above (top : Nat) (l : List Cell) (h : len H l ≤ top) :
    ∀ s ∈ spansDown H top l, top - len H l ≤ s.1 := by
  induction l generalizing top with
  | nil =>
    intro s hs
    simp [spansDown] at hs
  | cons c rest ih =>
    intro s hs
    simp only [spansDown, List.mem_cons] at hs
    simp only [len] at h
    rcases hs with rfl | hs
    · simp only [len]
      omega
    · have := ih (top - foot H c) (by omega) s hs
      simp only [len]
      omega

/-- Whether a byte at `x` lies in the span `s`. -/
def within (x : Nat) (s : Nat × Nat) : Prop := s.1 ≤ x ∧ x < s.2

/-- A byte of the gap, from the persistent end to the temporary top, lies in no cell of either tier. -/
theorem gap_idle (persistent temporary : List Cell) (size x : Nat) (hsize : len H temporary ≤ size)
    (hend : len H persistent ≤ x) (htop : x < size - len H temporary) :
    (∀ s ∈ spansUp H 0 persistent, ¬ within x s) ∧ (∀ s ∈ spansDown H size temporary, ¬ within x s) := by
  constructor
  · intro s hs ⟨_, hx⟩
    have := spansUp_below H 0 persistent s hs
    omega
  · intro s hs ⟨hx, _⟩
    have := spansDown_above H size temporary hsize s hs
    omega

/-- A byte of a region's room, below the region's depth, lies in no cell of the region. The room the walk passes
starts at the region's start, `regionEnd - R`, and the theorem holds for every byte under the depth. -/
theorem room_idle (region : List Cell) (regionEnd R x : Nat) (hR : len H region ≤ R) (hend : R ≤ regionEnd)
    (hhigh : x < regionEnd - len H region) :
    ∀ s ∈ spansDown H regionEnd region, ¬ within x s := by
  intro s hs ⟨hx, _⟩
  have := spansDown_above H regionEnd region (by omega) s hs
  omega

/-- Every region of a bounded set of regions has its room idle: a byte below its depth lies in no cell of it. -/
theorem regions_room_idle (bound : Regions.Bound) (regions : Regions.Regions) (h : Regions.Bounded H bound regions)
    (place regionEnd x : Nat) (hend : bound place ≤ regionEnd) (hhigh : x < regionEnd - len H (regions place)) :
    ∀ s ∈ spansDown H regionEnd (regions place), ¬ within x s :=
  room_idle H (regions place) regionEnd (bound place) x (h place) hend hhigh

/-- Whether a take of `size` bytes permits anything: its size below the cellblock's. -/
def permits (cellblock size : Nat) : Prop := size < cellblock

/-- A cellblock not yet placed, of size 0, refuses every take. -/
theorem unplaced_refuses (size : Nat) : ¬ permits 0 size := by
  simp [permits]

/-- A cellblock placed on `bytes` permits a take below them and refuses one at or over them. -/
theorem placed_permits (bytes size : Nat) : permits bytes size ↔ size < bytes := Iff.rfl

end LocusCarcerum.Idle
