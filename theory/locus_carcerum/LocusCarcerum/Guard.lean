import LocusCarcerum.Bounds

/-!
# LocusCarcerum, the guard and the record behind a token

The caller holds a guard and tokens. A token is a const identity, and the caller never holds the internal pointer that
it names. The guard takes the token and passes it to the record behind it. The record holds the internal state: the
cell base, the element stride, the count, the capacity and the neighbor start. The record decides if an operation
is legal, and one CAS on its state word commits it, as the movable option does.

Each check is bit math on the atomic word of the host, of `W` bits. No mask and no word has a bit past `W`.

* One CAS: a CAS commits only from the word it read, and a CAS that does not commit changes nothing. Of two CAS from
  the same word, one commits.

* The claim word: a claim tests `~word & mask == mask`, and the CAS sets `word | mask`. Two claims that both commit
  hold masks with no bit in common, and a release clears the bits of its claim.

* The bit of an element: element `n` has bit `n % W` in word `n / W`. Two elements share a bit only where they are
  the same element.

* The decay: `vec[n]` is the address `base + n * stride` and the span of one stride from it. The span test of the
  movable option (`Bounds.fits`) passes it only where `n` is below the count. A span that the test passes lies in the
  cell and stops at or before the neighbor. The spans of two elements do not overlap.
-/

namespace LocusCarcerum.Guard

/-! ## One CAS -/

/-- A compare and swap: the new word where the word is the expected one, and the word as it was where it is not.
The flag tells which. -/
def cas {β : Type} [DecidableEq β] (word expected new : β) : β × Bool :=
  if word = expected then (new, true) else (word, false)

theorem cas_commits {β : Type} [DecidableEq β] (word expected new : β) :
    (cas word expected new).2 = true ↔ word = expected := by
  unfold cas; split <;> simp_all

/-- A CAS that does not commit leaves the word as it was. -/
theorem cas_refused {β : Type} [DecidableEq β] (word expected new : β) (h : (cas word expected new).2 = false) :
    (cas word expected new).1 = word := by
  unfold cas at *; split <;> simp_all

/-- Of two CAS from the same word, the first commits. The second then reads a changed word and does not commit. -/
theorem cas_one_wins {β : Type} [DecidableEq β] (expected first second : β) (hn : first ≠ expected) :
    (cas (cas expected expected first).1 expected second).2 = false := by
  simp [cas, hn]

/-! ## The claim word -/

variable {W : Nat}

/-- The free test as the C writes it: each bit of the mask is clear in the word. -/
def free (word mask : BitVec W) : Bool := (~~~word &&& mask) == mask

theorem free_iff (word mask : BitVec W) : free word mask = true ↔ word &&& mask = 0 := by
  unfold free
  simp only [beq_iff_eq]
  constructor
  · intro h
    apply BitVec.eq_of_getLsbD_eq
    intro i hi
    have := congrArg (fun x => x.getLsbD i) h
    simp [hi] at this ⊢
    cases hw : word[i] <;> cases hm : mask[i] <;> simp_all
  · intro h
    apply BitVec.eq_of_getLsbD_eq
    intro i hi
    have := congrArg (fun x => x.getLsbD i) h
    simp [hi] at this ⊢
    cases hw : word[i] <;> cases hm : mask[i] <;> simp_all

/-- A claim: the free test on the word that the claim read, then one CAS from that word to the word with the mask
set. -/
def claim (word seen mask : BitVec W) : BitVec W × Bool :=
  if free seen mask then cas word seen (seen ||| mask) else (word, false)

/-- A claim commits only where the word is the word it read, and the mask is clear in it. -/
theorem claim_commits (word seen mask : BitVec W) :
    (claim word seen mask).2 = true ↔ word = seen ∧ word &&& mask = 0 := by
  unfold claim
  split
  · next hf =>
    rw [cas_commits]
    constructor
    · intro h; subst h; exact ⟨rfl, (free_iff _ _).1 hf⟩
    · intro h; exact h.1
  · next hf =>
    simp only [Bool.false_eq_true, false_iff, not_and]
    intro h hz
    subst h
    exact hf ((free_iff _ _).2 hz)

/-- A claim that does not commit leaves the word as it was. -/
theorem claim_refused (word seen mask : BitVec W) (h : (claim word seen mask).2 = false) :
    (claim word seen mask).1 = word := by
  unfold claim at *
  split
  · next hf => simp only [hf, ↓reduceIte] at h; exact cas_refused _ _ _ h
  · rfl

/-- A claim that commits sets the bits of its mask in the word, and changes no other bit. -/
theorem claim_sets (word seen mask : BitVec W) (h : (claim word seen mask).2 = true) :
    (claim word seen mask).1 = word ||| mask := by
  obtain ⟨hw, _⟩ := (claim_commits word seen mask).1 h
  subst hw
  unfold claim at *
  split
  · simp [cas]
  · next hf => simp_all

/-- Two claims that commit one after the other hold masks with no bit in common. Each claim read its own word, and the
second claim read the word that the first one left. -/
theorem claims_disjoint (word seenA maskA seenB maskB : BitVec W) (hA : (claim word seenA maskA).2 = true)
    (hB : (claim (claim word seenA maskA).1 seenB maskB).2 = true) : maskA &&& maskB = 0 := by
  rw [claim_sets _ _ _ hA] at hB
  obtain ⟨_, hz⟩ := (claim_commits _ _ _).1 hB
  apply BitVec.eq_of_getLsbD_eq
  intro i hi
  have := congrArg (fun x => x.getLsbD i) hz
  simp [hi] at this ⊢
  cases hw : word[i] <;> cases ha : maskA[i] <;> cases hb : maskB[i] <;> simp_all

/-- A claim whose mask has a bit of a held claim does not commit. -/
theorem overlap_refused (word seen held mask : BitVec W) (hs : word = seen ||| held) (hov : held &&& mask ≠ 0) :
    (claim word seen mask).2 = false := by
  cases hc : (claim word seen mask).2
  · rfl
  · obtain ⟨_, hz⟩ := (claim_commits _ _ _).1 hc
    exfalso
    apply hov
    rw [hs] at hz
    clear hs hc
    apply BitVec.eq_of_getLsbD_eq
    intro i hi
    have := congrArg (fun x => x.getLsbD i) hz
    simp [hi] at this ⊢
    cases hw : seen[i] <;> cases ha : held[i] <;> cases hb : mask[i] <;> simp_all

/-- A release: the bits of the mask cleared in the word. -/
def release (word mask : BitVec W) : BitVec W := word &&& ~~~mask

/-- A release after a claim gives back the word that the claim found. -/
theorem release_claim (word mask : BitVec W) (h : word &&& mask = 0) : release (word ||| mask) mask = word := by
  apply BitVec.eq_of_getLsbD_eq
  intro i hi
  have := congrArg (fun x => x.getLsbD i) h
  simp [release, hi] at this ⊢
  cases hw : word[i] <;> cases hm : mask[i] <;> simp_all

/-! ## The bit of an element -/

/-- The bit of element `n` in its word of the claim words, and the place of that word. -/
def bit (W n : Nat) : BitVec W := BitVec.twoPow W (n % W)

def wordOf (W n : Nat) : Nat := n / W

theorem twoPow_and (a b : Nat) (ha : a < W) (hb : b < W) :
    BitVec.twoPow W a &&& BitVec.twoPow W b = 0 ↔ a ≠ b := by
  constructor
  · intro h hab
    subst hab
    have := congrArg (fun x => x.getLsbD a) h
    simp [ha] at this
  · intro hab
    apply BitVec.eq_of_getLsbD_eq
    intro i hi
    simp [BitVec.getLsbD_twoPow]
    omega

/-- Two elements share a bit of the claim words only where they are the same element. -/
theorem bits_overlap (n m : Nat) (hW : 0 < W) :
    (wordOf W n = wordOf W m ∧ bit W n &&& bit W m ≠ 0) ↔ n = m := by
  unfold wordOf bit
  rw [Ne, twoPow_and _ _ (Nat.mod_lt _ hW) (Nat.mod_lt _ hW), Classical.not_not]
  constructor
  · intro ⟨hq, hr⟩
    rw [← Nat.div_add_mod n W, ← Nat.div_add_mod m W, hq, hr]
  · intro h; subst h; exact ⟨rfl, rfl⟩

/-! ## The decay of an element and its span -/

/-- The span test of `vec[n]`: the span of one stride from `n * stride`, in the live bytes `size * stride`. It is the
test of the movable option, in words of `Wd` values. -/
def legal (Wd stride size n : Nat) : Bool := Bounds.fits Wd (size * stride) (n * stride) stride

/-- The span test passes `vec[n]` only where `n` is below the count. -/
theorem legal_iff (Wd stride size n : Nat) (hs : 0 < stride) (hW : size * stride < Wd) :
    legal Wd stride size n = true ↔ n < size := by
  constructor
  · intro h
    have hin := Bounds.fits_inside Wd _ _ _ hW h
    have h1 : (n + 1) * stride ≤ size * stride := by rw [Nat.succ_mul]; exact hin
    exact Nat.le_of_mul_le_mul_right h1 hs
  · intro h
    have h1 : (n + 1) * stride ≤ size * stride := Nat.mul_le_mul_right _ h
    rw [Nat.succ_mul] at h1
    simp only [legal, Bounds.fits, Bool.and_eq_true, decide_eq_true_eq]
    rw [Nat.mod_eq_of_lt (by omega)]
    omega

/-- The record behind a token: the cell base, the element stride, the count, the capacity and the neighbor start. -/
structure Record where
  base : Nat
  stride : Nat
  size : Nat
  cap : Nat
  neighbor : Nat

/-- A record in its layout: the count at max the capacity, and the cell ends at or before its neighbor. -/
def Placed (r : Record) : Prop := r.size ≤ r.cap ∧ r.base + r.cap * r.stride ≤ r.neighbor

/-- The address that `vec[n]` decays to. -/
def decay (r : Record) (n : Nat) : Nat := r.base + n * r.stride

/-- A span that the test passes lies in the cell, below the end of its capacity. -/
theorem legal_inside (Wd : Nat) (r : Record) (n : Nat) (hp : Placed r) (hW : r.size * r.stride < Wd)
    (h : legal Wd r.stride r.size n = true) : decay r n + r.stride ≤ r.base + r.cap * r.stride := by
  have hin := Bounds.fits_inside Wd _ _ _ hW h
  have hc : r.size * r.stride ≤ r.cap * r.stride := Nat.mul_le_mul_right _ hp.1
  unfold decay
  omega

/-- A span that the test passes stops at or before the neighbor. So no legal operation touches the neighbor cell. -/
theorem legal_off_neighbor (Wd : Nat) (r : Record) (n : Nat) (hp : Placed r) (hW : r.size * r.stride < Wd)
    (h : legal Wd r.stride r.size n = true) : decay r n + r.stride ≤ r.neighbor :=
  Nat.le_trans (legal_inside Wd r n hp hW h) hp.2

/-- The spans of two elements do not overlap. -/
theorem decay_disjoint (r : Record) (n m : Nat) (h : n ≠ m) :
    decay r n + r.stride ≤ decay r m ∨ decay r m + r.stride ≤ decay r n := by
  unfold decay
  rcases Nat.lt_or_gt_of_ne h with hl | hl
  · left
    have h1 : (n + 1) * r.stride ≤ m * r.stride := Nat.mul_le_mul_right _ hl
    rw [Nat.succ_mul] at h1
    omega
  · right
    have h1 : (m + 1) * r.stride ≤ n * r.stride := Nat.mul_le_mul_right _ hl
    rw [Nat.succ_mul] at h1
    omega

end LocusCarcerum.Guard
