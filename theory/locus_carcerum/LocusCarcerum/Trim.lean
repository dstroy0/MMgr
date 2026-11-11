/-!
# LocusCarcerum, the trims a give leaves owed

The closed word as the writers and the gives share it. A writer holds the closed bit while it works alone. A give
sets its cell's header free, then sets the closed bit and its tier's trim bit with one fetch-or. Where the closed
bit was clear the give holds the cellblock, and where another holds it the give leaves.

A holder opens in one pass. One fetch-and takes the trim bits and the holder trims the tiers they name. Then a
second fetch-and clears the closed bit whatever trim bits stand. A trim bit a give sets between the two stays on
the open word. The next take, or the next read of the gap, holds the bit and runs it before its own pass.

A tier is `dirty` while a cell given back at its boundary stands untrimmed. The invariant: a dirty tier has its
trim bit set, or a give of that tier stands between its store and its fetch-or. The theorems: a cellblock with
no trim bit set and no give in flight has no dirty tier. The take that a holder runs first leaves no tier
dirty except through a give in flight.

`take` is the fetch-and and the walk as one step. In the C the walk follows the fetch-and with the closed bit
still held. A give between the two either stored its header before the walk, which then trims past it, or
sets its trim bit again. It sets the bit with its fetch-or, and that bit stays owed on the word for the next holder.
-/

namespace LocusCarcerum.Trim

/-- The two tiers. -/
inductive Tier where
  | persistent
  | temporary
  deriving DecidableEq, Repr

/-- Where a context stands: at rest, a give of a tier between its store and its fetch-or, or with the bit in hand. -/
inductive Place where
  | idle
  | stored (t : Tier)
  | holding
  deriving DecidableEq, Repr

/-- The closed bit, the trim bits, the dirty tiers and every context's place. -/
structure State where
  held : Bool
  owed : Tier → Bool
  dirty : Tier → Bool
  place : Nat → Place

/-- A context's place changed. -/
def moved (place : Nat → Place) (i : Nat) (p : Place) : Nat → Place := fun j => if j = i then p else place j

/-- A tier's bit changed. -/
def marked (bits : Tier → Bool) (t : Tier) (b : Bool) : Tier → Bool := fun u => if u = t then b else bits u

/-- The steps, each one atomic access to the closed word or one store of a header. -/
inductive Step : State → State → Prop where
  /-- A give sets its cell's header free: its tier is dirty. -/
  | store {s : State} (i : Nat) (t : Tier) (h : s.place i = .idle) :
      Step s { s with dirty := marked s.dirty t true, place := moved s.place i (.stored t) }
  /-- The give's fetch-or of the closed bit and its tier's trim bit: it holds where the closed bit was clear. -/
  | fetchOr {s : State} (i : Nat) (t : Tier) (h : s.place i = .stored t) :
      Step s { s with held := true, owed := marked s.owed t true,
                      place := moved s.place i (if s.held then .idle else .holding) }
  /-- A writer's shut: the closed bit taken where it was clear. -/
  | acquire {s : State} (i : Nat) (h : s.place i = .idle) (hc : s.held = false) :
      Step s { s with held := true, place := moved s.place i .holding }
  /-- The open's fetch-and: the trim bits taken and the tiers they name trimmed. -/
  | take {s : State} (i : Nat) (h : s.place i = .holding) :
      Step s { s with owed := fun _ => false, dirty := fun t => s.dirty t && !s.owed t }
  /-- The open's second fetch-and: the closed bit cleared, the trim bits left as they stand. -/
  | release {s : State} (i : Nat) (h : s.place i = .holding) :
      Step s { s with held := false, place := moved s.place i .idle }

/-- The cellblock as it starts: open, nothing owed, nothing dirty, every context at rest. -/
def start : State := ⟨false, fun _ => false, fun _ => false, fun _ => .idle⟩

/-- The states the steps reach from the start. -/
inductive Reach : State → Prop where
  | start : Reach start
  | step {s s' : State} : Reach s → Step s s' → Reach s'

/-- The invariant. A dirty tier has a trim owed or a give in flight. A holder means someone holds the bit, and
one context holds at a time. -/
structure Inv (s : State) : Prop where
  dirtyCovered : ∀ t, s.dirty t = true → s.owed t = true ∨ ∃ i, s.place i = .stored t
  holderHeld : ∀ i, s.place i = .holding → s.held = true
  oneHolder : ∀ i j, s.place i = .holding → s.place j = .holding → i = j

theorem inv_start : Inv start where
  dirtyCovered := by intro t h; simp [start] at h
  holderHeld := by intro i h; simp [start] at h
  oneHolder := by intro i j h; simp [start] at h

theorem moved_self (place : Nat → Place) (i : Nat) (p : Place) : moved place i p i = p := by
  simp [moved]

theorem moved_other (place : Nat → Place) (i j : Nat) (p : Place) (h : j ≠ i) : moved place i p j = place j := by
  simp [moved, h]

theorem inv_step {s s' : State} (hs : Inv s) (st : Step s s') : Inv s' := by
  cases st with
  | store i t h =>
    constructor
    · intro u hu
      by_cases hut : u = t
      · subst hut
        exact Or.inr ⟨i, moved_self _ _ _⟩
      · have hd : s.dirty u = true := by simpa [marked, hut] using hu
        rcases hs.dirtyCovered u hd with ho | ⟨j, hj⟩
        · exact Or.inl ho
        · refine Or.inr ⟨j, ?_⟩
          have hji : j ≠ i := by intro e; subst e; rw [h] at hj; cases hj
          simpa [moved_other _ _ _ _ hji] using hj
    · intro j hj
      by_cases hji : j = i
      · subst hji; simp [moved] at hj
      · exact hs.holderHeld j (by simpa [moved_other _ _ _ _ hji] using hj)
    · intro a b ha hb
      have hai : a ≠ i := by intro e; subst e; simp [moved] at ha
      have hbi : b ≠ i := by intro e; subst e; simp [moved] at hb
      exact hs.oneHolder a b (by simpa [moved_other _ _ _ _ hai] using ha)
        (by simpa [moved_other _ _ _ _ hbi] using hb)
  | fetchOr i t h =>
    constructor
    · intro u hu
      by_cases hut : u = t
      · subst hut; exact Or.inl (by simp [marked])
      · rcases hs.dirtyCovered u hu with ho | ⟨j, hj⟩
        · exact Or.inl (by simp [marked, hut, ho])
        · refine Or.inr ⟨j, ?_⟩
          have hji : j ≠ i := by
            intro e; subst e; rw [h] at hj; cases hj; exact hut rfl
          simpa [moved_other _ _ _ _ hji] using hj
    · intro _ _; rfl
    · intro a b ha hb
      cases hheld : s.held with
      | true =>
        have hai : a ≠ i := by intro e; subst e; simp [moved, hheld] at ha
        have hbi : b ≠ i := by intro e; subst e; simp [moved, hheld] at hb
        exact hs.oneHolder a b (by simpa [moved_other _ _ _ _ hai] using ha)
          (by simpa [moved_other _ _ _ _ hbi] using hb)
      | false =>
        have none : ∀ k, k ≠ i → moved s.place i (if s.held then .idle else .holding) k = .holding → False := by
          intro k hki hk
          have := hs.holderHeld k (by simpa [moved_other _ _ _ _ hki] using hk)
          rw [hheld] at this; cases this
        by_cases hai : a = i
        · by_cases hbi : b = i
          · rw [hai, hbi]
          · exact (none b hbi hb).elim
        · exact (none a hai ha).elim
  | acquire i h hc =>
    constructor
    · intro u hu
      rcases hs.dirtyCovered u hu with ho | ⟨j, hj⟩
      · exact Or.inl ho
      · refine Or.inr ⟨j, ?_⟩
        have hji : j ≠ i := by intro e; subst e; rw [h] at hj; cases hj
        simpa [moved_other _ _ _ _ hji] using hj
    · intro _ _; rfl
    · intro a b ha hb
      have none : ∀ k, k ≠ i → moved s.place i .holding k = .holding → False := by
        intro k hki hk
        have := hs.holderHeld k (by simpa [moved_other _ _ _ _ hki] using hk)
        rw [hc] at this; cases this
      by_cases hai : a = i
      · by_cases hbi : b = i
        · rw [hai, hbi]
        · exact (none b hbi hb).elim
      · exact (none a hai ha).elim
  | take i h =>
    constructor
    · intro u hu
      have hd : s.dirty u = true := by simp at hu; exact hu.1
      have hno : s.owed u = false := by simp at hu; exact hu.2
      rcases hs.dirtyCovered u hd with ho | hj
      · rw [hno] at ho; cases ho
      · exact Or.inr hj
    · exact hs.holderHeld
    · exact hs.oneHolder
  | release i h =>
    constructor
    · intro u hu
      rcases hs.dirtyCovered u hu with hou | ⟨j, hj⟩
      · exact Or.inl hou
      · refine Or.inr ⟨j, ?_⟩
        have hji : j ≠ i := by intro e; subst e; rw [h] at hj; cases hj
        simpa [moved_other _ _ _ _ hji] using hj
    · intro j hj
      have hji : j ≠ i := by intro e; subst e; simp [moved] at hj
      have := hs.oneHolder i j h (by simpa [moved_other _ _ _ _ hji] using hj)
      exact (hji this.symm).elim
    · intro a b ha hb
      have hai : a ≠ i := by intro e; subst e; simp [moved] at ha
      have hbi : b ≠ i := by intro e; subst e; simp [moved] at hb
      exact hs.oneHolder a b (by simpa [moved_other _ _ _ _ hai] using ha)
        (by simpa [moved_other _ _ _ _ hbi] using hb)

theorem inv_reach {s : State} (h : Reach s) : Inv s := by
  induction h with
  | start => exact inv_start
  | step _ st ih => exact inv_step ih st

/-- A cellblock has no dirty tier when its word holds no trim bit and no give is in flight. A trim walks past
each cell that a give frees at a boundary, or the word keeps the trim bit of its tier. -/
theorem open_trimmed {s : State} (h : Reach s) (hno : ∀ t, s.owed t = false)
    (hquiet : ∀ i t, s.place i ≠ .stored t) : ∀ t, s.dirty t = false := by
  intro t
  have hi := inv_reach h
  cases hd : s.dirty t with
  | false => rfl
  | true =>
    rcases hi.dirtyCovered t hd with ho | ⟨i, hit⟩
    · rw [hno t] at ho; cases ho
    · exact (hquiet i t hit).elim

/-- A holder runs the take first. After the take, each dirty tier has a give in flight. Thus the walk after the
take runs each trim that the word kept. -/
theorem taken_trimmed {s s' : State} (h : Reach s) (i : Nat) (hi : s.place i = .holding)
    (hs' : s' = { s with owed := fun _ => false, dirty := fun t => s.dirty t && !s.owed t }) :
    ∀ t, s'.dirty t = true → ∃ j, s'.place j = .stored t := by
  intro t hd
  have hinv := inv_step (inv_reach h) (Step.take i hi)
  rw [← hs'] at hinv
  rcases hinv.dirtyCovered t hd with ho | hj
  · rw [hs'] at ho; simp at ho
  · exact hj

end LocusCarcerum.Trim
