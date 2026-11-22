import Linea.Walk

/-!
# Linea, the plain-argument face of cellul

`CatenaFinitaCfg` of cellularum_laboro.h, and the calls of cellul that the face reaches, as their contracts state
them. Each call reads only the members that the header names for it. The face takes its arguments by value. It
fills the members that its call reads, and it passes `ci` as false, the compare of bytes with no fold.

The theorems. No call reads a member outside its own set. The length is the count of bytes before the first
terminator inside the bound. The equal test answers true only where both texts end together with no
difference before the end. The prefix test answers true only where the prefix ends with no difference before
its end. The byte search and the text search answer the first place that holds what they seek. The copy writes
the bytes of the source before its terminator and then a terminator, inside the bound. The number read takes
digits alone, and it stops at the first byte that is not a digit. The number read in a base takes the digits of a
base from 2 to 36 alone, and it stops at the first byte that is not one. The two span counts answer the bytes at the
start of the text in the set, or outside it, each before the terminator. The search for the last place of a byte
answers the last place before the end that holds it. No class holds a byte of 128 or above. The fold to lower case is
the fold of a compare that folds case, and the fold to upper case undoes it on a lower case letter.
-/

namespace Linea.Text

open Linea.Walk

/-- The members of CatenaFinitaCfg, each pointer as an address. -/
structure CatenaFinitaCfg where
  src : Nat
  cap : Nat
  other : Nat
  other_cap : Nat
  other_len : Nat
  dst : Nat
  «at» : Nat
  byte : Nat
  ci : Bool
  deriving DecidableEq, Repr

/-- The struct of a compound literal that names no member: every member zero. -/
def CatenaFinitaCfg.none : CatenaFinitaCfg := ⟨0, 0, 0, 0, 0, 0, 0, 0, false⟩

/-- The fold of a byte: an upper case letter to its lower case where `ci` holds, and the byte as it is otherwise. -/
def fold (ci : Bool) (b : Nat) : Nat := if ci ∧ 65 ≤ b ∧ b ≤ 90 then b + 32 else b

/-! ## The calls, as their contracts state them -/

/-- The scan of a length: from offset `i`, at max `k` more bytes of `s`. -/
def lenFrom (m : Mem) (s : Nat) : Nat → Nat → Nat
  | 0, i => i
  | k + 1, i => if m (s + i) = 0 then i else lenFrom m s k (i + 1)

/-- len: the bytes of `src` before its terminator, from `at` on, at max `cap - at`. -/
def len (c : CatenaFinitaCfg) (m : Mem) : Nat := lenFrom m (c.src + c.«at») (c.cap - c.«at») 0

/-- The scan of an equal test: from offset `i`, at max `k` more byte pairs. -/
def eqFrom (f : Nat → Nat) (m : Mem) (l r : Nat) : Nat → Nat → Bool
  | 0, _ => false
  | k + 1, i =>
    if f (m (l + i)) ≠ f (m (r + i)) then false else if m (l + i) = 0 then true else eqFrom f m l r k (i + 1)

/-- eq: whether `src` and `other` reach a terminator together with no difference before it, inside `cap`. -/
def eq (c : CatenaFinitaCfg) (m : Mem) : Bool := eqFrom (fold c.ci) m c.src c.other c.cap 0

/-- The scan of a prefix test: from offset `i`, at max `k` more byte pairs. -/
def startsFrom (f : Nat → Nat) (m : Mem) (l r : Nat) : Nat → Nat → Bool
  | 0, _ => false
  | k + 1, i => if m (r + i) = 0 then true else if f (m (l + i)) ≠ f (m (r + i)) then false
    else startsFrom f m l r k (i + 1)

/-- starts: whether `other` reaches its terminator with no difference from `src` before it, inside `cap`. -/
def starts (c : CatenaFinitaCfg) (m : Mem) : Bool := startsFrom (fold c.ci) m c.src c.other c.cap 0

/-- The scan of a byte search: from offset `i`, at max `k` more bytes, before the terminator. -/
def chrFrom (m : Mem) (s v : Nat) : Nat → Nat → Option Nat
  | 0, i => if v = 0 then some (s + i) else none
  | k + 1, i => if m (s + i) = v then some (s + i) else if m (s + i) = 0 then none else chrFrom m s v k (i + 1)

/-- chr: the address of the first byte of `src` equal to `byte`, before the terminator, inside `cap`. A byte of 0
answers the terminator, or `src + cap` where none lies in range. -/
def chr (c : CatenaFinitaCfg) (m : Mem) : Option Nat := chrFrom m c.src c.byte c.cap 0

/-- Whether the needle of `n` bytes at `o` lies at `s + p`, byte for byte. -/
def matchAt (f : Nat → Nat) (m : Mem) (s o n p : Nat) : Bool :=
  (List.range n).all fun j => f (m (s + p + j)) = f (m (o + j))

/-- The scan of a text search: from position `p`, at max `k` more positions. -/
def findFrom (f : Nat → Nat) (m : Mem) (s o n : Nat) : Nat → Nat → Option Nat
  | 0, _ => none
  | k + 1, p => if matchAt f m s o n p then some (s + p) else findFrom f m s o n k (p + 1)

/-- find: the first place in `src` that holds the needle `other`. The needle is `other_len` bytes where that is not
zero, and its measured length otherwise. The places run while the needle fits inside the haystack's length. -/
def find (c : CatenaFinitaCfg) (m : Mem) : Option Nat :=
  let n := if c.other_len ≠ 0 then c.other_len else lenFrom m c.other c.other_cap 0
  let h := lenFrom m c.src c.cap 0
  if n ≤ h then findFrom (fold c.ci) m c.src c.other n (h - n + 1) 0 else none

/-- copy: the bytes of `src` before its terminator, at max `cap - 1` of them, then a terminator, into `dst`. The
bytes go one at a time from the low end, each store after the read of its byte, and the terminator goes last. A
cap of 0 writes nothing. The answer is the bytes copied and the memory after. -/
def copy (c : CatenaFinitaCfg) (m : Mem) : Nat × Mem :=
  if c.cap = 0 then (0, m)
  else
    let n := lenFrom m c.src (c.cap - 1) 0
    let w := walkUp c.dst c.src (List.replicate n 1) 0 m
    (n, fun a => if a = c.dst + n then 0 else w a)

/-! ## The members that each call reads -/

theorem len_reads (c d : CatenaFinitaCfg) (m : Mem) (hs : c.src = d.src) (hc : c.cap = d.cap) (ha : c.«at» = d.«at») :
    len c m = len d m := by
  unfold len
  rw [hs, hc, ha]

theorem eq_reads (c d : CatenaFinitaCfg) (m : Mem) (hs : c.src = d.src) (ho : c.other = d.other)
    (hc : c.cap = d.cap) (hi : c.ci = d.ci) : eq c m = eq d m := by
  unfold eq
  rw [hs, ho, hc, hi]

theorem starts_reads (c d : CatenaFinitaCfg) (m : Mem) (hs : c.src = d.src) (ho : c.other = d.other)
    (hc : c.cap = d.cap) (hi : c.ci = d.ci) : starts c m = starts d m := by
  unfold starts
  rw [hs, ho, hc, hi]

theorem chr_reads (c d : CatenaFinitaCfg) (m : Mem) (hs : c.src = d.src) (hc : c.cap = d.cap)
    (hb : c.byte = d.byte) : chr c m = chr d m := by
  unfold chr
  rw [hs, hc, hb]

theorem find_reads (c d : CatenaFinitaCfg) (m : Mem) (hs : c.src = d.src) (hc : c.cap = d.cap)
    (ho : c.other = d.other) (hoc : c.other_cap = d.other_cap) (hol : c.other_len = d.other_len)
    (hi : c.ci = d.ci) : find c m = find d m := by
  unfold find
  rw [hs, hc, ho, hoc, hol, hi]

theorem copy_reads (c d : CatenaFinitaCfg) (m : Mem) (hd : c.dst = d.dst) (hs : c.src = d.src)
    (hc : c.cap = d.cap) : copy c m = copy d m := by
  unfold copy
  rw [hd, hs, hc]

/-! ## The face -/

/-- mmgr_text_length(text, cap). -/
def textLength (t cap : Nat) (m : Mem) : Nat := len { CatenaFinitaCfg.none with src := t, cap := cap } m

/-- mmgr_text_equal(left, right, cap). -/
def textEqual (l r cap : Nat) (m : Mem) : Bool :=
  eq { CatenaFinitaCfg.none with src := l, other := r, cap := cap } m

/-- mmgr_text_starts(text, prefix, cap). -/
def textStarts (t p cap : Nat) (m : Mem) : Bool :=
  starts { CatenaFinitaCfg.none with src := t, other := p, cap := cap } m

/-- mmgr_text_char(text, byte, cap). -/
def textChar (t v cap : Nat) (m : Mem) : Option Nat :=
  chr { CatenaFinitaCfg.none with src := t, cap := cap, byte := v } m

/-- mmgr_text_find(haystack, needle, cap): the needle measured inside the same bound. -/
def textFind (s o cap : Nat) (m : Mem) : Option Nat :=
  find { CatenaFinitaCfg.none with src := s, cap := cap, other := o, other_cap := cap } m

/-- mmgr_text_copy(dst, src, cap). -/
def textCopy (dst s cap : Nat) (m : Mem) : Nat × Mem :=
  copy { CatenaFinitaCfg.none with dst := dst, src := s, cap := cap } m

/-- The fold with `ci` false is the byte as it is, and so the face compares bytes with no fold. -/
theorem fold_false (b : Nat) : fold false b = b := by simp [fold]

/-! ## The length -/

theorem lenFrom_spec (m : Mem) (s : Nat) :
    ∀ (k i : Nat), i ≤ lenFrom m s k i ∧ lenFrom m s k i ≤ i + k ∧
      (∀ j, i ≤ j → j < lenFrom m s k i → m (s + j) ≠ 0) ∧
      (lenFrom m s k i < i + k → m (s + lenFrom m s k i) = 0)
  | 0, i => by simp [lenFrom]; intro j h1 h2; omega
  | k + 1, i => by
    simp only [lenFrom]
    by_cases hz : m (s + i) = 0
    · rw [ite_eq_left hz]
      refine ⟨Nat.le_refl i, by omega, fun j h1 h2 => by omega, fun _ => hz⟩
    · rw [ite_eq_right hz]
      obtain ⟨h1, h2, h3, h4⟩ := lenFrom_spec m s k (i + 1)
      refine ⟨by omega, by omega, ?_, fun hl => h4 (by omega)⟩
      intro j hj1 hj2
      by_cases hji : j = i
      · subst hji; exact hz
      · exact h3 j (by omega) hj2

/-- The length counts the bytes before the first terminator, at max the bound. Where it falls short of the
bound, the byte it stops at is the terminator. -/
theorem textLength_spec (t cap : Nat) (m : Mem) :
    textLength t cap m ≤ cap ∧ (∀ j, j < textLength t cap m → m (t + j) ≠ 0) ∧
      (textLength t cap m < cap → m (t + textLength t cap m) = 0) := by
  obtain ⟨_, h2, h3, h4⟩ := lenFrom_spec m t cap 0
  unfold textLength len
  have h0 : CatenaFinitaCfg.none.«at» = 0 := rfl
  simp only [h0, Nat.add_zero, Nat.sub_zero]
  refine ⟨by omega, fun j hj => h3 j (Nat.zero_le j) hj, fun hl => h4 (by omega)⟩

/-! ## The equal test and the prefix test -/

theorem eqFrom_true (m : Mem) (l r : Nat) :
    ∀ (k i : Nat), eqFrom id m l r k i = true →
      ∃ e, i ≤ e ∧ e < i + k ∧ m (l + e) = 0 ∧ m (r + e) = 0 ∧ ∀ j, i ≤ j → j < e → m (l + j) = m (r + j)
  | 0, i, h => by simp [eqFrom] at h
  | k + 1, i, h => by
    simp only [eqFrom, id] at h
    by_cases hd : m (l + i) ≠ m (r + i)
    · rw [ite_eq_left hd] at h; exact absurd h (by simp)
    · rw [ite_eq_right hd] at h
      have he : m (l + i) = m (r + i) := Classical.not_not.mp hd
      by_cases hz : m (l + i) = 0
      · refine ⟨i, Nat.le_refl i, by omega, hz, he ▸ hz, fun j h1 h2 => by omega⟩
      · rw [ite_eq_right hz] at h
        obtain ⟨e, h1, h2, h3, h4, h5⟩ := eqFrom_true m l r k (i + 1) h
        refine ⟨e, by omega, by omega, h3, h4, ?_⟩
        intro j hj1 hj2
        by_cases hji : j = i
        · subst hji; exact he
        · exact h5 j (by omega) hj2

/-- The equal test answers true only where both texts end together, at one place inside the bound, with no
difference before it. -/
theorem textEqual_true (l r cap : Nat) (m : Mem) (h : textEqual l r cap m = true) :
    ∃ e, e < cap ∧ m (l + e) = 0 ∧ m (r + e) = 0 ∧ ∀ j, j < e → m (l + j) = m (r + j) := by
  unfold textEqual eq at h
  have hf : fold (CatenaFinitaCfg.none.ci) = id := by funext b; simp [CatenaFinitaCfg.none, fold]
  simp only [hf] at h
  obtain ⟨e, _, h2, h3, h4, h5⟩ := eqFrom_true m l r cap 0 h
  exact ⟨e, by omega, h3, h4, fun j hj => h5 j (Nat.zero_le j) hj⟩

theorem startsFrom_true (m : Mem) (l r : Nat) :
    ∀ (k i : Nat), startsFrom id m l r k i = true →
      ∃ e, i ≤ e ∧ e < i + k ∧ m (r + e) = 0 ∧ ∀ j, i ≤ j → j < e → m (l + j) = m (r + j)
  | 0, i, h => by simp [startsFrom] at h
  | k + 1, i, h => by
    simp only [startsFrom, id] at h
    by_cases hz : m (r + i) = 0
    · exact ⟨i, Nat.le_refl i, by omega, hz, fun j h1 h2 => by omega⟩
    · rw [ite_eq_right hz] at h
      by_cases hd : m (l + i) ≠ m (r + i)
      · rw [ite_eq_left hd] at h; exact absurd h (by simp)
      · rw [ite_eq_right hd] at h
        have he : m (l + i) = m (r + i) := Classical.not_not.mp hd
        obtain ⟨e, h1, h2, h3, h4⟩ := startsFrom_true m l r k (i + 1) h
        refine ⟨e, by omega, by omega, h3, ?_⟩
        intro j hj1 hj2
        by_cases hji : j = i
        · subst hji; exact he
        · exact h4 j (by omega) hj2

/-- The prefix test answers true only where the prefix ends inside the bound with no difference from the text
before its end. -/
theorem textStarts_true (t p cap : Nat) (m : Mem) (h : textStarts t p cap m = true) :
    ∃ e, e < cap ∧ m (p + e) = 0 ∧ ∀ j, j < e → m (t + j) = m (p + j) := by
  unfold textStarts starts at h
  have hf : fold (CatenaFinitaCfg.none.ci) = id := by funext b; simp [CatenaFinitaCfg.none, fold]
  simp only [hf] at h
  obtain ⟨e, _, h2, h3, h4⟩ := startsFrom_true m t p cap 0 h
  exact ⟨e, by omega, h3, fun j hj => h4 j (Nat.zero_le j) hj⟩

/-! ## The byte search and the text search -/

theorem chrFrom_some (m : Mem) (s v : Nat) :
    ∀ (k i a : Nat), chrFrom m s v k i = some a →
      s + i ≤ a ∧ a ≤ s + i + k ∧ (m a = v ∨ (v = 0 ∧ a = s + i + k)) ∧
        ∀ j, i ≤ j → s + j < a → m (s + j) ≠ v ∧ m (s + j) ≠ 0
  | 0, i, a, h => by
    simp only [chrFrom] at h
    by_cases hv : v = 0
    · rw [ite_eq_left hv] at h
      simp only [Option.some.injEq] at h
      subst h
      exact ⟨by omega, by omega, Or.inr ⟨hv, by omega⟩, fun j h1 h2 => by omega⟩
    · rw [ite_eq_right hv] at h; exact absurd h (by simp)
  | k + 1, i, a, h => by
    simp only [chrFrom] at h
    by_cases hv : m (s + i) = v
    · rw [ite_eq_left hv] at h
      simp only [Option.some.injEq] at h
      subst h
      exact ⟨by omega, by omega, Or.inl hv, fun j h1 h2 => by omega⟩
    · rw [ite_eq_right hv] at h
      by_cases hz : m (s + i) = 0
      · rw [ite_eq_left hz] at h; exact absurd h (by simp)
      · rw [ite_eq_right hz] at h
        obtain ⟨h1, h2, h3, h4⟩ := chrFrom_some m s v k (i + 1) a h
        refine ⟨by omega, by omega, by
          rcases h3 with h3 | ⟨h3, h3'⟩
          · exact Or.inl h3
          · exact Or.inr ⟨h3, by omega⟩, ?_⟩
        intro j hj1 hj2
        by_cases hji : j = i
        · subst hji; exact ⟨hv, hz⟩
        · exact h4 j (by omega) hj2

/-- The byte search answers a place that holds the byte, before which no byte holds it and no byte ends the
text. A byte of 0 may answer the end of the bound instead. -/
theorem textChar_some (t v cap : Nat) (m : Mem) (a : Nat) (h : textChar t v cap m = some a) :
    t ≤ a ∧ a ≤ t + cap ∧ (m a = v ∨ (v = 0 ∧ a = t + cap)) ∧
      ∀ j, t + j < a → m (t + j) ≠ v ∧ m (t + j) ≠ 0 := by
  obtain ⟨h1, h2, h3, h4⟩ := chrFrom_some m t v cap 0 a h
  refine ⟨by omega, by omega, by simpa using h3, fun j hj => h4 j (Nat.zero_le j) hj⟩

theorem findFrom_some (f : Nat → Nat) (m : Mem) (s o n : Nat) :
    ∀ (k p a : Nat), findFrom f m s o n k p = some a →
      ∃ q, p ≤ q ∧ q < p + k ∧ a = s + q ∧ matchAt f m s o n q = true ∧
        ∀ r, p ≤ r → r < q → matchAt f m s o n r = false
  | 0, p, a, h => by simp [findFrom] at h
  | k + 1, p, a, h => by
    simp only [findFrom] at h
    by_cases hm : matchAt f m s o n p = true
    · rw [ite_eq_left hm] at h
      simp only [Option.some.injEq] at h
      exact ⟨p, Nat.le_refl p, by omega, h.symm, hm, fun r h1 h2 => by omega⟩
    · rw [ite_eq_right hm] at h
      obtain ⟨q, h1, h2, h3, h4, h5⟩ := findFrom_some f m s o n k (p + 1) a h
      refine ⟨q, by omega, by omega, h3, h4, ?_⟩
      intro r hr1 hr2
      by_cases hrp : r = p
      · subst hrp; simpa using hm
      · exact h5 r (by omega) hr2

/-- The text search answers the first place of the haystack that holds the needle byte for byte. -/
theorem textFind_some (s o cap : Nat) (m : Mem) (a : Nat) (h : textFind s o cap m = some a) :
    ∃ q n, a = s + q ∧ matchAt id m s o n q = true ∧ ∀ r, r < q → matchAt id m s o n r = false := by
  unfold textFind find at h
  have hf : fold (CatenaFinitaCfg.none.ci) = id := by funext b; simp [CatenaFinitaCfg.none, fold]
  have hl : CatenaFinitaCfg.none.other_len = 0 := rfl
  simp only [hf, hl, ne_eq, not_true_eq_false, ite_false] at h
  by_cases hfit : lenFrom m o cap 0 ≤ lenFrom m s cap 0
  · rw [ite_eq_left hfit] at h
    obtain ⟨q, _, _, h3, h4, h5⟩ := findFrom_some id m s o _ _ 0 a h
    exact ⟨q, _, h3, h4, fun r hr => h5 r (Nat.zero_le r) hr⟩
  · rw [ite_eq_right hfit] at h
    exact absurd h (by simp)

/-! ## The copy -/

/-- The copy writes, inside the bound, the bytes of the source before its terminator and then a terminator, where
the two regions do not overlap. The answer counts the bytes copied. -/
theorem textCopy_spec (dst s cap : Nat) (m : Mem) (hc : 0 < cap) (apart : dst + cap ≤ s ∨ s + cap ≤ dst) :
    let r := textCopy dst s cap m
    r.1 < cap ∧ r.2 (dst + r.1) = 0 ∧ (∀ j, j < r.1 → r.2 (dst + j) = m (s + j) ∧ m (s + j) ≠ 0) ∧
      ∀ a, ¬ (dst ≤ a ∧ a ≤ dst + r.1) → r.2 a = m a := by
  obtain ⟨_, h2, h3, _⟩ := lenFrom_spec m s (cap - 1) 0
  have hz : ¬ cap = 0 := by omega
  -- the walk of the bytes is the copy of Walk, where the regions do not overlap
  have hw : walkUp dst s (List.replicate (lenFrom m s (cap - 1) 0) 1) 0 m = moved dst s (lenFrom m s (cap - 1) 0) m :=
    walkUp_moved dst s _ _ m (by simp) (by omega)
  unfold textCopy copy
  simp only [hz, ite_false, hw]
  refine ⟨by omega, ?_, ?_, ?_⟩
  · simp
  · intro j hj
    have hne : ¬ dst + j = dst + lenFrom m s (cap - 1) 0 := by omega
    have hin : dst + 0 ≤ dst + j ∧ dst + j < dst + lenFrom m s (cap - 1) 0 := by omega
    simp only [hne, ite_false, moved, span, hin]
    refine ⟨by simp, h3 j (Nat.zero_le j) (by omega)⟩
  · intro a ha
    have h1 : ¬ (dst + 0 ≤ a ∧ a < dst + lenFrom m s (cap - 1) 0) := by omega
    have h2' : ¬ a = dst + lenFrom m s (cap - 1) 0 := by omega
    simp only [h2', ite_false, moved, span, h1]

/-! ## The number read -/

/-- Whether a byte is whitespace: space, tab, newline, vertical tab, form feed or carriage return. -/
def isSpace (b : Nat) : Bool := b = 32 ∨ (9 ≤ b ∧ b ≤ 13)

/-- Whether a byte is a decimal digit. -/
def isDigit (b : Nat) : Bool := 48 ≤ b ∧ b ≤ 57

/-- The scan past whitespace: from offset `i`, at max `k` more bytes. -/
def spaceFrom (m : Mem) (s : Nat) : Nat → Nat → Nat
  | 0, i => i
  | k + 1, i => if isSpace (m (s + i)) then spaceFrom m s k (i + 1) else i

/-- The scan of the digits: from offset `i` with the value `v` so far, at max `k` more bytes, each digit taken
into a word of `w` bits that wraps. The answer is the value and the offset past the last digit. -/
def digitsFrom (m : Mem) (s w : Nat) : Nat → Nat → Nat → Nat × Nat
  | 0, i, v => (v, i)
  | k + 1, i, v =>
    if isDigit (m (s + i)) then digitsFrom m s w k (i + 1) ((v * 10 + (m (s + i) - 48)) % 2 ^ w) else (v, i)

/-- The members of TransfiguroCfg: the text, the target of the end, as an address, and the base of to_ulong_in. -/
structure TransfiguroCfg where
  src : Nat
  «end» : Nat
  base : Nat := 0
  deriving DecidableEq, Repr

/-- to_ulong: leading whitespace skipped, one optional '+' taken, then the digits, in a word of `w` bits. The answer
is the value and the offset of the end from `src`: past the last digit, or 0 where no digit came, with the value 0
there. The text ends at a byte that is not part of the number, and `k` bounds the scan past that end. -/
def toUlong (w k : Nat) (c : TransfiguroCfg) (m : Mem) : Nat × Nat :=
  let i0 := spaceFrom m c.src k 0
  let i1 := if m (c.src + i0) = 43 then i0 + 1 else i0
  let r := digitsFrom m c.src w k i1 0
  if r.2 = i1 then (0, 0) else r

theorem toUlong_reads (w k : Nat) (c d : TransfiguroCfg) (m : Mem) (hs : c.src = d.src) :
    toUlong w k c m = toUlong w k d m := by
  unfold toUlong
  rw [hs]

/-- mmgr_text_unsigned(text, end). -/
def textUnsigned (w k t e : Nat) (m : Mem) : Nat × Nat := toUlong w k { src := t, «end» := e } m

theorem digitsFrom_spec (m : Mem) (s w : Nat) :
    ∀ (k i v : Nat), i ≤ (digitsFrom m s w k i v).2 ∧ (digitsFrom m s w k i v).2 ≤ i + k ∧
      (∀ j, i ≤ j → j < (digitsFrom m s w k i v).2 → isDigit (m (s + j)) = true) ∧
      ((digitsFrom m s w k i v).2 < i + k → isDigit (m (s + (digitsFrom m s w k i v).2)) = false)
  | 0, i, v => by simp [digitsFrom]; intro j h1 h2; omega
  | k + 1, i, v => by
    simp only [digitsFrom]
    by_cases hd : isDigit (m (s + i)) = true
    · rw [ite_eq_left hd]
      obtain ⟨h1, h2, h3, h4⟩ := digitsFrom_spec m s w k (i + 1) ((v * 10 + (m (s + i) - 48)) % 2 ^ w)
      refine ⟨by omega, by omega, ?_, fun hl => h4 (by omega)⟩
      intro j hj1 hj2
      by_cases hji : j = i
      · subst hji; exact hd
      · exact h3 j (by omega) hj2
    · rw [ite_eq_right hd]
      refine ⟨Nat.le_refl i, by omega, fun j h1 h2 => by omega, fun _ => by simpa using hd⟩

/-- The number read takes digits alone. Where it took any, every byte from its first digit to its end is a digit,
and the byte at the end is not one, unless the scan bound came first. Where it took none, the end is the start
and the value is 0. -/
theorem textUnsigned_spec (w k t e : Nat) (m : Mem) :
    let r := textUnsigned w k t e m
    (r.2 = 0 ∧ r.1 = 0) ∨
      ∃ i1, i1 < r.2 ∧ (∀ j, i1 ≤ j → j < r.2 → isDigit (m (t + j)) = true) ∧
        (r.2 < i1 + k → isDigit (m (t + r.2)) = false) := by
  unfold textUnsigned toUlong
  simp only
  generalize hi0 : spaceFrom m t k 0 = i0
  generalize hi1 : (if m (t + i0) = 43 then i0 + 1 else i0) = i1
  obtain ⟨h1, h2, h3, h4⟩ := digitsFrom_spec m t w k i1 0
  by_cases he : (digitsFrom m t w k i1 0).2 = i1
  · left
    rw [ite_eq_left he]
    exact ⟨rfl, rfl⟩
  · right
    rw [ite_eq_right he]
    exact ⟨i1, by omega, h3, h4⟩

/-! ## The number read in a base -/

/-- The value of a digit in a base up to 36: '0' to '9', then a letter of either case for 10 to 35, and 36 for any
other byte. -/
def digitValue (b : Nat) : Nat :=
  if 48 ≤ b ∧ b ≤ 57 then b - 48 else if 65 ≤ b ∧ b ≤ 90 then b - 55 else if 97 ≤ b ∧ b ≤ 122 then b - 87 else 36

/-- The scan of the digits of a base: from offset `i` with the value `v` so far, at max `k` more bytes. The value is
the number that the digits write, with no wrap. The answer is the value and the offset past the last digit. -/
def digitsInFrom (m : Mem) (s base : Nat) : Nat → Nat → Nat → Nat × Nat
  | 0, i, v => (v, i)
  | k + 1, i, v =>
    if digitValue (m (s + i)) < base then digitsInFrom m s base k (i + 1) (v * base + digitValue (m (s + i)))
    else (v, i)

/-- The offset past the whitespace and one optional sign, a '+' or a '-'. -/
def signEnd (m : Mem) (s k : Nat) : Nat :=
  if m (s + spaceFrom m s k 0) = 43 ∨ m (s + spaceFrom m s k 0) = 45 then spaceFrom m s k 0 + 1
  else spaceFrom m s k 0

/-- Whether the sign after the whitespace is a '-'. -/
def negative (m : Mem) (s k : Nat) : Bool := m (s + spaceFrom m s k 0) = 45

/-- Whether a 0, then an x or an X, then a hex digit, lie at offset `i`. -/
def prefixed (m : Mem) (s i : Nat) : Bool :=
  m (s + i) = 48 ∧ (m (s + i + 1) = 120 ∨ m (s + i + 1) = 88) ∧ digitValue (m (s + i + 2)) < 16

/-- The base the read takes: 16 after a prefix for a base of 0 or 16, 8 after a leading 0 or 10 for a base of 0,
and the base asked for otherwise. -/
def baseOf (base : Nat) (m : Mem) (s i : Nat) : Nat :=
  if (base = 0 ∨ base = 16) ∧ prefixed m s i = true then 16
  else if base = 0 then (if m (s + i) = 48 then 8 else 10) else base

/-- The offset of the first digit: past the prefix where the read takes it. -/
def digitsStart (base : Nat) (m : Mem) (s i : Nat) : Nat :=
  if (base = 0 ∨ base = 16) ∧ prefixed m s i = true then i + 2 else i

/-- to_ulong_in: leading whitespace skipped, one optional sign taken, then the base and its prefix, then the digits,
in a word of `w` bits. A value past the word answers all ones, and a '-' negates the value in the word. The answer is
the value and the offset of the end from `src`: past the last digit, or 0 where no digit came or the base is outside
2 to 36, with the value 0 there. `k` bounds the scan past the end of the number. -/
def toUlongIn (w k : Nat) (c : TransfiguroCfg) (m : Mem) : Nat × Nat :=
  if baseOf c.base m c.src (signEnd m c.src k) < 2 ∨ 36 < baseOf c.base m c.src (signEnd m c.src k) then (0, 0)
  else if (digitsInFrom m c.src (baseOf c.base m c.src (signEnd m c.src k)) k
      (digitsStart c.base m c.src (signEnd m c.src k)) 0).2 = digitsStart c.base m c.src (signEnd m c.src k) then (0, 0)
  else
    let r := digitsInFrom m c.src (baseOf c.base m c.src (signEnd m c.src k)) k
      (digitsStart c.base m c.src (signEnd m c.src k)) 0
    (if 2 ^ w ≤ r.1 then 2 ^ w - 1 else if negative m c.src k = true then (2 ^ w - r.1) % 2 ^ w else r.1, r.2)

theorem toUlongIn_reads (w k : Nat) (c d : TransfiguroCfg) (m : Mem) (hs : c.src = d.src) (hb : c.base = d.base) :
    toUlongIn w k c m = toUlongIn w k d m := by
  unfold toUlongIn
  rw [hs, hb]

/-- mmgr_text_unsigned_in(text, end, base). -/
def textUnsignedIn (w k t e base : Nat) (m : Mem) : Nat × Nat := toUlongIn w k { src := t, «end» := e, base := base } m

theorem digitsInFrom_spec (m : Mem) (s base : Nat) :
    ∀ (k i v : Nat), i ≤ (digitsInFrom m s base k i v).2 ∧ (digitsInFrom m s base k i v).2 ≤ i + k ∧
      (∀ j, i ≤ j → j < (digitsInFrom m s base k i v).2 → digitValue (m (s + j)) < base) ∧
      ((digitsInFrom m s base k i v).2 < i + k → base ≤ digitValue (m (s + (digitsInFrom m s base k i v).2)))
  | 0, i, v => by simp [digitsInFrom]; intro j h1 h2; omega
  | k + 1, i, v => by
    simp only [digitsInFrom]
    by_cases hd : digitValue (m (s + i)) < base
    · rw [ite_eq_left hd]
      obtain ⟨h1, h2, h3, h4⟩ := digitsInFrom_spec m s base k (i + 1) (v * base + digitValue (m (s + i)))
      refine ⟨by omega, by omega, ?_, fun hl => h4 (by omega)⟩
      intro j hj1 hj2
      by_cases hji : j = i
      · subst hji; exact hd
      · exact h3 j (by omega) hj2
    · rw [ite_eq_right hd]
      refine ⟨Nat.le_refl i, by omega, fun j h1 h2 => by omega, fun _ => Nat.le_of_not_lt hd⟩

/-- The number read in a base takes digits of its base alone. Where it took any, the base is 2 to 36, every byte from
its first digit to its end is a digit of the base, and the byte at the end is not one, unless the scan bound came
first. Where it took none, or the base is outside 2 to 36, the end is the start and the value is 0. -/
theorem textUnsignedIn_spec (w k t e base : Nat) (m : Mem) :
    let r := textUnsignedIn w k t e base m
    (r.2 = 0 ∧ r.1 = 0) ∨
      ∃ b i2, 2 ≤ b ∧ b ≤ 36 ∧ i2 < r.2 ∧ (∀ j, i2 ≤ j → j < r.2 → digitValue (m (t + j)) < b) ∧
        (r.2 < i2 + k → b ≤ digitValue (m (t + r.2))) := by
  unfold textUnsignedIn toUlongIn
  simp only
  generalize baseOf base m t (signEnd m t k) = b
  generalize digitsStart base m t (signEnd m t k) = i2
  by_cases hb : b < 2 ∨ 36 < b
  · left
    rw [ite_eq_left hb]
    exact ⟨rfl, rfl⟩
  · rw [ite_eq_right hb]
    obtain ⟨h1, h2, h3, h4⟩ := digitsInFrom_spec m t b k i2 0
    by_cases he : (digitsInFrom m t b k i2 0).2 = i2
    · left
      rw [ite_eq_left he]
      exact ⟨rfl, rfl⟩
    · right
      rw [ite_eq_right he]
      exact ⟨b, i2, by omega, by omega, by omega, h3, h4⟩

/-! ## The span counts -/

/-- Whether the set at `o` holds the byte `b`: a byte of the set before its terminator, inside a scan of `k`. -/
def inSet (k : Nat) (m : Mem) (o b : Nat) : Bool := (List.range (lenFrom m o k 0)).any fun j => m (o + j) = b

/-- The scan of a span: from offset `i`, at max `n` more bytes, each before the terminator and each that `held`
keeps. -/
def spanFrom (held : Nat → Bool) (m : Mem) (s : Nat) : Nat → Nat → Nat
  | 0, i => i
  | n + 1, i => if m (s + i) = 0 then i else if held (m (s + i)) = true then spanFrom held m s n (i + 1) else i

/-- span: the bytes at the start of `src` that the set `other` holds, before the terminator, inside `cap`. -/
def span (k : Nat) (c : CatenaFinitaCfg) (m : Mem) : Nat := spanFrom (inSet k m c.other) m c.src c.cap 0

/-- span_not: the bytes at the start of `src` that the set `other` does not hold, before the terminator, inside
`cap`. -/
def spanNot (k : Nat) (c : CatenaFinitaCfg) (m : Mem) : Nat :=
  spanFrom (fun b => !inSet k m c.other b) m c.src c.cap 0

theorem span_reads (k : Nat) (c d : CatenaFinitaCfg) (m : Mem) (hs : c.src = d.src) (ho : c.other = d.other)
    (hc : c.cap = d.cap) : span k c m = span k d m := by
  unfold span
  rw [hs, ho, hc]

theorem spanNot_reads (k : Nat) (c d : CatenaFinitaCfg) (m : Mem) (hs : c.src = d.src) (ho : c.other = d.other)
    (hc : c.cap = d.cap) : spanNot k c m = spanNot k d m := by
  unfold spanNot
  rw [hs, ho, hc]

theorem spanFrom_spec (held : Nat → Bool) (m : Mem) (s : Nat) :
    ∀ (n i : Nat), i ≤ spanFrom held m s n i ∧ spanFrom held m s n i ≤ i + n ∧
      (∀ j, i ≤ j → j < spanFrom held m s n i → m (s + j) ≠ 0 ∧ held (m (s + j)) = true) ∧
      (spanFrom held m s n i < i + n →
        m (s + spanFrom held m s n i) = 0 ∨ held (m (s + spanFrom held m s n i)) = false)
  | 0, i => by simp [spanFrom]; intro j h1 h2; omega
  | n + 1, i => by
    simp only [spanFrom]
    by_cases hz : m (s + i) = 0
    · rw [ite_eq_left hz]
      exact ⟨Nat.le_refl i, by omega, fun j h1 h2 => by omega, fun _ => Or.inl hz⟩
    · rw [ite_eq_right hz]
      by_cases hh : held (m (s + i)) = true
      · rw [ite_eq_left hh]
        obtain ⟨h1, h2, h3, h4⟩ := spanFrom_spec held m s n (i + 1)
        refine ⟨by omega, by omega, ?_, fun hl => h4 (by omega)⟩
        intro j hj1 hj2
        by_cases hji : j = i
        · subst hji; exact ⟨hz, hh⟩
        · exact h3 j (by omega) hj2
      · rw [ite_eq_right hh]
        exact ⟨Nat.le_refl i, by omega, fun j h1 h2 => by omega, fun _ => Or.inr (by simpa using hh)⟩

/-- mmgr_text_span(text, set, cap). -/
def textSpan (k t o cap : Nat) (m : Mem) : Nat := span k { CatenaFinitaCfg.none with src := t, other := o, cap := cap } m

/-- mmgr_text_span_not(text, set, cap). -/
def textSpanNot (k t o cap : Nat) (m : Mem) : Nat :=
  spanNot k { CatenaFinitaCfg.none with src := t, other := o, cap := cap } m

theorem textSpan_eq (k t o cap : Nat) (m : Mem) : textSpan k t o cap m = spanFrom (inSet k m o) m t cap 0 := rfl

theorem textSpanNot_eq (k t o cap : Nat) (m : Mem) :
    textSpanNot k t o cap m = spanFrom (fun b => !inSet k m o b) m t cap 0 := rfl

/-- The span count answers the bytes at the start of the text that the set holds, each before the terminator. The
byte at the count ends the text or lies outside the set, unless the bound came first. -/
theorem textSpan_spec (k t o cap : Nat) (m : Mem) :
    textSpan k t o cap m ≤ cap ∧
      (∀ j, j < textSpan k t o cap m → m (t + j) ≠ 0 ∧ inSet k m o (m (t + j)) = true) ∧
      (textSpan k t o cap m < cap →
        m (t + textSpan k t o cap m) = 0 ∨ inSet k m o (m (t + textSpan k t o cap m)) = false) := by
  rw [textSpan_eq]
  obtain ⟨_, h2, h3, h4⟩ := spanFrom_spec (inSet k m o) m t cap 0
  exact ⟨by omega, fun j hj => h3 j (Nat.zero_le j) hj, fun hl => h4 (by omega)⟩

/-- The count of a span outside the set answers the bytes at the start of the text that the set does not hold,
each before the terminator. The byte at the count ends the text or lies in the set, unless the bound came first. -/
theorem textSpanNot_spec (k t o cap : Nat) (m : Mem) :
    textSpanNot k t o cap m ≤ cap ∧
      (∀ j, j < textSpanNot k t o cap m → m (t + j) ≠ 0 ∧ inSet k m o (m (t + j)) = false) ∧
      (textSpanNot k t o cap m < cap →
        m (t + textSpanNot k t o cap m) = 0 ∨ inSet k m o (m (t + textSpanNot k t o cap m)) = true) := by
  rw [textSpanNot_eq]
  obtain ⟨_, h2, h3, h4⟩ := spanFrom_spec (fun b => !inSet k m o b) m t cap 0
  refine ⟨by omega, fun j hj => ?_, fun hl => ?_⟩
  · obtain ⟨h5, h6⟩ := h3 j (Nat.zero_le j) hj
    exact ⟨h5, by simpa using h6⟩
  · rcases h4 (by omega) with h5 | h5
    · exact Or.inl h5
    · exact Or.inr (by simpa using h5)

/-! ## The search for the last place of a byte -/

/-- The search back for a byte: the last of the first `n` bytes of `s` that equals `v`. -/
def lastIn (m : Mem) (s v : Nat) : Nat → Option Nat
  | 0 => none
  | n + 1 => if m (s + n) = v then some (s + n) else lastIn m s v n

/-- chr_last: the address of the last byte of `src` equal to `byte`, before the terminator, inside `cap`. A byte of 0
answers the terminator, or `src + cap` where none lies in range. -/
def chrLast (c : CatenaFinitaCfg) (m : Mem) : Option Nat :=
  if c.byte = 0 then some (c.src + lenFrom m c.src c.cap 0) else lastIn m c.src c.byte (lenFrom m c.src c.cap 0)

theorem chrLast_reads (c d : CatenaFinitaCfg) (m : Mem) (hs : c.src = d.src) (hc : c.cap = d.cap)
    (hb : c.byte = d.byte) : chrLast c m = chrLast d m := by
  unfold chrLast
  rw [hs, hc, hb]

theorem lastIn_some (m : Mem) (s v : Nat) :
    ∀ (n a : Nat), lastIn m s v n = some a →
      ∃ j, j < n ∧ a = s + j ∧ m (s + j) = v ∧ ∀ j', j < j' → j' < n → m (s + j') ≠ v
  | 0, a, h => by simp [lastIn] at h
  | n + 1, a, h => by
    simp only [lastIn] at h
    by_cases hv : m (s + n) = v
    · rw [ite_eq_left hv] at h
      simp only [Option.some.injEq] at h
      exact ⟨n, by omega, h.symm, hv, fun j' h1 h2 => by omega⟩
    · rw [ite_eq_right hv] at h
      obtain ⟨j, h1, h2, h3, h4⟩ := lastIn_some m s v n a h
      refine ⟨j, by omega, h2, h3, ?_⟩
      intro j' hj1 hj2
      by_cases hjn : j' = n
      · subst hjn; exact hv
      · exact h4 j' hj1 (by omega)

/-- mmgr_text_char_last(text, byte, cap). -/
def textCharLast (t v cap : Nat) (m : Mem) : Option Nat :=
  chrLast { CatenaFinitaCfg.none with src := t, cap := cap, byte := v } m

theorem textCharLast_eq (t v cap : Nat) (m : Mem) :
    textCharLast t v cap m =
      if v = 0 then some (t + lenFrom m t cap 0) else lastIn m t v (lenFrom m t cap 0) := rfl

/-- The search for the last place of a byte answers a place before the end of the text that holds the byte, past
which no byte before the end holds it. -/
theorem textCharLast_some (t v cap : Nat) (m : Mem) (a : Nat) (hv : v ≠ 0) (h : textCharLast t v cap m = some a) :
    ∃ j, j < lenFrom m t cap 0 ∧ a = t + j ∧ m (t + j) = v ∧
      ∀ j', j < j' → j' < lenFrom m t cap 0 → m (t + j') ≠ v := by
  rw [textCharLast_eq, ite_eq_right hv] at h
  exact lastIn_some m t v _ a h

/-- A byte of 0 answers the end of the text: the terminator, or the end of the bound. -/
theorem textCharLast_zero (t cap : Nat) (m : Mem) : textCharLast t 0 cap m = some (t + lenFrom m t cap 0) := by
  rw [textCharLast_eq, ite_eq_left rfl]

/-! ## The class test and the case folds -/

/-- The classes of MmgrAsciiClass in their order, each as the bytes it holds below 128. -/
def classHolds : Nat → Nat → Bool
  | 0, b => 48 ≤ b ∧ b ≤ 57
  | 1, b => (65 ≤ b ∧ b ≤ 90) ∨ (97 ≤ b ∧ b ≤ 122)
  | 2, b => (48 ≤ b ∧ b ≤ 57) ∨ (65 ≤ b ∧ b ≤ 90) ∨ (97 ≤ b ∧ b ≤ 122)
  | 3, b => 65 ≤ b ∧ b ≤ 90
  | 4, b => 97 ≤ b ∧ b ≤ 122
  | 5, b => (48 ≤ b ∧ b ≤ 57) ∨ (65 ≤ b ∧ b ≤ 70) ∨ (97 ≤ b ∧ b ≤ 102)
  | 6, b => (33 ≤ b ∧ b ≤ 47) ∨ (58 ≤ b ∧ b ≤ 64) ∨ (91 ≤ b ∧ b ≤ 96) ∨ (123 ≤ b ∧ b ≤ 126)
  | 7, b => (9 ≤ b ∧ b ≤ 13) ∨ b = 32
  | 8, b => b ≤ 31 ∨ b = 127
  | 9, b => 32 ≤ b ∧ b ≤ 126
  | _, _ => false

/-- mmgr_text_class(byte, class). -/
def textClass (b type : Nat) : Bool := b < 128 ∧ classHolds type b = true

/-- mmgr_text_lower(byte). -/
def textLower (b : Nat) : Nat := if 65 ≤ b ∧ b ≤ 90 then b + 32 else b

/-- mmgr_text_upper(byte). -/
def textUpper (b : Nat) : Nat := if 97 ≤ b ∧ b ≤ 122 then b - 32 else b

/-- No class holds a byte of 128 or above. -/
theorem textClass_ascii (b type : Nat) (h : textClass b type = true) : b < 128 := by
  simp [textClass] at h
  exact h.1

/-- The fold to lower case is the fold that a compare with `ci` set reads each byte through. -/
theorem textLower_fold (b : Nat) : textLower b = fold true b := by
  simp [textLower, fold]

/-- The fold to upper case takes a lower case letter to the upper case letter whose lower case it is. -/
theorem textLower_upper (b : Nat) (h : 97 ≤ b ∧ b ≤ 122) : textLower (textUpper b) = b := by
  unfold textUpper
  rw [ite_eq_left h]
  unfold textLower
  rw [ite_eq_left (show 65 ≤ b - 32 ∧ b - 32 ≤ 90 by omega)]
  omega

/-- A byte that is no letter stays as it is under both folds. -/
theorem textFold_other (b : Nat) (h1 : ¬ (65 ≤ b ∧ b ≤ 90)) (h2 : ¬ (97 ≤ b ∧ b ≤ 122)) :
    textLower b = b ∧ textUpper b = b := by
  unfold textLower textUpper
  rw [ite_eq_right h1, ite_eq_right h2]
  exact ⟨rfl, rfl⟩

end Linea.Text
