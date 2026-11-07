/-!
# LocusCarcerum, the rewind of the temporary tier

The temporary tier is its cells laid from the top up to the cellblock's size, each a footprint of `H` and its
payload. A rewind to a mark (`carcer_rewound`) walks a cell at a time from the top while the walk stands below the
mark, and stores the top where the walk stops.

The theorems: the walk stops on a cell boundary, the rest of the tier standing from there; it stops at or past the
mark where the mark lies within the tier; the cells it drops are the ones that start below the mark; and a mark
standing on a boundary, as a mark handed out stands, is where it stops.

`old_inside` is the rewind that stored the mark itself: a mark the top was trimmed past, with a cell laid across
it later, stores a top inside that cell, and the next walk reads payload bytes as a header.
-/

namespace LocusCarcerum.Rewind

variable (H : Nat)

/-- The bytes a cell of `size` payload lays. -/
def foot (size : Nat) : Nat := H + size

/-- The tier's bytes from its top. -/
def len : List Nat → Nat
  | [] => 0
  | s :: rest => foot H s + len rest

/-- The walk from `off` over `cells` while it stands below `mark`: where it stops, and the cells left. -/
def walk (mark : Nat) : Nat → List Nat → Nat × List Nat
  | off, [] => (off, [])
  | off, s :: rest => if off < mark then walk mark (off + foot H s) rest else (off, s :: rest)

/-- The offsets the cells of a tier start at, the first at `off`. -/
def starts : Nat → List Nat → List Nat
  | _, [] => []
  | off, s :: rest => off :: starts (off + foot H s) rest

/-- The walk stops where the cells it leaves start: the store puts the top on a cell boundary, and the end of the
tier stands where it stood. -/
theorem walk_boundary (mark off : Nat) (cells : List Nat) :
    (walk H mark off cells).1 + len H (walk H mark off cells).2 = off + len H cells := by
  induction cells generalizing off with
  | nil => simp [walk, len]
  | cons s rest ih =>
    simp only [walk]
    split
    · rw [ih]; simp [len]; omega
    · simp [len]

/-- The walk stops at or past the mark where the mark lies within the tier. -/
theorem walk_past (mark off : Nat) (cells : List Nat) (h : mark ≤ off + len H cells) :
    mark ≤ (walk H mark off cells).1 := by
  induction cells generalizing off with
  | nil => simp [walk, len] at h ⊢; exact h
  | cons s rest ih =>
    simp only [walk]
    split
    · exact ih _ (by simp [len] at h; omega)
    · omega

/-- The walk never goes back below where it started. -/
theorem walk_ge (mark off : Nat) (cells : List Nat) : off ≤ (walk H mark off cells).1 := by
  induction cells generalizing off with
  | nil => simp [walk]
  | cons s rest ih =>
    simp only [walk]
    split
    · exact Nat.le_trans (by unfold foot; omega) (ih _)
    · exact Nat.le_refl _

/-- The cells left all start at or past the mark: every cell starting below it went with the rewind. -/
theorem walk_left_past (mark off : Nat) (cells : List Nat) :
    ∀ x ∈ starts H (walk H mark off cells).1 (walk H mark off cells).2, mark ≤ x ∨ (walk H mark off cells).2 = [] := by
  induction cells generalizing off with
  | nil => intro x hx; simp [walk, starts] at hx
  | cons s rest ih =>
    simp only [walk]
    split
    · exact ih _
    · next hlt =>
      intro x hx
      left
      simp only [starts, List.mem_cons] at hx
      rcases hx with hx | hx
      · omega
      · -- every later start lies past the first, which is at or past the mark
        have key : ∀ (o : Nat) (l : List Nat), ∀ y ∈ starts H o l, o ≤ y := by
          intro o l
          induction l generalizing o with
          | nil => intro y hy; simp [starts] at hy
          | cons t r ihr =>
            intro y hy
            simp only [starts, List.mem_cons] at hy
            rcases hy with hy | hy
            · omega
            · exact Nat.le_trans (by unfold foot; omega) (ihr _ y hy)
        have := key _ _ x hx
        unfold foot at this
        omega

/-- A mark on a boundary is where the walk stops. -/
theorem walk_on_boundary (hH : 0 < H) (off : Nat) (front back : List Nat) :
    (walk H (off + len H front) off (front ++ back)).1 = off + len H front := by
  induction front generalizing off with
  | nil =>
    cases back with
    | nil => simp [walk, len]
    | cons s rest => simp [walk, len]
  | cons s rest ih =>
    simp only [List.cons_append, walk]
    have hlt : off < off + len H (s :: rest) := by simp [len, foot]; omega
    simp only [hlt, ite_true]
    have := ih (off + foot H s)
    simp [len] at this ⊢
    rw [show off + (foot H s + len H rest) = off + foot H s + len H rest by omega]
    exact this

/-- The tier the stress run read: a header of 16, cells of 248 and 80 from a top at 16024. A mark of 16144 the top
was trimmed past lies inside the first. -/
def tier : List Nat := [248, 80]

/-- The rewind as the C stores it: at the walk's stop, the end of the cell the mark lies in. -/
theorem new_boundary : (walk 16 16144 16024 tier).1 = 16288 := by decide

/-- The rewind that stored the mark: 16144 is no cell's start, and the next walk reads a header there. -/
theorem old_inside : 16144 ∉ starts 16 16024 tier := by decide

end LocusCarcerum.Rewind
