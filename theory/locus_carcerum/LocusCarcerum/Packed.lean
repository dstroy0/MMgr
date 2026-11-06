/-!
# LocusCarcerum, the two tiers as one word

The state of a cellblock's two boundaries, the overruns and the two chains' ends, and the bumps as single
atomic steps: a bump's read-modify-write on its own boundary and its read of the other boundary are one step,
which is what one word carrying both boundaries is. Under that model the invariant stands through a round,
refusal is absorbing, taken bumps are contiguous and the settle restores rest.
-/

namespace LocusCarcerum.Packed

/-- The cellblock's state. `eP` is the end of the persistent chain and `tQ` the start of the temporary chain:
the chains themselves are abstracted to their ends, since every lemma about the cells reads off those. -/
structure State where
  S  : Nat
  E  : Nat
  T  : Nat
  OE : Nat
  OT : Nat
  eP : Nat
  tQ : Nat
deriving Repr, DecidableEq

/-- The invariant of a round: the boundaries are the chain ends displaced by the overruns, the chains do not
cross, the top is within the storage, and an overrun standing means the boundaries have crossed. The last
two conjuncts are the absorbing refusal. -/
def Inv (s : State) : Prop :=
  s.E = s.eP + s.OE ∧ s.T + s.OT = s.tQ ∧ s.eP ≤ s.tQ ∧ s.tQ ≤ s.S ∧
  (0 < s.OE → s.T < s.E) ∧ (0 < s.OT → s.T < s.E)

/-- Rest: the invariant with no overrun standing, so the boundaries are the chain ends. -/
def Rest (s : State) : Prop := Inv s ∧ s.OE = 0 ∧ s.OT = 0

/-- The initial state over `S` bytes. -/
def initial (S : Nat) : State := ⟨S, 0, S, 0, 0, 0, S⟩

theorem rest_initial (S : Nat) : Rest (initial S) := by
  simp [Rest, Inv, initial]

/-- A bump up of need `n`: one atomic step reads `E` and `T`, moves `E`, and either lays the cell at the old
`E` or adds the need to the overrun. -/
def bumpUp (n : Nat) (s : State) : State :=
  if s.E + n ≤ s.T then
    { s with E := s.E + n, eP := s.E + n }
  else
    { s with E := s.E + n, OE := s.OE + n }

/-- A bump down of need `n`, with `n ≤ T` (the wrap the C refuses by its third conjunct is outside this model). -/
def bumpDown (n : Nat) (s : State) : State :=
  if s.E ≤ s.T - n then
    { s with T := s.T - n, tQ := s.T - n }
  else
    { s with T := s.T - n, OT := s.OT + n }

/-- Settle: each boundary pulled back by its overrun, the overruns cleared. -/
def settle (s : State) : State :=
  { s with E := s.E - s.OE, T := s.T + s.OT, OE := 0, OT := 0 }

/-- Lemma 2 (refusal is absorbing), in the form the invariant carries: an overrun standing means every later
bump of either tier in the round is refused, since the boundaries have crossed and a bump only widens the
crossing. -/
theorem crossed_refuses_up (s : State) (n : Nat) (h : s.T < s.E) : ¬ (s.E + n ≤ s.T) := by omega

theorem crossed_refuses_down (s : State) (n : Nat) (h : s.T < s.E) : ¬ (s.E ≤ s.T - n) := by omega

/-- Lemma 3 (taken bumps are contiguous): a bump up that is taken reads `E` at the chain's end, with no overrun
standing on either tier. -/
theorem taken_up_at_end (s : State) (n : Nat) (hI : Inv s) (hτ : s.E + n ≤ s.T) :
    s.OE = 0 ∧ s.OT = 0 ∧ s.E = s.eP := by
  obtain ⟨h1, h2, h3, h4, h5, h6⟩ := hI
  have hOE : s.OE = 0 := by
    rcases Nat.eq_zero_or_pos s.OE with h | h
    · exact h
    · have := h5 h
      omega
  have hOT : s.OT = 0 := by
    rcases Nat.eq_zero_or_pos s.OT with h | h
    · exact h
    · have := h6 h
      omega
  exact ⟨hOE, hOT, by omega⟩

theorem taken_down_at_start (s : State) (n : Nat) (hn : 0 < n) (hnT : n ≤ s.T) (hI : Inv s)
    (hτ : s.E ≤ s.T - n) : s.OE = 0 ∧ s.OT = 0 ∧ s.T = s.tQ := by
  obtain ⟨h1, h2, h3, h4, h5, h6⟩ := hI
  have hOE : s.OE = 0 := by
    rcases Nat.eq_zero_or_pos s.OE with h | h
    · exact h
    · have := h5 h
      omega
  have hOT : s.OT = 0 := by
    rcases Nat.eq_zero_or_pos s.OT with h | h
    · exact h
    · have := h6 h
      omega
  exact ⟨hOE, hOT, by omega⟩

/-- Lemma 4, one step: a bump up preserves the invariant. -/
theorem inv_bumpUp (s : State) (n : Nat) (_hn : 0 < n) (hI : Inv s) : Inv (bumpUp n s) := by
  unfold bumpUp
  split
  · rename_i hτ
    obtain ⟨hOE, hOT, hE⟩ := taken_up_at_end s n hI hτ
    obtain ⟨h1, h2, h3, h4, h5, h6⟩ := hI
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_⟩ <;> simp only <;> omega
  · rename_i hτ
    obtain ⟨h1, h2, h3, h4, h5, h6⟩ := hI
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_⟩ <;> simp only <;> omega

/-- Lemma 4, one step: a bump down preserves the invariant. -/
theorem inv_bumpDown (s : State) (n : Nat) (hn : 0 < n) (hnT : n ≤ s.T) (hI : Inv s) :
    Inv (bumpDown n s) := by
  unfold bumpDown
  split
  · rename_i hτ
    obtain ⟨hOE, hOT, hT⟩ := taken_down_at_start s n hn hnT hI hτ
    obtain ⟨h1, h2, h3, h4, h5, h6⟩ := hI
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_⟩ <;> simp only <;> omega
  · rename_i hτ
    obtain ⟨h1, h2, h3, h4, h5, h6⟩ := hI
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_⟩ <;> simp only <;> omega

/-- The steps of a round: bumps up and bumps down, each of a positive need, a bump down within the top. -/
inductive Round : State → State → Prop
  | refl (s : State) : Round s s
  | up (s s' : State) (n : Nat) (hn : 0 < n) (h : Round s s') : Round s (bumpUp n s')
  | down (s s' : State) (n : Nat) (hn : 0 < n) (hnT : n ≤ s'.T) (h : Round s s') :
      Round s (bumpDown n s')

/-- Lemma 4: the invariant stands throughout a round. -/
theorem inv_round (s s' : State) (hI : Inv s) (h : Round s s') : Inv s' := by
  induction h with
  | refl => exact hI
  | up s2 n hn _ ih => exact inv_bumpUp s2 n hn ih
  | down s2 n hn hnT _ ih => exact inv_bumpDown s2 n hn hnT ih

/-- Lemma 5: the settle of a state in the invariant is at rest, with the boundaries the chain ends. -/
theorem rest_settle (s : State) (hI : Inv s) : Rest (settle s) := by
  obtain ⟨h1, h2, h3, h4, h5, h6⟩ := hI
  refine ⟨⟨?_, ?_, ?_, ?_, ?_, ?_⟩, rfl, rfl⟩ <;> simp only [settle] <;> omega

theorem settle_ends (s : State) (hI : Inv s) : (settle s).E = s.eP ∧ (settle s).T = s.tQ := by
  obtain ⟨h1, h2, h3, h4, h5, h6⟩ := hI
  simp only [settle]
  omega

/-- The theorem: from rest, a round of bumps and then a settle is at rest again, and the chains never cross,
which is the disjointness of every persistent cell from every temporary one. -/
theorem round_then_settle (s s' : State) (hR : Rest s) (h : Round s s') :
    Rest (settle s') ∧ s'.eP ≤ s'.tQ := by
  have hI := inv_round s s' hR.1 h
  exact ⟨rest_settle s' hI, hI.2.2.1⟩

end LocusCarcerum.Packed
