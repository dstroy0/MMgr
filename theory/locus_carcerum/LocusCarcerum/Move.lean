import LocusCarcerum.Take

/-!
# LocusCarcerum, the move of a cell

The mover (`mmgr_movable_buf_move`) carries the busy cell `c` at the tier's end into a free run `r` below it. The
run splits: the cell's bytes sit at the run's offset and the rest stays free. The cell's old place goes free. The
move ends with the sweep of the tier: the move sets the persistent trim bit, and the open runs the sweep.

The theorems: the swept tier ends busy, so a release of the moved cell finds it at the boundary; every owner's
count is as it was before the move; and a tier of the run and the cell alone is the moved cell alone after the
sweep, so that release leaves the tier empty.
-/

namespace LocusCarcerum.Move

open LocusCarcerum.Layout
open LocusCarcerum.Take

variable (H : Nat)

/-- The tier before the move: the run `r` lies under `between`, and the cell `c` ends the tier. -/
def moveFrom (before : List Cell) (r : Cell) (between : List Cell) (c : Cell) : List Cell :=
  before ++ (r :: between) ++ [c]

/-- The tier after the move and before the sweep: `before` lies under the run, the moved cell and the run's rest
take the run's place, `between` lies from the run to the cell's old place, and the old place is free. -/
def moveInto (before : List Cell) (r : Cell) (between : List Cell) (c : Cell) : List Cell :=
  before ++ (⟨c.size, true, c.owner⟩ :: ⟨r.size - c.size - H, false, 0⟩ :: between) ++ [⟨c.size, false, 0⟩]

/-- The swept tier ends busy after a move. -/
theorem move_ends (before : List Cell) (r : Cell) (between : List Cell) (c : Cell) :
    endsBusy (sweep H (moveInto H before r between c)) := sweep_ends _ _

/-- Every owner's count is as it was before the move, where the run is free and the cell busy. -/
theorem move_count (o : Nat) (before : List Cell) (r : Cell) (between : List Cell) (c : Cell)
    (hr : r.busy = false) (hc : c.busy = true) :
    count o (sweep H (moveInto H before r between c)) = count o (moveFrom before r between c) := by
  rw [sweep_count]
  unfold moveInto moveFrom
  by_cases ho : c.owner = o <;> simp [count, count_append, hr, hc, ho] <;> omega

/-- A tier of the run and the cell alone is the moved cell alone after the sweep. -/
theorem move_alone (r : Cell) (c : Cell) :
    sweep H (moveInto H [] r [] c) = [⟨c.size, true, c.owner⟩] := by
  simp [moveInto, sweep, merge, trimEnd, joined]

end LocusCarcerum.Move
