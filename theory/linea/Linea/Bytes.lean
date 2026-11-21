import Linea.Walk

/-!
# Linea, the plain-argument face of memor

`MemoriaCfg` of memoria_operor.h, and the five calls of memor as their contracts state them. Each call reads
only the members that the header names for it. The face takes its arguments by value. It fills the members
that its call reads, and leaves every other member zero, as a compound literal with those members named does.

The theorems. No call reads a member outside its own set, and so the zero members change no answer. The copy
is the copy of Walk where the regions do not overlap, and the move is the copy at every overlap. The compare
answers 0 exactly where every byte agrees. The find answers the first address that holds the byte, or none
where no byte in range holds it.
-/

namespace Linea.Bytes

open Linea.Walk

/-- The members of MemoriaCfg, each pointer as an address. -/
structure MemoriaCfg where
  dst : Nat
  src : Nat
  other : Nat
  bytes : Nat
  val : Nat
  deriving DecidableEq, Repr

/-- The struct of a compound literal that names no member: every member zero. -/
def MemoriaCfg.none : MemoriaCfg := ⟨0, 0, 0, 0, 0⟩

/-- cpy and move_down: the upward walk, in the blocks `ws` that the backend steps in. -/
def cpy (ws : List Nat) (c : MemoriaCfg) (m : Mem) : Mem := walkUp c.dst c.src ws 0 m

/-- move_up: the downward walk, in the blocks `ws`. -/
def moveUp (ws : List Nat) (c : MemoriaCfg) (m : Mem) : Mem := walkDown c.dst c.src ws c.bytes m

/-- set, as `fill`: the byte `val` in each of `bytes` bytes from `dst`. -/
def fill (c : MemoriaCfg) (m : Mem) : Mem :=
  fun a => if c.dst ≤ a ∧ a < c.dst + c.bytes then c.val else m a

/-- The scan of a compare: from offset `i`, `k` byte pairs of `l` and `r`. -/
def cmpScan (m : Mem) (l r : Nat) : Nat → Nat → Int
  | 0, _ => 0
  | k + 1, i => if m (l + i) = m (r + i) then cmpScan m l r k (i + 1) else (m (l + i) : Int) - m (r + i)

/-- cmp: the difference of the first unequal pair of `src` and `other`, or 0 where every pair agrees. -/
def cmp (c : MemoriaCfg) (m : Mem) : Int := cmpScan m c.src c.other c.bytes 0

/-- The scan of a find: from offset `i`, `k` bytes of `s`. -/
def chrScan (m : Mem) (s v : Nat) : Nat → Nat → Option Nat
  | 0, _ => none
  | k + 1, i => if m (s + i) = v then some (s + i) else chrScan m s v k (i + 1)

/-- chr: the address of the first byte of `src` equal to `val`, within `bytes`. -/
def chr (c : MemoriaCfg) (m : Mem) : Option Nat := chrScan m c.src c.val c.bytes 0

/-! ## The members that each call reads -/

theorem cpy_reads (ws : List Nat) (c d : MemoriaCfg) (m : Mem) (hd : c.dst = d.dst) (hs : c.src = d.src) :
    cpy ws c m = cpy ws d m := by
  unfold cpy
  rw [hd, hs]

theorem moveUp_reads (ws : List Nat) (c d : MemoriaCfg) (m : Mem) (hd : c.dst = d.dst) (hs : c.src = d.src)
    (hb : c.bytes = d.bytes) : moveUp ws c m = moveUp ws d m := by
  unfold moveUp
  rw [hd, hs, hb]

theorem fill_reads (c d : MemoriaCfg) (m : Mem) (hd : c.dst = d.dst) (hb : c.bytes = d.bytes) (hv : c.val = d.val) :
    fill c m = fill d m := by
  unfold fill
  rw [hd, hb, hv]

theorem cmp_reads (c d : MemoriaCfg) (m : Mem) (hs : c.src = d.src) (ho : c.other = d.other)
    (hb : c.bytes = d.bytes) : cmp c m = cmp d m := by
  unfold cmp
  rw [hs, ho, hb]

theorem chr_reads (c d : MemoriaCfg) (m : Mem) (hs : c.src = d.src) (hb : c.bytes = d.bytes) (hv : c.val = d.val) :
    chr c m = chr d m := by
  unfold chr
  rw [hs, hb, hv]

/-! ## The face -/

/-- mmgr_bytes_copy(dst, src, bytes). -/
def bytesCopy (ws : List Nat) (dst src n : Nat) (m : Mem) : Mem :=
  cpy ws { MemoriaCfg.none with dst := dst, src := src, bytes := n } m

/-- mmgr_bytes_move(dst, src, bytes): move_down where the destination is at or below the source, else move_up. -/
def bytesMove (ws : List Nat) (dst src n : Nat) (m : Mem) : Mem :=
  if dst ≤ src then cpy ws { MemoriaCfg.none with dst := dst, src := src, bytes := n } m
  else moveUp ws { MemoriaCfg.none with dst := dst, src := src, bytes := n } m

/-- mmgr_bytes_set(dst, value, bytes). -/
def bytesSet (dst v n : Nat) (m : Mem) : Mem := fill { MemoriaCfg.none with dst := dst, bytes := n, val := v } m

/-- mmgr_bytes_compare(left, right, bytes). -/
def bytesCompare (l r n : Nat) (m : Mem) : Int := cmp { MemoriaCfg.none with src := l, other := r, bytes := n } m

/-- mmgr_bytes_find(region, value, bytes). -/
def bytesFind (s v n : Nat) (m : Mem) : Option Nat := chr { MemoriaCfg.none with src := s, bytes := n, val := v } m

/-- The copy copies where the regions do not overlap, and where the destination lies below the source. -/
theorem bytesCopy_moved (ws : List Nat) (dst src n : Nat) (m : Mem) (cover : ws.sum = n)
    (apart : dst ≤ src ∨ src + n ≤ dst) : bytesCopy ws dst src n m = moved dst src n m :=
  walkUp_moved dst src n ws m cover apart

/-- The move copies at every overlap. -/
theorem bytesMove_moved (ws : List Nat) (dst src n : Nat) (m : Mem) (cover : ws.sum = n) :
    bytesMove ws dst src n m = moved dst src n m := by
  have h := move_moved dst src n ws m cover
  unfold move at h
  unfold bytesMove cpy moveUp
  simpa using h

/-- The set writes the value in each byte of the region, and leaves every other byte. -/
theorem bytesSet_spec (dst v n : Nat) (m : Mem) (a : Nat) :
    bytesSet dst v n m a = if dst ≤ a ∧ a < dst + n then v else m a := rfl

/-- A scan of a compare answers 0 exactly where every pair it covers agrees. -/
theorem cmpScan_zero (m : Mem) (l r : Nat) :
    ∀ (k i : Nat), cmpScan m l r k i = 0 ↔ ∀ j, i ≤ j → j < i + k → m (l + j) = m (r + j)
  | 0, i => by simp [cmpScan]; intro j h1 h2; omega
  | k + 1, i => by
    simp only [cmpScan]
    by_cases he : m (l + i) = m (r + i)
    · rw [ite_eq_left he, cmpScan_zero m l r k (i + 1)]
      constructor
      · intro h j h1 h2
        by_cases hj : j = i
        · subst hj; exact he
        · exact h j (by omega) (by omega)
      · intro h j h1 h2
        exact h j (by omega) (by omega)
    · rw [ite_eq_right he]
      constructor
      · intro h
        exfalso
        apply he
        omega
      · intro h
        exact absurd (h i (Nat.le_refl i) (by omega)) he

/-- The compare answers 0 exactly where every byte of the two regions agrees. -/
theorem bytesCompare_zero (l r n : Nat) (m : Mem) :
    bytesCompare l r n m = 0 ↔ ∀ j, j < n → m (l + j) = m (r + j) := by
  unfold bytesCompare cmp
  rw [cmpScan_zero m l r n 0]
  simp

/-- A scan of a find answers an address that holds the byte, inside the bytes it covers, and the first one. -/
theorem chrScan_some (m : Mem) (s v : Nat) :
    ∀ (k i a : Nat), chrScan m s v k i = some a →
      m a = v ∧ s + i ≤ a ∧ a < s + i + k ∧ ∀ j, i ≤ j → s + j < a → m (s + j) ≠ v
  | 0, i, a, h => by simp [chrScan] at h
  | k + 1, i, a, h => by
    simp only [chrScan] at h
    by_cases hv : m (s + i) = v
    · rw [ite_eq_left hv] at h
      simp only [Option.some.injEq] at h
      subst h
      refine ⟨hv, by omega, by omega, ?_⟩
      intro j h1 h2
      omega
    · rw [ite_eq_right hv] at h
      obtain ⟨h1, h2, h3, h4⟩ := chrScan_some m s v k (i + 1) a h
      refine ⟨h1, by omega, by omega, ?_⟩
      intro j hj hja
      by_cases hji : j = i
      · subst hji; exact hv
      · exact h4 j (by omega) hja

/-- A scan of a find answers none exactly where no byte it covers holds the value. -/
theorem chrScan_none (m : Mem) (s v : Nat) :
    ∀ (k i : Nat), chrScan m s v k i = none → ∀ j, i ≤ j → j < i + k → m (s + j) ≠ v
  | 0, i, _ => by intro j h1 h2; omega
  | k + 1, i, h => by
    simp only [chrScan] at h
    by_cases hv : m (s + i) = v
    · rw [ite_eq_left hv] at h
      exact absurd h (by simp)
    · rw [ite_eq_right hv] at h
      intro j h1 h2
      by_cases hji : j = i
      · subst hji; exact hv
      · exact chrScan_none m s v k (i + 1) h j (by omega) (by omega)

/-- The find answers the first address of the region that holds the byte. -/
theorem bytesFind_some (s v n : Nat) (m : Mem) (a : Nat) (h : bytesFind s v n m = some a) :
    m a = v ∧ s ≤ a ∧ a < s + n ∧ ∀ j, s + j < a → m (s + j) ≠ v := by
  obtain ⟨h1, h2, h3, h4⟩ := chrScan_some m s v n 0 a h
  exact ⟨h1, by omega, by omega, fun j hj => h4 j (Nat.zero_le j) hj⟩

/-- The find answers none where no byte of the region holds the value. -/
theorem bytesFind_none (s v n : Nat) (m : Mem) (h : bytesFind s v n m = none) :
    ∀ j, j < n → m (s + j) ≠ v := fun j hj => chrScan_none m s v n 0 h j (Nat.zero_le j) (by omega)

end Linea.Bytes
