import LocusCarcerum.Packed

/-!
# LocusCarcerum, the two tiers as two words

The same state as `Packed`, with a bump in two steps as two atomic words have it: the reservation, a
read-modify-write on the bump's own boundary, and the decision, a later read of the other boundary. Steps
of different bumps interleave. One interleaving, from the words of the witness in the document, refuses an earlier
reservation and takes a later one above it: the invariant breaks, and the settle puts the persistent end below
a laid cell.
-/

namespace LocusCarcerum.Split

open LocusCarcerum.Packed (State settle)

/-- The reservation of a bump up: the old `E` is the cell's offset should the bump be taken. -/
def reserveUp (n : Nat) (s : State) : Nat × State := (s.E, { s with E := s.E + n })

/-- The decision of a bump up reserved at `w`: the top read now, not when `E` moved. -/
def decideUp (w n : Nat) (s : State) : State :=
  if w + n ≤ s.T then { s with eP := w + n } else { s with OE := s.OE + n }

/-- The reservation of a bump down: the old `T`. -/
def reserveDown (n : Nat) (s : State) : Nat × State := (s.T, { s with T := s.T - n })

/-- The decision of a bump down reserved at `w`: the end read now. -/
def decideDown (w n : Nat) (s : State) : State :=
  if s.E ≤ w - n then { s with tQ := w - n } else { s with OT := s.OT + n }

/-- The state a stress run stood in before the interleaving below, read off the cellblock's words:
a mebibyte of storage, the persistent end at 0xfd4c0, the temporary top at 0xfd868, nothing overrun. -/
def before : State := ⟨0x100000, 0xfd4c0, 0xfd868, 0, 0, 0xfd4c0, 0xfd868⟩

theorem inv_before : Packed.Inv before := by
  unfold Packed.Inv before
  decide

/-- The witness interleaving: `A` reserves up 0x1e8, `B` reserves up 0xa8, `B` decides against a top
still at 0xfd868 and is taken, `C` reserves down 0x210, `A` decides against the top `C` moved and is refused,
`C` decides against the end `B` moved and is refused. -/
def after : State :=
  let (wA, s1) := reserveUp 0x1e8 before
  let (wB, s2) := reserveUp 0xa8 s1
  let s3 := decideUp wB 0xa8 s2
  let (wC, s4) := reserveDown 0x210 s3
  let s5 := decideUp wA 0x1e8 s4
  decideDown wC 0x210 s5

/-- The words after: `E` at 0xfd750, the laid persistent chain ending there, 0x1e8 overrun, the top at 0xfd658
with 0x210 overrun and the temporary chain still starting at 0xfd868. -/
theorem after_words : after = ⟨0x100000, 0xfd750, 0xfd658, 0x1e8, 0x210, 0xfd750, 0xfd868⟩ := by
  decide

/-- Lemma 6: the invariant fails after the round. `E - O_E` is 0xfd568, below the laid chain's end
0xfd750: the refused reservation of `A` lies below the taken cell of `B`, and the overrun counts it as if it
lay above. -/
theorem not_inv_after : ¬ Packed.Inv after := by
  rw [after_words]
  unfold Packed.Inv
  decide

/-- The settle then puts the persistent end 0x1e8 bytes below the laid cell's end: the cell `B` laid at
[0xfd6a8, 0xfd750) stands above the end, in the gap the next bumps take from. -/
theorem settle_below_laid : (settle after).E < after.eP := by
  rw [after_words]
  decide

/-- Lemma 2 fails for two words: `A`'s reservation precedes `B`'s, `A` is refused, `B` is taken. The proof of
Lemma 2 needs `B`'s read of the top to follow `A`'s; the two reads are separate steps and `B`'s came first. -/
theorem not_absorbing :
    let (wA, s1) := reserveUp 0x1e8 before
    let (wB, s2) := reserveUp 0xa8 s1
    let (_, s4) := reserveDown 0x210 (decideUp wB 0xa8 s2)
    wA < wB ∧ (wB + 0xa8 ≤ s2.T) ∧ ¬ (wA + 0x1e8 ≤ s4.T) := by
  decide

end LocusCarcerum.Split
