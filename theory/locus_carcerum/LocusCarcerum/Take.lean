import LocusCarcerum.Layout

/-!
# LocusCarcerum, a take laid over a free run, and the mover

A take that finds a free run lays its cell over it (`carcer_split`). The run splits where its rest holds a cell of
its own, a header and at least `A` bytes, and stays whole where it does not. The cell counts what it carries.

The theorems: the run's bytes stay laid. The cell carries at least the request and less than the request and a
header and `A` more. The owner's count rises by what the cell carries.

The mover (`mmgr_movable_buf_move`) lays a busy cell over the first free run below it the same way and frees the
place it stood. A run kept whole carries more than the cell did, and the module's running count for the owner
rises by the difference. The theorem is that the running count stays the count the layout holds. So the release,
which counts off what the header says, takes the owner back to what it was.
-/

namespace LocusCarcerum.Take

open LocusCarcerum.Layout

variable (H A : Nat)

/-- A take of `want` by `owner` laid over the free run `run`. -/
def laid (want owner : Nat) (run : Cell) : List Cell :=
  if want + H + A ≤ run.size then [⟨want, true, owner⟩, ⟨run.size - want - H, false, 0⟩]
  else [⟨run.size, true, owner⟩]

/-- What the cell carries. -/
def carried (want : Nat) (run : Cell) : Nat := if want + H + A ≤ run.size then want else run.size

theorem laid_len (want owner : Nat) (run : Cell) :
    len H (laid H A want owner run) = foot H run := by
  unfold laid
  split <;> simp [len, foot] <;> omega

theorem carried_least (want : Nat) (run : Cell) (h : want ≤ run.size) : want ≤ carried H A want run := by
  unfold carried; split <;> omega

theorem carried_most (want : Nat) (run : Cell) (hA : 0 < A ∨ 0 < H) :
    carried H A want run < want + H + A := by
  unfold carried; split <;> omega

theorem count_append (o : Nat) (a b : List Cell) : count o (a ++ b) = count o a + count o b := by
  induction a with
  | nil => simp [count]
  | cons c rest ih => simp [count, ih]; omega

theorem laid_count (want owner o : Nat) (run : Cell) (hfree : run.busy = false) :
    count o (laid H A want owner run) = count o [run] + (if owner = o then carried H A want run else 0) := by
  unfold laid carried
  split <;> by_cases ho : owner = o <;> simp [count, hfree, ho]

/-- The tier before a move: the cells below the run, the free run, the cells between, the busy cell, the rest. -/
def before (low : List Cell) (run : Cell) (mid : List Cell) (cell : Cell) (high : List Cell) : List Cell :=
  low ++ run :: mid ++ cell :: high

/-- The tier after: the cell laid over the run, its old place free. -/
def after (low : List Cell) (run : Cell) (mid : List Cell) (cell : Cell) (high : List Cell) : List Cell :=
  low ++ laid H A cell.size cell.owner run ++ mid ++ ⟨cell.size, false, 0⟩ :: high

/-- The running count the mover keeps: the count before, the cell's carried bytes past its own added. -/
def moved (counted : Nat) (o : Nat) (run cell : Cell) : Nat :=
  counted + (if cell.owner = o then carried H A cell.size run - cell.size else 0)

/-- The mover keeps the running count the layout holds. -/
theorem move_count (low mid high : List Cell) (run cell : Cell) (o : Nat)
    (hrun : run.busy = false) (hcell : cell.busy = true) (hfits : cell.size ≤ run.size) :
    count o (after H A low run mid cell high) = moved H A (count o (before low run mid cell high)) o run cell := by
  unfold after before moved
  simp only [count_append, List.cons_append, List.append_assoc]
  rw [laid_count H A cell.size cell.owner o run hrun]
  have hc : cell.size ≤ carried H A cell.size run := carried_least H A cell.size run hfits
  by_cases ho : cell.owner = o <;> simp [count, count_append, hrun, hcell, ho] <;> omega

end LocusCarcerum.Take
