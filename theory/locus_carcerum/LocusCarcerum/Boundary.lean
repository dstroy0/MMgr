/-!
# LocusCarcerum, the boundary a give reads

One tier's cells by place: the cell at `top` stands at the tier's boundary, every cell from `top` up to `size` is
live, and `free k` says whether the cell at `k` is given back. A writer's trim walks up from `top` one header at a
time, stops at the first busy cell or at `size`, and only then stores the place it stopped at as the new `top`.

A give stores its cell's header free, then reads the closed bit and `top`, and goes on to the fetch-or of
`Trim.lean` where the closed bit is set or its cell stands at `top`. The open takes the trim bit once, walks, and
clears the closed bit whatever trim bit stands. The theorems: a cellblock with no trim owed and every context at
rest has no free cell at its top, and the next holder's take walks from `top` where a trim stands owed.

The give of the earlier protocol read `top` before its store. `old_loses` is the interleaving the race program
read: the give of the cell above the top reads `top` while the cell below still stands, the give of that cell
trims, its walk stops at the first cell still busy, and the first give then stores that cell free and leaves.

The model is sequentially consistent. In the C a full fence stands between a give's store and its reads, and
between a writer's fetch on the closed word and the headers it reads, and each read sees every store before it.
The tier modelled is the temporary one; the persistent tier is the same with the walk running down.
-/

namespace LocusCarcerum.Boundary

/-- Where a context stands: at rest, a give between its store and its read, a give bound for its fetch-or, or a
writer: holding the bit, walking at a place, stopped at a place, or done with its walk. -/
inductive Place where
  | idle
  | stored (c : Nat)
  | fetching
  | holding
  | walking (k : Nat)
  | stopped (k : Nat)
  | published
  deriving DecidableEq, Repr

/-- The closed bit, the tier's trim bit, the boundary, the free cells and every context's place. -/
structure State where
  held : Bool
  owed : Bool
  top : Nat
  free : Nat → Bool
  place : Nat → Place

/-- A context's place changed. -/
def moved (place : Nat → Place) (i : Nat) (p : Place) : Nat → Place := fun j => if j = i then p else place j

/-- A cell's header changed. -/
def marked (free : Nat → Bool) (c : Nat) (b : Bool) : Nat → Bool := fun j => if j = c then b else free j

/-- A place a writer stands in while it holds the closed bit. -/
def Writer : Place → Prop
  | .holding => True
  | .walking _ => True
  | .stopped _ => True
  | .published => True
  | _ => False

section

variable (size : Nat)

/-- The steps, each one access to the closed word, to `top` or to one header. -/
inductive Step : State → State → Prop where
  /-- A give stores its live cell's header free. -/
  | store {s : State} (i c : Nat) (h : s.place i = .idle) (hl : s.top ≤ c) (hc : c < size) (hb : s.free c = false) :
      Step s { s with free := marked s.free c true, place := moved s.place i (.stored c) }
  /-- The give reads the closed bit and `top` after its store, and goes to its fetch-or where the bit is set or its
  cell stands at `top`. -/
  | read {s : State} (i c : Nat) (h : s.place i = .stored c) :
      Step s { s with place := moved s.place i (if s.held || decide (s.top = c) then .fetching else .idle) }
  /-- The give's fetch-or of the closed bit and the trim bit: it holds where the closed bit was clear. -/
  | fetchOr {s : State} (i : Nat) (h : s.place i = .fetching) :
      Step s { s with held := true, owed := true, place := moved s.place i (if s.held then .idle else .holding) }
  /-- A writer's shut: the closed bit taken where it was clear. -/
  | acquire {s : State} (i : Nat) (h : s.place i = .idle) (hc : s.held = false) :
      Step s { s with held := true, place := moved s.place i .holding }
  /-- The open's first fetch-and, once a pass: the trim bit taken, and the walk begun at `top` where it was set. -/
  | take {s : State} (i : Nat) (h : s.place i = .holding) :
      Step s { s with owed := false, place := moved s.place i (if s.owed then .walking s.top else .published) }
  /-- The walk reads a free header and steps past it. -/
  | advance {s : State} (i k : Nat) (h : s.place i = .walking k) (hf : s.free k = true) (hk : k < size) :
      Step s { s with place := moved s.place i (.walking (k + 1)) }
  /-- The walk reads a busy header, or reaches the tier's end, and stops there. -/
  | stop {s : State} (i k : Nat) (h : s.place i = .walking k) (hb : s.free k = false ∨ size ≤ k) :
      Step s { s with place := moved s.place i (.stopped k) }
  /-- The walk's place stored as `top`. -/
  | publish {s : State} (i k : Nat) (h : s.place i = .stopped k) :
      Step s { s with top := k, place := moved s.place i .published }
  /-- The open's second fetch-and: the closed bit cleared, the trim bit left as it stands. -/
  | release {s : State} (i : Nat) (h : s.place i = .published) :
      Step s { s with held := false, place := moved s.place i .idle }
  /-- A take through a pass: a busy cell laid below `top`, which moves down to it. A pass begun before the closed bit
  is set can still bump until the writer's drain, and the drain ends before any walk begins; a bump lays a busy
  cell at `top` and leaves no free one there, and the model lets bumps run while the cellblock is open. -/
  | bump {s : State} (i : Nat) (h : s.place i = .idle) (hc : s.held = false) (ht : 0 < s.top) :
      Step s { s with top := s.top - 1, free := marked s.free (s.top - 1) false }

/-- The cellblock as it is laid out: open, nothing owed, every cell taken, every context at rest. -/
def start : State := ⟨false, false, size, fun _ => false, fun _ => .idle⟩

/-- The states the steps reach from the start. -/
inductive Reach : State → Prop where
  | start : Reach (start size)
  | step {s s' : State} : Reach s → Step size s s' → Reach s'

/-- The invariant. A writer means the bit is held; one writer at a time. A walk stands
at or above `top` with every cell it passed free. A stop at a live cell that is free again since has a give of it
in flight or its trim owed. A free cell at `top` has its trim owed, a give in flight, or a writer at its walk. -/
structure Inv (s : State) : Prop where
  writerHeld : ∀ i, Writer (s.place i) → s.held = true
  oneWriter : ∀ i j, Writer (s.place i) → Writer (s.place j) → i = j
  walked : ∀ i k, s.place i = .walking k → s.top ≤ k ∧ ∀ j, s.top ≤ j → j < k → s.free j = true
  stoppedCovered : ∀ i k, s.place i = .stopped k → k < size → s.free k = true →
    s.owed = true ∨ (∃ j, s.place j = .stored k) ∨ (∃ j, s.place j = .fetching)
  topCovered : s.top < size → s.free s.top = true →
    s.owed = true ∨ (∃ j, s.place j = .stored s.top) ∨ (∃ j, s.place j = .fetching) ∨
      (∃ j k, s.place j = .walking k) ∨ (∃ j k, s.place j = .stopped k)

end

theorem moved_self (place : Nat → Place) (i : Nat) (p : Place) : moved place i p i = p := by
  simp [moved]

theorem moved_other (place : Nat → Place) (i j : Nat) (p : Place) (h : j ≠ i) :
    moved place i p j = place j := by
  simp [moved, h]

theorem inv_start (size : Nat) : Inv size (start size) where
  writerHeld := by intro i h; simp [start, Writer] at h
  oneWriter := by intro i j h; simp [start, Writer] at h
  walked := by intro i k h; simp [start] at h
  stoppedCovered := by intro i k h; simp [start] at h
  topCovered := by intro h; simp [start] at h

/-- A context other than the stepping one keeps its place. -/
theorem kept {place : Nat → Place} {i j : Nat} {p q : Place} (hj : moved place i p j = q) (hji : j ≠ i) :
    place j = q := by
  rwa [moved_other _ _ _ _ hji] at hj

/-- Where the stepping context's new place is no writer, every writer after the step was one before. -/
theorem writer_before {place : Nat → Place} {i j : Nat} {p : Place} (hp : ¬ Writer p)
    (hj : Writer (moved place i p j)) : j ≠ i ∧ Writer (place j) := by
  by_cases hji : j = i
  · subst hji; rw [moved_self] at hj; exact (hp hj).elim
  · exact ⟨hji, by simpa [moved, hji] using hj⟩

theorem inv_step {size : Nat} {s s' : State} (hs : Inv size s) (st : Step size s s') : Inv size s' := by
  cases st with
  | store i c h hl hc hb =>
    have hnw : ¬ Writer (Place.stored c) := by simp [Writer]
    refine ⟨?_, ?_, ?_, ?_, ?_⟩
    · intro j hj; exact hs.writerHeld j (writer_before hnw hj).2
    · intro a b ha hb'
      exact hs.oneWriter a b (writer_before hnw ha).2 (writer_before hnw hb').2
    · intro j k hj
      have hji : j ≠ i := by intro e; subst e; simp [moved] at hj
      obtain ⟨hk, hw⟩ := hs.walked j k (kept hj hji)
      refine ⟨hk, fun m hm hmk => ?_⟩
      by_cases hmc : m = c
      · simp [marked, hmc]
      · simp [marked, hmc, hw m hm hmk]
    · intro j k hj hk hf
      have hji : j ≠ i := by intro e; subst e; simp [moved] at hj
      by_cases hkc : k = c
      · subst hkc; exact Or.inr (Or.inl ⟨i, moved_self _ _ _⟩)
      · have hf' : s.free k = true := by simpa [marked, hkc] using hf
        rcases hs.stoppedCovered j k (kept hj hji) hk hf' with ho | ⟨m, hm⟩ | ⟨m, hm⟩
        · exact Or.inl ho
        · have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
          exact Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩)
        · have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
          exact Or.inr (Or.inr ⟨m, by simp [moved, hmi, hm]⟩)
    · intro ht hf
      by_cases htc : s.top = c
      · exact Or.inr (Or.inl ⟨i, by simp [moved, htc]⟩)
      · have hf' : s.free s.top = true := by simpa [marked, htc] using hf
        rcases hs.topCovered ht hf' with ho | ⟨m, hm⟩ | ⟨m, hm⟩ | ⟨m, k, hm⟩ | ⟨m, k, hm⟩
        · exact Or.inl ho
        all_goals
          have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
        · exact Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩)
        · exact Or.inr (Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩))
        · exact Or.inr (Or.inr (Or.inr (Or.inl ⟨m, k, by simp [moved, hmi, hm]⟩)))
        · exact Or.inr (Or.inr (Or.inr (Or.inr ⟨m, k, by simp [moved, hmi, hm]⟩)))
  | read i c h =>
    have hnw : ¬ Writer (if s.held || decide (s.top = c) then Place.fetching else Place.idle) := by
      split <;> simp [Writer]
    refine ⟨?_, ?_, ?_, ?_, ?_⟩
    · intro j hj; exact hs.writerHeld j (writer_before hnw hj).2
    · intro a b ha hb'
      exact hs.oneWriter a b (writer_before hnw ha).2 (writer_before hnw hb').2
    · intro j k hj
      have hji : j ≠ i := by intro e; subst e; dsimp only at hj; rw [moved_self] at hj; split at hj <;> cases hj
      exact hs.walked j k (kept hj hji)
    · intro j k hj hk hf
      have hji : j ≠ i := by intro e; subst e; dsimp only at hj; rw [moved_self] at hj; split at hj <;> cases hj
      have hjs := kept hj hji
      have hheld : s.held = true := hs.writerHeld j (by rw [hjs]; trivial)
      rcases hs.stoppedCovered j k hjs hk hf with ho | ⟨m, hm⟩ | ⟨m, hm⟩
      · exact Or.inl ho
      · by_cases hmi : m = i
        · subst hmi
          exact Or.inr (Or.inr ⟨m, by simp [moved, hheld]⟩)
        · exact Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩)
      · have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
        exact Or.inr (Or.inr ⟨m, by simp [moved, hmi, hm]⟩)
    · intro ht hf
      rcases hs.topCovered ht hf with ho | ⟨m, hm⟩ | ⟨m, hm⟩ | ⟨m, k, hm⟩ | ⟨m, k, hm⟩
      · exact Or.inl ho
      · by_cases hmi : m = i
        · subst hmi
          rw [h] at hm
          have hc : c = s.top := by cases hm; rfl
          exact Or.inr (Or.inr (Or.inl ⟨m, by simp [moved, hc]⟩))
        · exact Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩)
      all_goals
        have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
      · exact Or.inr (Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩))
      · exact Or.inr (Or.inr (Or.inr (Or.inl ⟨m, k, by simp [moved, hmi, hm]⟩)))
      · exact Or.inr (Or.inr (Or.inr (Or.inr ⟨m, k, by simp [moved, hmi, hm]⟩)))
  | fetchOr i h =>
    refine ⟨fun _ _ => rfl, ?_, ?_, ?_, ?_⟩
    · intro a b ha hb'
      cases hheld : s.held with
      | true =>
        have hnw : ¬ Writer (if s.held then Place.idle else Place.holding) := by simp [hheld, Writer]
        exact hs.oneWriter a b (writer_before hnw ha).2 (writer_before hnw hb').2
      | false =>
        have none : ∀ k, k ≠ i → Writer (moved s.place i (if s.held then .idle else .holding) k) → False := by
          intro k hki hk
          have := hs.writerHeld k (by simpa [moved, hki] using hk)
          rw [hheld] at this; cases this
        by_cases hai : a = i
        · by_cases hbi : b = i
          · rw [hai, hbi]
          · exact (none b hbi hb').elim
        · exact (none a hai ha).elim
    · intro j k hj
      have hji : j ≠ i := by intro e; subst e; dsimp only at hj; rw [moved_self] at hj; split at hj <;> cases hj
      exact hs.walked j k (kept hj hji)
    · intro _ _ _ _ _; exact Or.inl rfl
    · intro _ _; exact Or.inl rfl
  | acquire i h hc =>
    refine ⟨fun _ _ => rfl, ?_, ?_, ?_, ?_⟩
    · intro a b ha hb'
      have none : ∀ k, k ≠ i → Writer (moved s.place i .holding k) → False := by
        intro k hki hk
        have := hs.writerHeld k (by simpa [moved, hki] using hk)
        rw [hc] at this; cases this
      by_cases hai : a = i
      · by_cases hbi : b = i
        · rw [hai, hbi]
        · exact (none b hbi hb').elim
      · exact (none a hai ha).elim
    · intro j k hj
      have hji : j ≠ i := by intro e; subst e; simp [moved] at hj
      exact hs.walked j k (kept hj hji)
    · intro j k hj hk hf
      have hji : j ≠ i := by intro e; subst e; simp [moved] at hj
      rcases hs.stoppedCovered j k (kept hj hji) hk hf with ho | ⟨m, hm⟩ | ⟨m, hm⟩
      · exact Or.inl ho
      all_goals
        have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
      · exact Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩)
      · exact Or.inr (Or.inr ⟨m, by simp [moved, hmi, hm]⟩)
    · intro ht hf
      rcases hs.topCovered ht hf with ho | ⟨m, hm⟩ | ⟨m, hm⟩ | ⟨m, k, hm⟩ | ⟨m, k, hm⟩
      · exact Or.inl ho
      all_goals
        have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
      · exact Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩)
      · exact Or.inr (Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩))
      · exact Or.inr (Or.inr (Or.inr (Or.inl ⟨m, k, by simp [moved, hmi, hm]⟩)))
      · exact Or.inr (Or.inr (Or.inr (Or.inr ⟨m, k, by simp [moved, hmi, hm]⟩)))
  | take i h =>
    have hiw : Writer (s.place i) := by rw [h]; trivial
    have hheld : s.held = true := hs.writerHeld i hiw
    have only : ∀ j, j ≠ i → ¬ Writer (s.place j) := fun j hji hj => hji (hs.oneWriter j i hj hiw)
    have hnew : Writer (if s.owed then Place.walking s.top else Place.published) := by split <;> trivial
    refine ⟨?_, ?_, ?_, ?_, ?_⟩
    · intro _ _; exact hheld
    · intro a b ha hb'
      have : ∀ k, Writer (moved s.place i (if s.owed then .walking s.top else .published) k) → k = i := by
        intro k hk
        by_cases hki : k = i
        · exact hki
        · exact (only k hki (by simpa [moved, hki] using hk)).elim
      rw [this a ha, this b hb']
    · intro j k hj
      by_cases hji : j = i
      · subst hji; dsimp only at hj; rw [moved_self] at hj
        split at hj
        · cases hj; exact ⟨Nat.le_refl _, fun m hm hmk => absurd hmk (Nat.not_lt.mpr hm)⟩
        · cases hj
      · exact (only j hji (by rw [kept hj hji]; trivial)).elim
    · intro j k hj
      by_cases hji : j = i
      · subst hji; dsimp only at hj; rw [moved_self] at hj; split at hj <;> cases hj
      · exact (only j hji (by rw [kept hj hji]; trivial)).elim
    · intro ht hf
      cases ho : s.owed with
      | true => exact Or.inr (Or.inr (Or.inr (Or.inl ⟨i, s.top, by simp [moved]⟩)))
      | false =>
        rcases hs.topCovered ht hf with hso | ⟨m, hm⟩ | ⟨m, hm⟩ | ⟨m, k, hm⟩ | ⟨m, k, hm⟩
        · rw [ho] at hso; cases hso
        · have hmi : m ≠ i := by intro e; subst e; rw [hm] at hiw; exact hiw
          exact Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩)
        · have hmi : m ≠ i := by intro e; subst e; rw [hm] at hiw; exact hiw
          exact Or.inr (Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩))
        · have hmi : m = i :=
            Classical.byContradiction fun hmi => only m hmi (by rw [hm]; trivial)
          subst hmi; rw [h] at hm; cases hm
        · have hmi : m = i :=
            Classical.byContradiction fun hmi => only m hmi (by rw [hm]; trivial)
          subst hmi; rw [h] at hm; cases hm
  | advance i k h hf hk =>
    have hiw : Writer (s.place i) := by rw [h]; trivial
    have only : ∀ j, j ≠ i → ¬ Writer (s.place j) := fun j hji hj => hji (hs.oneWriter j i hj hiw)
    refine ⟨?_, ?_, ?_, ?_, ?_⟩
    · intro _ _; exact hs.writerHeld i hiw
    · intro a b ha hb'
      have : ∀ m, Writer (moved s.place i (.walking (k + 1)) m) → m = i := by
        intro m hm
        by_cases hmi : m = i
        · exact hmi
        · exact (only m hmi (by simpa [moved, hmi] using hm)).elim
      rw [this a ha, this b hb']
    · intro j k' hj
      by_cases hji : j = i
      · subst hji; dsimp only at hj; rw [moved_self] at hj; cases hj
        obtain ⟨htk, hw⟩ := hs.walked j k h
        refine ⟨Nat.le_succ_of_le htk, fun m hm hmk => ?_⟩
        rcases Nat.lt_succ_iff_lt_or_eq.mp hmk with hlt | heq
        · exact hw m hm hlt
        · rw [heq]; exact hf
      · exact (only j hji (by rw [kept hj hji]; trivial)).elim
    · intro j k' hj
      by_cases hji : j = i
      · subst hji; dsimp only at hj; rw [moved_self] at hj; cases hj
      · exact (only j hji (by rw [kept hj hji]; trivial)).elim
    · intro ht hf'
      exact Or.inr (Or.inr (Or.inr (Or.inl ⟨i, k + 1, moved_self _ _ _⟩)))
  | stop i k h hb =>
    have hiw : Writer (s.place i) := by rw [h]; trivial
    have only : ∀ j, j ≠ i → ¬ Writer (s.place j) := fun j hji hj => hji (hs.oneWriter j i hj hiw)
    refine ⟨?_, ?_, ?_, ?_, ?_⟩
    · intro _ _; exact hs.writerHeld i hiw
    · intro a b ha hb'
      have : ∀ m, Writer (moved s.place i (.stopped k) m) → m = i := by
        intro m hm
        by_cases hmi : m = i
        · exact hmi
        · exact (only m hmi (by simpa [moved, hmi] using hm)).elim
      rw [this a ha, this b hb']
    · intro j k' hj
      by_cases hji : j = i
      · subst hji; dsimp only at hj; rw [moved_self] at hj; cases hj
      · exact (only j hji (by rw [kept hj hji]; trivial)).elim
    · intro j k' hj hk hf
      by_cases hji : j = i
      · subst hji; dsimp only at hj; rw [moved_self] at hj; cases hj
        rcases hb with hb | hb
        · rw [hb] at hf; cases hf
        · exact absurd hk (Nat.not_lt.mpr hb)
      · exact (only j hji (by rw [kept hj hji]; trivial)).elim
    · intro ht hf
      exact Or.inr (Or.inr (Or.inr (Or.inr ⟨i, k, moved_self _ _ _⟩)))
  | publish i k h =>
    have hiw : Writer (s.place i) := by rw [h]; trivial
    have only : ∀ j, j ≠ i → ¬ Writer (s.place j) := fun j hji hj => hji (hs.oneWriter j i hj hiw)
    refine ⟨?_, ?_, ?_, ?_, ?_⟩
    · intro _ _; exact hs.writerHeld i hiw
    · intro a b ha hb'
      have : ∀ m, Writer (moved s.place i .published m) → m = i := by
        intro m hm
        by_cases hmi : m = i
        · exact hmi
        · exact (only m hmi (by simpa [moved, hmi] using hm)).elim
      rw [this a ha, this b hb']
    · intro j k' hj
      by_cases hji : j = i
      · subst hji; dsimp only at hj; rw [moved_self] at hj; cases hj
      · exact (only j hji (by rw [kept hj hji]; trivial)).elim
    · intro j k' hj
      by_cases hji : j = i
      · subst hji; dsimp only at hj; rw [moved_self] at hj; cases hj
      · exact (only j hji (by rw [kept hj hji]; trivial)).elim
    · intro ht hf
      rcases hs.stoppedCovered i k h ht hf with ho | ⟨m, hm⟩ | ⟨m, hm⟩
      · exact Or.inl ho
      · have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
        exact Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩)
      · have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
        exact Or.inr (Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩))
  | release i h =>
    have hiw : Writer (s.place i) := by rw [h]; trivial
    have only : ∀ j, j ≠ i → ¬ Writer (s.place j) := fun j hji hj => hji (hs.oneWriter j i hj hiw)
    have hnw : ¬ Writer Place.idle := by simp [Writer]
    refine ⟨?_, ?_, ?_, ?_, ?_⟩
    · intro j hj
      obtain ⟨hji, hjw⟩ := writer_before hnw hj
      exact (only j hji hjw).elim
    · intro a b ha _
      obtain ⟨hai, haw⟩ := writer_before hnw ha
      exact (only a hai haw).elim
    · intro j k hj
      have hji : j ≠ i := by intro e; subst e; simp [moved] at hj
      exact (only j hji (by rw [kept hj hji]; trivial)).elim
    · intro j k hj
      have hji : j ≠ i := by intro e; subst e; simp [moved] at hj
      exact (only j hji (by rw [kept hj hji]; trivial)).elim
    · intro ht hf
      rcases hs.topCovered ht hf with hso | ⟨m, hm⟩ | ⟨m, hm⟩ | ⟨m, k, hm⟩ | ⟨m, k, hm⟩
      · exact Or.inl hso
      · have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
        exact Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩)
      · have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
        exact Or.inr (Or.inr (Or.inl ⟨m, by simp [moved, hmi, hm]⟩))
      · have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
        exact (only m hmi (by rw [hm]; trivial)).elim
      · have hmi : m ≠ i := by intro e; subst e; rw [h] at hm; cases hm
        exact (only m hmi (by rw [hm]; trivial)).elim
  | bump i h hc ht =>
    have none : ∀ j, ¬ Writer (s.place j) := fun j hj => by
      have := hs.writerHeld j hj; rw [hc] at this; cases this
    refine ⟨hs.writerHeld, hs.oneWriter, ?_, ?_, ?_⟩
    · intro j k hj; exact (none j (by rw [hj]; trivial)).elim
    · intro j k hj; exact (none j (by rw [hj]; trivial)).elim
    · intro _ hf; simp [marked] at hf

theorem inv_reach {size : Nat} {s : State} (h : Reach size s) : Inv size s := by
  induction h with
  | start => exact inv_start size
  | step _ st ih => exact inv_step ih st

/-- A cellblock has no free cell at its top when its word holds no trim bit and each context is at rest. A trim
walks past each free cell at the boundary, or the word keeps the trim bit. -/
theorem open_trimmed {size : Nat} {s : State} (h : Reach size s) (hno : s.owed = false)
    (hrest : ∀ i, s.place i = .idle) (ht : s.top < size) : s.free s.top = false := by
  have hi := inv_reach h
  cases hf : s.free s.top with
  | false => rfl
  | true =>
    rcases hi.topCovered ht hf with ho | ⟨m, hm⟩ | ⟨m, hm⟩ | ⟨m, k, hm⟩ | ⟨m, k, hm⟩
    · rw [hno] at ho; cases ho
    all_goals rw [hrest m] at hm; cases hm

/-- The next holder runs the trim that the open word keeps. Its take starts the walk at `top`, and the walk goes
past the free cell there. -/
theorem owed_walked {size : Nat} {s : State} (i : Nat) (h : s.place i = .holding) (ho : s.owed = true) :
    Step size s { s with owed := false, place := moved s.place i (.walking s.top) } := by
  have st := Step.take (size := size) i h
  rw [ho] at st
  exact st

/-- Two live cells, both busy: cell 0 at the top and cell 1 above it, the cellblock open. -/
def twoCells : State := ⟨false, false, 0, fun _ => false, fun _ => .idle⟩

/-- The earlier protocol's interleaving from `twoCells`, as the states it passes through. The give of cell 1 reads
`top` first and finds 0 there, not its cell. The give of cell 0 stores its header free and holds with its fetch-or;
its open takes the trim bit, walks past cell 0, stops at cell 1 still busy, stores 1 as the top and opens. The give
of cell 1 then stores its header free and leaves on the `top` it read. -/
def oldAfter : State :=
  let s1 : State := { twoCells with free := marked twoCells.free 0 true }
  let s2 : State := { s1 with held := true, owed := true }
  let s3 : State := { s2 with owed := false, top := 1 }
  let s4 : State := { s3 with held := false }
  { s4 with free := marked s4.free 1 true }

/-- The read the earlier give of cell 1 made: `top` was not its cell. -/
theorem old_read : twoCells.top ≠ 1 := by decide

/-- The earlier protocol leaves the cellblock open, nothing owed and the free cell 1 at its top. -/
theorem old_loses : oldAfter.held = false ∧ oldAfter.owed = false ∧ oldAfter.top = 1 ∧ oldAfter.free 1 = true := by
  decide

/-- In this protocol the give of cell 1 reads `top` after its store, finds its own cell there, and goes on to its
fetch-or: it holds, and its trim walks past cell 1. -/
theorem new_reads : oldAfter.top = 1 := by decide

end LocusCarcerum.Boundary
