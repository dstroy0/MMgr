import Std.Tactic.BVDecide

/-!
# LocusCarcerum, the place of the lowest bit

A claim (`carcer_entry_claimed`) takes the lowest free bit of a word of `claimed` with `free & (0 - free)`. It
names the entry at the place of that bit, and `carcer_lowest` gives the place. The C widens the word to 64 bits.
A word of none reads as every bit on, and so its place is 0. Six steps then add 32, 16, 8, 4, 2 and 1, one for
each mask that holds the bit.

The theorems:

- `lowest_single`: the word of the one bit at place k reads k, for each k below 64.
- `lowest_on` and `lowest_least`: a word with a bit on has its bit at the place on, and no bit below the place on.
- `lowest_zero`: a word of none reads 0.
- `lowest_lt`: the place is below 64.
- `isolated_place`: the bit that the claim takes is the one bit at the place of the free word's lowest bit. Thus the
  bit the claim sets and the entry it names agree.
-/

namespace LocusCarcerum.Lowest

/-- The word widened, with every bit on for a word of none. -/
def whole (bits : BitVec 64) : BitVec 64 := bits ||| (0 - (bif bits == 0 then 1 else 0))

/-- The lowest bit of the widened word, alone. -/
def low (bits : BitVec 64) : BitVec 64 := whole bits &&& (0 - whole bits)

/-- One step: `add` where the mask holds the bit, 0 where it does not. -/
def step (bit mask add : BitVec 64) : BitVec 64 := bif (bit &&& mask) != 0 then add else 0

/-- The place of the lowest bit, as `carcer_lowest` adds it. -/
def lowest (bits : BitVec 64) : BitVec 64 :=
  step (low bits) 0xFFFFFFFF00000000#64 32 + step (low bits) 0xFFFF0000FFFF0000#64 16 +
    step (low bits) 0xFF00FF00FF00FF00#64 8 + step (low bits) 0xF0F0F0F0F0F0F0F0#64 4 +
    step (low bits) 0xCCCCCCCCCCCCCCCC#64 2 + step (low bits) 0xAAAAAAAAAAAAAAAA#64 1

theorem lowest_single (k : BitVec 64) (hk : k < 64) : lowest (1#64 <<< k) = k := by
  simp only [lowest, step, low, whole]
  bv_decide

theorem lowest_on (bits : BitVec 64) (h : bits ≠ 0) : (bits >>> lowest bits) &&& 1 = 1 := by
  simp only [lowest, step, low, whole]
  bv_decide

theorem lowest_least (bits : BitVec 64) : bits &&& ((1#64 <<< lowest bits) - 1) = 0 := by
  simp only [lowest, step, low, whole]
  bv_decide

theorem lowest_zero : lowest 0 = 0 := by
  simp only [lowest, step, low, whole]
  bv_decide

theorem lowest_lt (bits : BitVec 64) : lowest bits < 64 := by
  simp only [lowest, step, low, whole]
  bv_decide

theorem isolated_place (free : BitVec 64) (h : free ≠ 0) :
    free &&& (0 - free) = 1#64 <<< lowest free ∧ lowest (free &&& (0 - free)) = lowest free := by
  simp only [lowest, step, low, whole]
  bv_decide

end LocusCarcerum.Lowest
