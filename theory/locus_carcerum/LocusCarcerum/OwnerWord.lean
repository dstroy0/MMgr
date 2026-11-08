import Std.Tactic.BVDecide

/-!
# LocusCarcerum, the owner word and the undo of one owner's work

A cell carries an owner word behind its payload. Its fields, from high to low: the owner's place in
`MMGR_OWNER_PLACE_BITS` bits, the completion bit, three signaling bits, and the owner's sequence in the bits left.
The sequence is the old value of a `fetch_add` on a counter of the owner, one for each take. So the takes of one
owner have sequences one after the other, in the order the counter gives them.

The pair (place, sequence) names each take. An undo of one owner's work back to a mark gives back the owner's cells
whose sequence is after the mark, and keeps each other cell. Which cells those are depends on the cells alone, and
on no time or schedule.

The sequence rolls in its field of b bits. A cell is after the mark where its distance from the mark, modulo 2^b,
is above 0 and below 2^(b - 1). That reads the order exactly while fewer than 2^(b - 1) takes of the owner come
between the mark and the undo.

The theorems:

- `fields_64` and `fields_32`: the fields at 1024 owners, ten place bits, do not overlap and fill the word.
- `word_fields_64` and `word_fields_32`: a word built from a place and a sequence gives both back.
- `after_exact` and `before_exact`: a take d steps after the mark, d below 2^(b - 1), reads after it, and a take at
  or before the mark reads not after it.
- `undo_keeps_others`, `undo_removes`: the undo keeps each cell of another owner and each cell of the owner at or
  before the mark, and gives back each other cell.
- `undos_commute`: the undos of two owners give the same cells in either order.
-/

namespace LocusCarcerum.OwnerWord

/-! ## The fields -/

/-- The fields at a word of w bits with p place bits: the sequence's bits are the w - p - 4 low ones. -/
def sequenceBits (w p : Nat) : Nat := w - p - 4

def placeMask64 : BitVec 64 := ((1 <<< 10) - 1) <<< 54
def complete64 : BitVec 64 := 1 <<< 53
def signal64 : BitVec 64 := 7 <<< 50
def sequenceMask64 : BitVec 64 := (1 <<< 50) - 1

def placeMask32 : BitVec 32 := ((1 <<< 10) - 1) <<< 22
def complete32 : BitVec 32 := 1 <<< 21
def signal32 : BitVec 32 := 7 <<< 18
def sequenceMask32 : BitVec 32 := (1 <<< 18) - 1

theorem sequence_bits : sequenceBits 64 10 = 50 ∧ sequenceBits 32 10 = 18 := by decide

theorem fields_64 :
    placeMask64 &&& complete64 = 0 ∧ placeMask64 &&& signal64 = 0 ∧ placeMask64 &&& sequenceMask64 = 0 ∧
      complete64 &&& signal64 = 0 ∧ complete64 &&& sequenceMask64 = 0 ∧ signal64 &&& sequenceMask64 = 0 ∧
      (placeMask64 ||| complete64 ||| signal64 ||| sequenceMask64) = ~~~0 := by
  simp only [placeMask64, complete64, signal64, sequenceMask64]
  bv_decide

theorem fields_32 :
    placeMask32 &&& complete32 = 0 ∧ placeMask32 &&& signal32 = 0 ∧ placeMask32 &&& sequenceMask32 = 0 ∧
      complete32 &&& signal32 = 0 ∧ complete32 &&& sequenceMask32 = 0 ∧ signal32 &&& sequenceMask32 = 0 ∧
      (placeMask32 ||| complete32 ||| signal32 ||| sequenceMask32) = ~~~0 := by
  simp only [placeMask32, complete32, signal32, sequenceMask32]
  bv_decide

/-- The word of a take: the place in its field, no completion, no signal, and the sequence's low bits. -/
def word64 (place sequence : BitVec 64) : BitVec 64 := (place <<< 54) ||| (sequence &&& sequenceMask64)
def word32 (place sequence : BitVec 32) : BitVec 32 := (place <<< 22) ||| (sequence &&& sequenceMask32)

theorem word_fields_64 (place sequence : BitVec 64) (h : place < 1024) :
    (word64 place sequence &&& placeMask64) >>> 54 = place ∧
      word64 place sequence &&& sequenceMask64 = sequence &&& sequenceMask64 ∧
      word64 place sequence &&& (complete64 ||| signal64) = 0 := by
  simp only [word64, placeMask64, sequenceMask64, complete64, signal64]
  bv_decide

theorem word_fields_32 (place sequence : BitVec 32) (h : place < 1024) :
    (word32 place sequence &&& placeMask32) >>> 22 = place ∧
      word32 place sequence &&& sequenceMask32 = sequence &&& sequenceMask32 ∧
      word32 place sequence &&& (complete32 ||| signal32) = 0 := by
  simp only [word32, placeMask32, sequenceMask32, complete32, signal32]
  bv_decide

/-! ## The order of one owner's takes -/

/-- A sequence after a mark: its distance from the mark, modulo 2^b, above 0 and below 2^(b - 1). -/
def after {b : Nat} (sequence mark : BitVec b) : Bool :=
  0 < (sequence - mark).toNat && (sequence - mark).toNat < 2 ^ (b - 1)

/-- A take d steps after the mark, with d below 2^(b - 1), has distance d. -/
theorem distance_exact {b : Nat} (mark : BitVec b) (d : Nat) (hd : d < 2 ^ (b - 1)) (hb : 0 < b) :
    (mark + BitVec.ofNat b d - mark).toNat = d := by
  have hlt : d < 2 ^ b :=
    Nat.lt_of_lt_of_le hd (Nat.pow_le_pow_right (by decide) (by omega))
  rw [BitVec.add_comm, BitVec.add_sub_cancel, BitVec.toNat_ofNat, Nat.mod_eq_of_lt hlt]

/-- A take from 1 to 2^(b - 1) - 1 steps after the mark reads after it. -/
theorem after_exact {b : Nat} (mark : BitVec b) (d : Nat) (h0 : 0 < d) (hd : d < 2 ^ (b - 1)) (hb : 0 < b) :
    after (mark + BitVec.ofNat b d) mark = true := by
  simp only [after, distance_exact mark d hd hb, Bool.and_eq_true, decide_eq_true_eq]
  exact ⟨h0, hd⟩

/-- The take at the mark does not read after it. -/
theorem mark_not_after {b : Nat} (mark : BitVec b) : after mark mark = false := by
  simp [after]

/-- A take from 1 to 2^(b - 1) steps before the mark does not read after it. -/
theorem before_exact {b : Nat} (mark : BitVec b) (d : Nat) (h0 : 0 < d) (hd : d ≤ 2 ^ (b - 1)) (hb : 0 < b) :
    after (mark - BitVec.ofNat b d) mark = false := by
  have hhalf : 2 ^ (b - 1) * 2 = 2 ^ b := by
    rw [← Nat.pow_succ]
    congr 1
    omega
  have hlt : d < 2 ^ b := by omega
  have hsub : mark - BitVec.ofNat b d - mark = -BitVec.ofNat b d := by
    calc mark - BitVec.ofNat b d - mark = -BitVec.ofNat b d + (mark - mark) := by
          simp only [BitVec.sub_eq_add_neg]
          ac_rfl
      _ = -BitVec.ofNat b d := by simp
  simp only [after, hsub, BitVec.toNat_neg, BitVec.toNat_ofNat, Nat.mod_eq_of_lt hlt, Bool.and_eq_false_iff,
    decide_eq_false_iff_not, Nat.not_lt]
  right
  rw [Nat.mod_eq_of_lt (by omega)]
  omega

/-! ## The undo -/

/-- A cell: its owner's place and its sequence. -/
structure Cell (b : Nat) where
  place : Nat
  sequence : BitVec b
  deriving DecidableEq

/-- The cells an undo keeps: each cell but those of the owner after the mark. -/
def undo {b : Nat} (owner : Nat) (mark : BitVec b) (cells : List (Cell b)) : List (Cell b) :=
  cells.filter fun c => !(c.place == owner && after c.sequence mark)

theorem undo_keeps_others {b : Nat} (owner : Nat) (mark : BitVec b) (cells : List (Cell b)) (c : Cell b)
    (hc : c ∈ cells) (ho : c.place ≠ owner) : c ∈ undo owner mark cells := by
  simp only [undo, List.mem_filter]
  exact ⟨hc, by simp [ho]⟩

theorem undo_keeps_before {b : Nat} (owner : Nat) (mark : BitVec b) (cells : List (Cell b)) (c : Cell b)
    (hc : c ∈ cells) (ha : after c.sequence mark = false) : c ∈ undo owner mark cells := by
  simp only [undo, List.mem_filter]
  exact ⟨hc, by simp [ha]⟩

theorem undo_removes {b : Nat} (owner : Nat) (mark : BitVec b) (cells : List (Cell b)) (c : Cell b)
    (ho : c.place = owner) (ha : after c.sequence mark = true) : c ∉ undo owner mark cells := by
  simp [undo, ho, ha]

/-- The undos of two owners keep the same cells in either order. -/
theorem undos_commute {b : Nat} (o1 o2 : Nat) (m1 m2 : BitVec b) (cells : List (Cell b)) :
    undo o1 m1 (undo o2 m2 cells) = undo o2 m2 (undo o1 m1 cells) := by
  simp only [undo, List.filter_filter]
  congr 1
  funext c
  exact Bool.and_comm _ _

end LocusCarcerum.OwnerWord
