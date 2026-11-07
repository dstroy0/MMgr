/-!
# LocusCarcerum, the persistent tier as its cells

The persistent tier is its cells laid end to end from offset 0: each cell a header, its payload and its owner
word, `H` the bytes of the header and the owner word together. A cell is busy or free, and a busy cell counts
its payload to its owner. The end is the sum of the cells' footprints.

The sweep (`carcer_swept`) merges every run of free cells into one free cell, the run's footprints summed, and
returns a free run at the end to the gap. The theorems: the merge keeps every byte laid, the busy cells in
order and every owner's count; after the merge no two free cells stand together; the trim leaves no free cell
at the end, keeps the busy cells and the counts, and the end never rises.
-/

namespace LocusCarcerum.Layout

/-- A cell: its payload's bytes, whether it is busy, and the owner it counts to. -/
structure Cell where
  size : Nat
  busy : Bool
  owner : Nat
  deriving DecidableEq, Repr

variable (H : Nat)

/-- The bytes a cell lays: its header and owner word, and its payload. -/
def foot (c : Cell) : Nat := H + c.size

/-- The bytes a tier lays, which is the persistent end. -/
def len : List Cell → Nat
  | [] => 0
  | c :: rest => foot H c + len rest

/-- The busy cells, in order. -/
def busyCells : List Cell → List Cell
  | [] => []
  | c :: rest => if c.busy then c :: busyCells rest else busyCells rest

/-- An owner's count: the payload of every busy cell it owns. -/
def count (o : Nat) : List Cell → Nat
  | [] => 0
  | c :: rest => (if c.busy ∧ c.owner = o then c.size else 0) + count o rest

/-- The free cell two free cells merge into: both footprints laid as one. -/
def joined (a b : Cell) : Cell := ⟨a.size + H + b.size, false, 0⟩

/-- Every run of free cells merged into one free cell. -/
def merge : List Cell → List Cell
  | [] => []
  | c :: rest =>
    match merge rest with
    | [] => [c]
    | d :: tail => if !c.busy && !d.busy then joined H c d :: tail else c :: d :: tail

/-- No two free cells stand together. -/
def separated : List Cell → Prop
  | [] => True
  | [_] => True
  | a :: b :: rest => (a.busy = true ∨ b.busy = true) ∧ separated (b :: rest)

/-- The last cell, where there is one, is busy. -/
def endsBusy : List Cell → Prop
  | [] => True
  | [c] => c.busy = true
  | _ :: b :: rest => endsBusy (b :: rest)

/-- The free cell at the end returned to the gap. -/
def trimEnd : List Cell → List Cell
  | [] => []
  | c :: rest =>
    match trimEnd rest with
    | [] => if c.busy then [c] else []
    | d :: tail => c :: d :: tail

/-- The sweep: the merge, then the trim. -/
def sweep (l : List Cell) : List Cell := trimEnd (merge H l)

theorem merge_len (l : List Cell) : len H (merge H l) = len H l := by
  induction l with
  | nil => rfl
  | cons c rest ih =>
    simp only [merge]
    split
    · next h => rw [h] at ih; simp [len] at ih ⊢; omega
    · next d tail h =>
      rw [h] at ih
      split
      · simp [len, joined, foot] at ih ⊢; omega
      · simp [len] at ih ⊢; omega

theorem merge_busy (l : List Cell) : busyCells (merge H l) = busyCells l := by
  induction l with
  | nil => rfl
  | cons c rest ih =>
    simp only [merge]
    split
    · next h => rw [h] at ih; simp [busyCells] at ih ⊢; simp [ih]
    · next d tail h =>
      rw [h] at ih
      split
      · next hf =>
        simp at hf
        simp [busyCells, joined, hf.1, hf.2] at ih ⊢
        exact ih
      · simp [busyCells] at ih ⊢; rw [← ih]

theorem count_of_busy (o : Nat) (l : List Cell) : count o l = count o (busyCells l) := by
  induction l with
  | nil => rfl
  | cons c rest ih =>
    cases hc : c.busy <;> simp [count, busyCells, hc, ih]

theorem merge_count (o : Nat) (l : List Cell) : count o (merge H l) = count o l := by
  rw [count_of_busy, merge_busy, ← count_of_busy]

/-- The head of a merge is the list's head or a free cell joined from it. -/
theorem merge_head_busy (c : Cell) (rest : List Cell) (d : Cell) (tail : List Cell)
    (h : merge H (c :: rest) = d :: tail) : d.busy = c.busy ∨ (d.busy = false ∧ c.busy = false) := by
  simp only [merge] at h
  split at h
  · simp at h; left; rw [h.1]
  · split at h
    · next hf => simp at h hf; right; rw [← h.1]; exact ⟨rfl, hf.1⟩
    · simp at h; left; rw [h.1]

theorem merge_separated (l : List Cell) : separated (merge H l) := by
  induction l with
  | nil => trivial
  | cons c rest ih =>
    simp only [merge]
    split
    · trivial
    · next d tail h =>
      rw [h] at ih
      split
      · next hf =>
        -- the joined cell is free, and what follows it was separated from the free d
        simp at hf
        cases tail with
        | nil => trivial
        | cons e tail' =>
          simp [separated] at ih ⊢
          refine ⟨?_, ih.2⟩
          rcases ih.1 with hd | he
          · rw [hf.2] at hd; cases hd
          · exact Or.inr he
      · next hf =>
        refine ⟨?_, ih⟩
        cases hc : c.busy
        · cases hd : d.busy
          · simp [hc, hd] at hf
          · exact Or.inr rfl
        · exact Or.inl rfl

theorem trimEnd_ends (l : List Cell) : endsBusy (trimEnd l) := by
  induction l with
  | nil => trivial
  | cons c rest ih =>
    simp only [trimEnd]
    split
    · split
      · next hc => exact hc
      · trivial
    · next d tail h => rw [h] at ih; exact ih

theorem trimEnd_busy (l : List Cell) : busyCells (trimEnd l) = busyCells l := by
  induction l with
  | nil => rfl
  | cons c rest ih =>
    simp only [trimEnd]
    split
    · next h =>
      rw [h] at ih
      split
      · next hc => simp [busyCells, hc] at ih ⊢; exact ih
      · next hc => simp [busyCells, hc] at ih ⊢; exact ih
    · next d tail h => rw [h] at ih; simp [busyCells] at ih ⊢; rw [← ih]

theorem trimEnd_len (l : List Cell) : len H (trimEnd l) ≤ len H l := by
  induction l with
  | nil => exact Nat.le_refl _
  | cons c rest ih =>
    simp only [trimEnd]
    split
    · next h =>
      rw [h] at ih
      split
      · simp only [len] at ih ⊢; omega
      · simp [len]
    · next d tail h => rw [h] at ih; simp [len] at ih ⊢; omega

theorem sweep_busy (l : List Cell) : busyCells (sweep H l) = busyCells l := by
  unfold sweep; rw [trimEnd_busy, merge_busy]

theorem sweep_count (o : Nat) (l : List Cell) : count o (sweep H l) = count o l := by
  rw [count_of_busy, sweep_busy, ← count_of_busy]

theorem sweep_len (l : List Cell) : len H (sweep H l) ≤ len H l := by
  unfold sweep
  exact Nat.le_trans (trimEnd_len H _) (Nat.le_of_eq (merge_len H l))

theorem sweep_ends (l : List Cell) : endsBusy (sweep H l) := trimEnd_ends _

end LocusCarcerum.Layout
