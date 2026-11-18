import LocusCarcerum.Layout
import LocusCarcerum.Temporary

/-!
# LocusCarcerum, a temporary stack for each thread

The temporary tier is a stack of cells from its top down, as `Temporary` lays it. A take puts its cell on the top. A
give sets the newest busy cell of its owner free, and the free run at the top then returns to the gap
(`Temporary.trimStart`). The depth of a stack is the bytes it positions, `Layout.len`.

Each thread here gives its newest cell before it takes the next one. That is the order of a call that takes a cell,
uses it and gives it back.

The theorems in two parts. One stack that two such threads share sinks without a bound. After `n` rounds it is
`2 * n + 2` cells deep, and two of them are busy. Each give of a thread finds the cell of the other thread on top of
its own, and so no give returns a run to the gap.

A stack for each thread does not sink. An operation of a thread changes its own stack and no other. Each cell of a
stack is busy and belongs to the thread of the stack, after any interleaving of takes and gives. So the depth of each
stack is the footprint of its live cells and no more. The two threads of the first part keep one cell each.
-/

namespace LocusCarcerum.Stacks

open LocusCarcerum.Layout
open LocusCarcerum.Temporary

variable (H : Nat)

/-- A cell with its busy flag clear. -/
def freed (c : Cell) : Cell := { c with busy := false }

/-- The newest busy cell of `owner` set free. The cells before it and after it stay as they are. -/
def giveNewest (owner : Nat) : List Cell → List Cell
  | [] => []
  | c :: rest => if c.busy = true ∧ c.owner = owner then freed c :: rest else c :: giveNewest owner rest

/-- A take of `want` bytes by `owner`: its cell on the top of the stack. -/
def take (want owner : Nat) (stack : List Cell) : List Cell := ⟨want, true, owner⟩ :: stack

/-- A give by `owner`: its newest busy cell set free, then the free run at the top returned to the gap. -/
def give (owner : Nat) (stack : List Cell) : List Cell := trimStart (giveNewest owner stack)

/-! ## One stack that two threads share -/

/-- One round of the two threads on one shared stack. Thread 0 gives and takes, then thread 1 gives and takes. -/
def sharedRound (want : Nat) (stack : List Cell) : List Cell :=
  take want 1 (give 1 (take want 0 (give 0 stack)))

/-- The free cells that `n` rounds leave under the two live cells. -/
def freedRounds (want : Nat) : Nat → List Cell
  | 0 => []
  | n + 1 => ⟨want, false, 1⟩ :: ⟨want, false, 0⟩ :: freedRounds want n

/-- The shared stack after `n` rounds, from one live cell for each thread. -/
def sharedAfter (want : Nat) : Nat → List Cell
  | 0 => [⟨want, true, 1⟩, ⟨want, true, 0⟩]
  | n + 1 => sharedRound want (sharedAfter want n)

/-- A round on the shared stack frees the two live cells in place and puts two new ones on the top. -/
theorem sharedRound_shape (want : Nat) (rest : List Cell) :
    sharedRound want (⟨want, true, 1⟩ :: ⟨want, true, 0⟩ :: rest) =
      ⟨want, true, 1⟩ :: ⟨want, true, 0⟩ :: ⟨want, false, 1⟩ :: ⟨want, false, 0⟩ :: rest := by
  simp [sharedRound, take, give, giveNewest, freed, trimStart]

/-- The shared stack after `n` rounds: the two live cells on top of the free cells of each round. -/
theorem sharedAfter_shape (want n : Nat) :
    sharedAfter want n = ⟨want, true, 1⟩ :: ⟨want, true, 0⟩ :: freedRounds want n := by
  induction n with
  | zero => rfl
  | succ n ih => simp only [sharedAfter, ih, sharedRound_shape, freedRounds]

theorem freedRounds_len (want n : Nat) : len H (freedRounds want n) = 2 * n * (H + want) := by
  induction n with
  | zero => simp [freedRounds, len]
  | succ n ih =>
    simp only [freedRounds, len, foot, ih]
    rw [show 2 * (n + 1) = 2 * n + 2 by omega, Nat.add_mul]
    omega

theorem freedRounds_busy (want n : Nat) : busyCells (freedRounds want n) = [] := by
  induction n with
  | zero => rfl
  | succ n ih => simp [freedRounds, busyCells, ih]

/-- The shared stack is `2 * n + 2` cells deep after `n` rounds. -/
theorem sharedAfter_len (want n : Nat) : len H (sharedAfter want n) = (2 * n + 2) * (H + want) := by
  rw [sharedAfter_shape]
  simp only [len, foot, freedRounds_len]
  rw [Nat.add_mul]
  omega

/-- Two cells of the shared stack are busy after each round, the newest cell of each thread. -/
theorem sharedAfter_busy (want n : Nat) :
    busyCells (sharedAfter want n) = [⟨want, true, 1⟩, ⟨want, true, 0⟩] := by
  rw [sharedAfter_shape]
  simp [busyCells, freedRounds_busy]

/-- The shared stack sinks without a bound: for each depth, a count of rounds goes past it, and the live cells stay
two. A header of at least one byte makes each cell take room. -/
theorem shared_sinks (want depth : Nat) (hH : 0 < H) :
    ∃ n, depth < len H (sharedAfter want n) ∧ (busyCells (sharedAfter want n)).length = 2 := by
  refine ⟨depth, ?_, by rw [sharedAfter_busy]; rfl⟩
  rw [sharedAfter_len]
  have hw : 1 ≤ H + want := by omega
  calc depth < 2 * depth + 2 := by omega
    _ = (2 * depth + 2) * 1 := (Nat.mul_one _).symm
    _ ≤ (2 * depth + 2) * (H + want) := Nat.mul_le_mul_left _ hw

/-! ## A stack for each thread -/

/-- The stacks, one for each thread. -/
abbrev Stacks := Nat → List Cell

/-- The stacks with the stack of `thread` replaced by `stack`. -/
def put (stacks : Stacks) (thread : Nat) (stack : List Cell) : Stacks :=
  fun other => if other = thread then stack else stacks other

/-- An operation of a thread on its own stack. -/
inductive Op where
  | take (want : Nat)
  | give
  deriving DecidableEq, Repr

/-- One operation of `thread`, on its own stack. -/
def step (stacks : Stacks) (thread : Nat) : Op → Stacks
  | .take want => put stacks thread (take want thread (stacks thread))
  | .give => put stacks thread (give thread (stacks thread))

/-- A run of operations, each of one thread, in the order the threads make them. -/
def run (stacks : Stacks) : List (Nat × Op) → Stacks
  | [] => stacks
  | (thread, op) :: rest => run (step stacks thread op) rest

/-- An operation of one thread leaves the stack of each other thread as it was. -/
theorem step_other (stacks : Stacks) (thread other : Nat) (op : Op) (h : other ≠ thread) :
    step stacks thread op other = stacks other := by
  cases op <;> simp [step, put, h]

/-- Each cell of a stack is busy and belongs to the thread of the stack. -/
def Owned (thread : Nat) (stack : List Cell) : Prop := ∀ c ∈ stack, c.busy = true ∧ c.owner = thread

theorem trimStart_owned (thread : Nat) (stack : List Cell) (h : Owned thread stack) : trimStart stack = stack := by
  cases stack with
  | nil => rfl
  | cons c rest =>
    have hc := (h c (List.mem_cons_self ..)).1
    simp [trimStart, hc]

/-- A give on a stack that its thread owns sets its top cell free and returns it to the gap. -/
theorem give_owned (thread : Nat) (stack : List Cell) (h : Owned thread stack) :
    give thread stack = stack.tail := by
  cases stack with
  | nil => rfl
  | cons c rest =>
    have hc := h c (List.mem_cons_self ..)
    have hrest : Owned thread rest := fun d hd => h d (List.mem_cons_of_mem _ hd)
    simp only [give, giveNewest, hc, and_self, ite_true, freed, trimStart, List.tail]
    exact trimStart_owned thread rest hrest

theorem take_owned (thread want : Nat) (stack : List Cell) (h : Owned thread stack) :
    Owned thread (take want thread stack) := by
  intro c hc
  simp only [take, List.mem_cons] at hc
  rcases hc with rfl | hc
  · exact ⟨rfl, rfl⟩
  · exact h c hc

theorem tail_owned (thread : Nat) (stack : List Cell) (h : Owned thread stack) : Owned thread stack.tail := by
  cases stack with
  | nil => exact h
  | cons c rest => exact fun d hd => h d (List.mem_cons_of_mem _ hd)

/-- Each stack is owned by its thread. -/
def AllOwned (stacks : Stacks) : Prop := ∀ thread, Owned thread (stacks thread)

theorem step_allOwned (stacks : Stacks) (thread : Nat) (op : Op) (h : AllOwned stacks) :
    AllOwned (step stacks thread op) := by
  intro other
  by_cases ho : other = thread
  · subst ho
    cases op with
    | take want => simpa [step, put] using take_owned other want (stacks other) (h other)
    | give =>
      simp only [step, put, ite_true]
      rw [give_owned other (stacks other) (h other)]
      exact tail_owned other (stacks other) (h other)
  · rw [step_other stacks thread other op ho]
    exact h other

/-- Each stack stays owned by its thread after any interleaving of takes and gives. -/
theorem run_allOwned (stacks : Stacks) (ops : List (Nat × Op)) (h : AllOwned stacks) : AllOwned (run stacks ops) := by
  induction ops generalizing stacks with
  | nil => exact h
  | cons op rest ih => exact ih _ (step_allOwned stacks op.1 op.2 h)

theorem owned_busy (thread : Nat) (stack : List Cell) (h : Owned thread stack) : busyCells stack = stack := by
  induction stack with
  | nil => rfl
  | cons c rest ih =>
    have hc := (h c (List.mem_cons_self ..)).1
    simp [busyCells, hc, ih (fun d hd => h d (List.mem_cons_of_mem _ hd))]

/-- No stack sinks: after any interleaving from empty stacks, the depth of each stack is the footprint of its live
cells. -/
theorem run_depth (ops : List (Nat × Op)) (thread : Nat) :
    len H (run (fun _ => []) ops thread) = len H (busyCells (run (fun _ => []) ops thread)) := by
  have h := run_allOwned (fun _ => []) ops (fun _ c hc => absurd hc (List.not_mem_nil))
  rw [owned_busy thread _ (h thread)]

/-- The round of the shared part, on a stack for each thread. -/
def ownRound (want : Nat) (stacks : Stacks) : Stacks :=
  step (step (step (step stacks 0 .give) 0 (.take want)) 1 .give) 1 (.take want)

/-- The stacks after `n` rounds, from one live cell for each thread. -/
def ownAfter (want : Nat) : Nat → Stacks
  | 0 => fun thread => [⟨want, true, thread⟩]
  | n + 1 => ownRound want (ownAfter want n)

/-- On a stack for each thread, the two threads of the shared part keep one cell each after each round. -/
theorem ownAfter_shape (want n : Nat) : ownAfter want n 0 = [⟨want, true, 0⟩] ∧ ownAfter want n 1 = [⟨want, true, 1⟩] := by
  induction n with
  | zero => exact ⟨rfl, rfl⟩
  | succ n ih =>
    obtain ⟨h0, h1⟩ := ih
    constructor <;> simp [ownAfter, ownRound, step, put, take, give, giveNewest, freed, trimStart, h0, h1]

/-- Each of the two stacks is one cell deep after each round. -/
theorem ownAfter_len (want n thread : Nat) (h : thread = 0 ∨ thread = 1) :
    len H (ownAfter want n thread) = H + want := by
  obtain ⟨h0, h1⟩ := ownAfter_shape want n
  rcases h with rfl | rfl
  · simp [h0, len, foot]
  · simp [h1, len, foot]

end LocusCarcerum.Stacks
