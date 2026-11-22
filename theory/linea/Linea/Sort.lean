/-!
# Linea, the sort of memor

The sort of memor as a list of elements. Each element is the `bytes` bytes of one place of the region, and `le`
answers whether the order of the caller puts its first element at or before its second, as an order of 0 or less
does. The sort is bottom-up. A pass of width `w` cuts the list into blocks of `2 * w` from the low end, and it
merges the two halves of each block. A merge takes the left element where `le` holds of it and the right element,
and the right element otherwise, as `List.merge` does. The first pass has width 1, each later pass twice the width
of the one before, and the passes stop at the first width that is the length of the list or more.

The theorems. A pass keeps the elements of the list, and a pass over runs of its width that are each in order leaves
runs of twice the width that are each in order. The sort keeps the elements of the list, and it leaves them in order
where `le` is transitive and total.
-/

namespace Linea.Sort

variable {α : Type} (le : α → α → Bool)

/-- Whether `le` holds of every pair of the list in its order: the list is in the order of `le`. -/
def Sorted (l : List α) : Prop := l.Pairwise fun a b => le a b = true

/-- The block of a pass at the start of the list: its first `w` elements merged with the `w` after them. -/
def block (w : Nat) (l : List α) : List α := List.merge (l.take w) ((l.drop w).take w) le

/-- One pass of width `w`: the list cut into blocks of `2 * w`, the two halves of each block merged. -/
def mergePass (w : Nat) (hw : 0 < w) : List α → List α
  | [] => []
  | x :: xs => block le w (x :: xs) ++ mergePass w hw ((x :: xs).drop (2 * w))
termination_by l => l.length
decreasing_by simp only [List.length_drop, List.length_cons]; omega

/-- The passes of the sort: from width `2 ^ e`, while the width is below the length, at max `k` of them. -/
def sortPasses : Nat → Nat → List α → List α
  | 0, _, l => l
  | k + 1, e, l => if 2 ^ e < l.length then sortPasses k (e + 1) (mergePass le (2 ^ e) (Nat.two_pow_pos e) l) else l

/-- memor.sort: the passes from width 1. The length of the list bounds their count, as `2 ^ n` passes `n`. -/
def bytesSort (l : List α) : List α := sortPasses le l.length 0 l

/-- Every run of width `w` from the low end is in order. -/
def runsSorted (w : Nat) (l : List α) : Prop := ∀ i, Sorted le ((l.drop (i * w)).take w)

theorem mergePass_nil (w : Nat) (hw : 0 < w) : mergePass le w hw ([] : List α) = [] := by
  rw [mergePass]

theorem mergePass_cons (w : Nat) (hw : 0 < w) (x : α) (xs : List α) :
    mergePass le w hw (x :: xs) = block le w (x :: xs) ++ mergePass le w hw ((x :: xs).drop (2 * w)) := by
  rw [mergePass]

theorem block_length (w : Nat) (l : List α) : (block le w l).length = min (2 * w) l.length := by
  simp only [block, List.length_merge, List.length_take, List.length_drop]
  omega

/-! ## A pass keeps the elements -/

theorem mergePass_perm (w : Nat) (hw : 0 < w) : ∀ l : List α, List.Perm (mergePass le w hw l) l
  | [] => by rw [mergePass_nil]
  | x :: xs => by
    rw [mergePass_cons]
    have h1 : List.Perm (block le w (x :: xs)) ((x :: xs).take (2 * w)) := by
      have h3 : (x :: xs).take (2 * w) = (x :: xs).take w ++ ((x :: xs).drop w).take w := by
        rw [show 2 * w = w + w by omega, List.take_add]
      rw [h3]
      exact List.merge_perm_append le
    have h2 := mergePass_perm w hw ((x :: xs).drop (2 * w))
    have h4 := h1.append h2
    rwa [List.take_append_drop] at h4
termination_by l => l.length
decreasing_by simp only [List.length_drop, List.length_cons]; omega

theorem mergePass_length (w : Nat) (hw : 0 < w) (l : List α) : (mergePass le w hw l).length = l.length :=
  (mergePass_perm le w hw l).length_eq

/-! ## A pass doubles the runs that are in order -/

theorem mergePass_take (w : Nat) (hw : 0 < w) (l : List α) :
    (mergePass le w hw l).take (2 * w) = block le w l := by
  cases l with
  | nil => simp [mergePass_nil, block]
  | cons x xs =>
    rw [mergePass_cons, List.take_append, List.take_of_length_le (by rw [block_length]; omega)]
    by_cases hl : 2 * w ≤ (x :: xs).length
    · rw [show 2 * w - (block le w (x :: xs)).length = 0 by rw [block_length]; omega, List.take_zero,
        List.append_nil]
    · rw [List.drop_eq_nil_of_le (by omega : (x :: xs).length ≤ 2 * w), mergePass_nil, List.take_nil,
        List.append_nil]

theorem mergePass_drop (w : Nat) (hw : 0 < w) (l : List α) :
    (mergePass le w hw l).drop (2 * w) = mergePass le w hw (l.drop (2 * w)) := by
  cases l with
  | nil => simp [mergePass_nil]
  | cons x xs =>
    rw [mergePass_cons, List.drop_append, List.drop_eq_nil_of_le (by rw [block_length]; omega), List.nil_append]
    by_cases hl : 2 * w ≤ (x :: xs).length
    · rw [show 2 * w - (block le w (x :: xs)).length = 0 by rw [block_length]; omega, List.drop_zero]
    · rw [List.drop_eq_nil_of_le (by omega : (x :: xs).length ≤ 2 * w), mergePass_nil, List.drop_nil]

theorem mergePass_drop_mul (w : Nat) (hw : 0 < w) :
    ∀ (i : Nat) (l : List α), (mergePass le w hw l).drop (i * (2 * w)) = mergePass le w hw (l.drop (i * (2 * w)))
  | 0, l => by simp
  | i + 1, l => by
    rw [Nat.succ_mul, Nat.add_comm, ← List.drop_drop, mergePass_drop, mergePass_drop_mul w hw i, List.drop_drop]

theorem mergePass_runs (trans : ∀ a b c : α, le a b = true → le b c = true → le a c = true)
    (total : ∀ a b : α, (le a b || le b a) = true) (w : Nat) (hw : 0 < w) (l : List α)
    (h : runsSorted le w l) : runsSorted le (2 * w) (mergePass le w hw l) := by
  intro i
  rw [mergePass_drop_mul, mergePass_take]
  unfold block
  apply List.pairwise_merge trans total
  · have h1 := h (2 * i)
    rwa [show 2 * i * w = i * (2 * w) by rw [Nat.mul_comm 2 i, Nat.mul_assoc]] at h1
  · have h2 := h (2 * i + 1)
    rw [List.drop_drop]
    rwa [show (2 * i + 1) * w = i * (2 * w) + w by
      rw [Nat.add_mul, Nat.one_mul, Nat.mul_comm 2 i, Nat.mul_assoc]] at h2

/-! ## The sort -/

theorem runsSorted_one (l : List α) : runsSorted le 1 l := by
  intro i
  generalize l.drop (i * 1) = r
  cases r with
  | nil => simp [Sorted]
  | cons x xs => simp [Sorted]

theorem sorted_of_runs (w : Nat) (l : List α) (h : runsSorted le w l) (hl : l.length ≤ w) : Sorted le l := by
  have h0 := h 0
  rwa [Nat.zero_mul, List.drop_zero, List.take_of_length_le hl] at h0

theorem sortPasses_perm : ∀ (k e : Nat) (l : List α), List.Perm (sortPasses le k e l) l
  | 0, _, l => by simp [sortPasses]
  | k + 1, e, l => by
    simp only [sortPasses]
    split
    · exact (sortPasses_perm k (e + 1) _).trans (mergePass_perm le _ _ l)
    · exact List.Perm.refl l

theorem sortPasses_sorted (trans : ∀ a b c : α, le a b = true → le b c = true → le a c = true)
    (total : ∀ a b : α, (le a b || le b a) = true) :
    ∀ (k e : Nat) (l : List α), runsSorted le (2 ^ e) l → l.length ≤ 2 ^ (e + k) → Sorted le (sortPasses le k e l)
  | 0, e, l, h, hl => by
    simp only [sortPasses]
    exact sorted_of_runs le (2 ^ e) l h (by simpa using hl)
  | k + 1, e, l, h, hl => by
    simp only [sortPasses]
    split
    · apply sortPasses_sorted trans total k (e + 1)
      · have h1 := mergePass_runs le trans total (2 ^ e) (Nat.two_pow_pos e) l h
        rwa [show 2 * 2 ^ e = 2 ^ (e + 1) by rw [Nat.pow_succ, Nat.mul_comm]] at h1
      · rw [mergePass_length, show e + 1 + k = e + (k + 1) by omega]
        exact hl
    · exact sorted_of_runs le (2 ^ e) l h (by omega)

/-- The sort keeps the elements of the list. -/
theorem bytesSort_perm (l : List α) : List.Perm (bytesSort le l) l := sortPasses_perm le l.length 0 l

/-- The sort leaves the list in the order of `le`, where `le` is transitive and total. -/
theorem bytesSort_sorted (trans : ∀ a b c : α, le a b = true → le b c = true → le a c = true)
    (total : ∀ a b : α, (le a b || le b a) = true) (l : List α) : Sorted le (bytesSort le l) :=
  sortPasses_sorted le trans total l.length 0 l (by simpa using runsSorted_one le l)
    (by rw [Nat.zero_add]; exact Nat.le_of_lt Nat.lt_two_pow_self)

/-- The sort keeps the length of the list. -/
theorem bytesSort_length (l : List α) : (bytesSort le l).length = l.length := (bytesSort_perm le l).length_eq

end Linea.Sort
