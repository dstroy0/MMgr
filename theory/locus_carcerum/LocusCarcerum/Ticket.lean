import EmbeddedTypes.TicketRing
import EmbeddedTypes.Claim
import EmbeddedTypes.Flags
import LocusCarcerum.Give
import LocusCarcerum.OwnerWord
import LocusCarcerum.Entry

/-!
# LocusCarcerum, the cellblock over the ticket ring

embedded_types proves a ring of slots that a ticket indexes (`TicketRing.lean`), the claim of a slot by one
compare-and-swap (`Claim.lean`), and the flag matrix of a stamp (`Flags.lean`). The cellblock carries the same
three devices under other names, and this file states each one as an instance of the proof it imports.

- The owner word is a stamp. The completion fetch-or of a give is `Flags.transition` with the completion bit as
  the flag. `complete_once` is `Flags.set_once`: the fetch-or that finds the bit clear changes the word, and a
  second one finds it set. `complete_keeps_place`, `complete_keeps_sequence` and `complete_keeps_signal` are
  `Flags.set_keeps_others`: the completion bit changes no other field.

- A give is a claim. A give that names the cell's owner is a claim with the stamp of the slot. A give that
  names another owner is a claim with another stamp. `give_counts_as_claims` maps every sequence of gives on
  one cell onto `Claim.claims` on one slot: the bytes counted off are the cell's size times the claims that
  landed. `give_bytes_once` is then `Claim.claims_at_most_one`: the bytes come off once at the most.

- The owner's sequence is a ticket. `OwnerWord.lean` reads the sequence in `b` bits of the word, and
  `sequence_window_distinct` is `TicketRing.window_distinct`: two takes fewer than `2^b` apart have sequences
  of their own. `sequence_roll` is `TicketRing.place_roll`: a counter that rolls at 32 bits gives the same field
  as the counter with no roll, for `b ≤ 32`.

- A movable entry is a slot. The claim bit of an entry, set with one fetch-or, is `Claim.push`. `entry_push`
  states that the fetch-or that finds the bit clear is the push that lands. `entry_push_once` is
  `Claim.push_once`.

- The release of a movable cell runs inside a pass. `release_drained` is `Claim.claim_drained`: a move, which is
  a writer, drains the turnstiles, and the drain waits for the pass that released an entry.
-/

namespace LocusCarcerum.Ticket

open EmbeddedTypes

/-! ## The owner word as a stamp -/

/-- The completion fetch-or of a give, as a transition of the flag matrix. -/
theorem complete_is_transition (w : BitVec 64) :
    w ||| OwnerWord.complete64 = Flags.transition w OwnerWord.complete64 := rfl

/-- The fetch-or that finds the completion bit clear changes the word. So the compare that expects the word
before it lands once, and a second give finds the bit set. -/
theorem complete_once (w : BitVec 64) (h : w &&& OwnerWord.complete64 = 0) :
    Flags.transition w OwnerWord.complete64 ≠ w :=
  Flags.set_once w OwnerWord.complete64 (by decide) h

/-- The completion bit changes no bit of the place field. -/
theorem complete_keeps_place (w : BitVec 64) :
    Flags.transition w OwnerWord.complete64 &&& OwnerWord.placeMask64 = w &&& OwnerWord.placeMask64 :=
  Flags.set_keeps_others w OwnerWord.complete64 OwnerWord.placeMask64 (by decide)

/-- The completion bit changes no bit of the sequence field. -/
theorem complete_keeps_sequence (w : BitVec 64) :
    Flags.transition w OwnerWord.complete64 &&& OwnerWord.sequenceMask64 = w &&& OwnerWord.sequenceMask64 :=
  Flags.set_keeps_others w OwnerWord.complete64 OwnerWord.sequenceMask64 (by decide)

/-- The completion bit changes no signal bit. -/
theorem complete_keeps_signal (w : BitVec 64) :
    Flags.transition w OwnerWord.complete64 &&& OwnerWord.signal64 = w &&& OwnerWord.signal64 :=
  Flags.set_keeps_others w OwnerWord.complete64 OwnerWord.signal64 (by decide)

/-! ## A give as a claim -/

/-- The stamp a give claims with: the slot's own stamp where the give names the cell's owner, and another
stamp where it names another owner. -/
def stampOf (permit : Bool) (s : Nat) : Nat := if permit then s else s + 1

/-- The slot a cell's completion bit stands for: pushed while the bit is clear, consumed by some pass once the bit
holds. -/
def Rel (complete : Bool) (slot : Claim.Slot) (s : Nat) : Prop :=
  (complete = false ∧ slot = .pushed s) ∨ (complete = true ∧ ∃ q, slot = .consumed s q)

/-- One give is one claim. The claim lands exactly where the give has its permit and the bit is clear, and the
slot after stands for the bit after. -/
theorem claim_step (c : Bool) (slot : Claim.Slot) (s : Nat) (hr : Rel c slot s) (p : Bool) (q : Nat) :
    (Claim.claim slot (stampOf p s) q).2 = (p && !c) ∧ Rel (p || c) (Claim.claim slot (stampOf p s) q).1 s := by
  rcases hr with ⟨hc, hs⟩ | ⟨hc, q0, hs⟩
  · subst hc hs
    cases p
    · simp [stampOf, Claim.claim_other_fails s (s + 1) q (by omega), Rel]
    · simp [stampOf, Claim.claim_pushed, Rel]
  · subst hc hs
    cases p <;> simp [Claim.claim, Rel]

/-- Every sequence of gives on one cell is a sequence of claims on one slot. The bytes counted off are the
cell's size times the claims that landed. -/
theorem give_counts_as_claims (size s : Nat) (c : Bool) (slot : Claim.Slot) (hr : Rel c slot s)
    (gives : List (Bool × Nat)) :
    Give.claims size c (gives.map Prod.fst) =
      size * (Claim.claims slot (gives.map fun g => (stampOf g.1 s, g.2))).2 := by
  induction gives generalizing c slot with
  | nil => simp [Give.claims, Claim.claims]
  | cons g rest ih =>
    obtain ⟨p, q⟩ := g
    obtain ⟨hland, hrel⟩ := claim_step c slot s hr p q
    have hbit : (Give.claim size p c).2 = (p || c) := by
      cases p <;> cases c <;> simp [Give.claim]
    have hbytes : (Give.claim size p c).1 = if (p && !c) then size else 0 := by
      cases p <;> cases c <;> simp [Give.claim]
    rw [← hbit] at hrel
    have := ih (Give.claim size p c).2 (Claim.claim slot (stampOf p s) q).1 hrel
    simp only [List.map, Give.claims, Claim.claims]
    rw [this, hbytes, hland]
    cases p <;> cases c <;> simp [Nat.mul_add, Nat.add_comm]

/-- The bytes a sequence of gives counts off one cell are the cell's size at the most, since at most one claim
lands. -/
theorem give_bytes_once (size s : Nat) (gives : List (Bool × Nat)) :
    Give.claims size false (gives.map Prod.fst) ≤ size := by
  rw [give_counts_as_claims size s false (.pushed s) (Or.inl ⟨rfl, rfl⟩) gives]
  have := Claim.claims_at_most_one (.pushed s) (gives.map fun g => (stampOf g.1 s, g.2))
  calc size * (Claim.claims (.pushed s) (gives.map fun g => (stampOf g.1 s, g.2))).2 ≤ size * 1 :=
        Nat.mul_le_mul_left size this
    _ = size := Nat.mul_one size

/-! ## The owner's sequence as a ticket -/

/-- Two takes of one owner fewer than `2^b` apart have sequences of their own in a field of `b` bits. -/
theorem sequence_window_distinct (b c1 c2 : Nat) (hlt : c1 < c2) (hwin : c2 - c1 < 2 ^ b) :
    BitVec.ofNat b c1 ≠ BitVec.ofNat b c2 := by
  intro h
  have := congrArg BitVec.toNat h
  simp only [BitVec.toNat_ofNat] at this
  exact TicketRing.window_distinct b c1 c2 hlt hwin (by simpa [TicketRing.place] using this)

/-- A counter that rolls at 32 bits gives the same sequence field as the counter with no roll, for a field of at
most 32 bits. -/
theorem sequence_roll (b c : Nat) (hb : b ≤ 32) : BitVec.ofNat b (c % 2 ^ 32) = BitVec.ofNat b c := by
  apply BitVec.eq_of_toNat_eq
  simp only [BitVec.toNat_ofNat]
  exact TicketRing.place_roll b c hb

/-! ## A movable entry as a slot -/

/-- The slot an entry's claim bit stands for: empty while the bit is clear, pushed once the bit holds. -/
def entrySlot (claimed : Bool) : Claim.Slot := if claimed then .pushed 0 else .empty

/-- The fetch-or on the claim bit lands exactly where the bit was clear, which is the push that lands on an empty
slot. -/
theorem entry_push (claimed : Bool) : (Claim.push (entrySlot claimed) 0).2 = !claimed := by
  cases claimed <;> simp [entrySlot, Claim.push]

/-- A second claim of one entry fails, as a second push on one slot fails. -/
theorem entry_push_once (claimed : Bool) :
    (Claim.push (Claim.push (entrySlot claimed) 0).1 0).2 = false :=
  Claim.push_once (entrySlot claimed) 0 0

/-! ## The release inside a pass -/

/-- The release of a movable cell is the work of a pass, and so it is a claim by the pass that holds the lane. A
move is a writer, which drains the turnstiles: a drain that read the lane held at the release waits until that
pass ends. -/
theorem release_drained {s t : Claim.State} (st : Claim.Step s t) (v p : Nat) (i : Manifold.Drain.Inv s.lane)
    (hs : s.slot ≠ .consumed v p) (ht : t.slot = .consumed v p) {u : Manifold.Drain.State}
    (f : Manifold.Drain.Follows s.lane u) (ho : Manifold.Drain.over s.lane u = true) : u.ended p = true :=
  Claim.claim_drained st v p i hs ht f ho

end LocusCarcerum.Ticket
