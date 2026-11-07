/-!
# LocusCarcerum, the fragmentation reading

`mmgr_series_cellularum_fragmentation` reads the bytes the persistent tier occupies that no payload holds, the
holes, against the hundredth of the bytes it occupies, rounded up, and reads 0 below a hundred bytes. The divisor
is a hundredth rather than the holes times a hundred, since that product can pass what a narrow word counts.

The theorem: the holes being at most the bytes occupied, the reading is at most 100. `old_over` is the reading
against the hundredth rounded down, which a tier of 199 bytes reads as 152.
-/

namespace LocusCarcerum.Fragment

/-- The hundredth of the bytes occupied, rounded up. -/
def hundredth (occupied : Nat) : Nat := occupied / 100 + (if occupied % 100 ≠ 0 then 1 else 0)

/-- The reading. -/
def reading (occupied holes : Nat) : Nat := if 100 ≤ occupied then holes / hundredth occupied else 0

theorem hundredth_covers (occupied : Nat) : occupied ≤ 100 * hundredth occupied := by
  unfold hundredth
  have h := Nat.div_add_mod occupied 100
  split <;> omega

theorem hundredth_pos (occupied : Nat) (h : 100 ≤ occupied) : 0 < hundredth occupied := by
  unfold hundredth
  have : 1 ≤ occupied / 100 := (Nat.le_div_iff_mul_le (by decide)).2 (by omega)
  omega

/-- The reading is a percent. -/
theorem reading_le (occupied holes : Nat) (h : holes ≤ occupied) : reading occupied holes ≤ 100 := by
  unfold reading
  split
  · next hbig =>
    apply Nat.div_le_of_le_mul
    have := hundredth_covers occupied
    rw [Nat.mul_comm] at this
    omega
  · exact Nat.zero_le _

/-- The reading against the hundredth rounded down. -/
def oldReading (occupied holes : Nat) : Nat := if 100 ≤ occupied then holes / (occupied / 100) else 0

/-- A tier of 199 bytes with 152 of them holes read 152 against the rounded-down hundredth. -/
theorem old_over : oldReading 199 152 = 152 ∧ reading 199 152 ≤ 100 := by decide

end LocusCarcerum.Fragment
