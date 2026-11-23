import LocusCarcerum.Guard

/-!
# LocusCarcerum, the order of a call around its CAS

`Vector.lean` commits a call in one step. The C does the call in four steps, and threads interleave them. A thread
reads the claim word. It makes the claim, which is one CAS (`Guard.claim`). It does its work on the cell while it
holds the claim. Then it gives the claim back with one more CAS (`Guard.release`).

This file runs any count of threads on any schedule, one step at a time. It keeps a second memory beside the cells,
the model. The model changes only where a claim commits, by the effect of the call of that claim. That is the memory
of the one step commit. The theorems hold for each count of threads, with no bound on the count.

* A claim that does not commit changes no word, no cell and no model.

* Two threads that hold claims at the same time hold two different elements.

* While a thread holds an element, no other thread changes it.

* A reader gives back the value that the model held when its claim committed.

* With no claim held, the cells are the model. So the run in four steps leaves the memory of the one step commit, with
  each call at the point of its claim.
-/

namespace LocusCarcerum.Linearizable

open LocusCarcerum.Guard

variable {α : Type} {W : Nat}

/-- A function with the value at one point changed. -/
def update {β : Type} (f : Nat → β) (k : Nat) (v : β) : Nat → β := fun j => if j = k then v else f j

@[simp] theorem update_same {β : Type} (f : Nat → β) (k : Nat) (v : β) : update f k v k = v := by simp [update]

@[simp] theorem update_other {β : Type} (f : Nat → β) (k j : Nat) (v : β) (h : j ≠ k) : update f k v j = f j := by
  simp [update, h]

/-- The call of a thread: a write of an element, or a read of one. -/
inductive Kind (α : Type) where
  | put (x : α)
  | get

/-- Whether a call writes. -/
def Kind.isPut : Kind α → Bool
  | .put _ => true
  | .get => false

/-- The step that a thread is at. A thread goes from `idle` to `seen` at its read of the word, and to `held` at its
claim. It goes to `worked` at its work, and to `done` at its release. A read gives its value. -/
inductive Phase (α : Type) (W : Nat) where
  | idle
  | seen (w : BitVec W)
  | held
  | worked (r : Option α)
  | done (r : Option α)

/-- The thread holds its claim and has not done its work. -/
def Phase.claimed : Phase α W → Bool
  | .held => true
  | _ => false

/-- The thread holds its claim. -/
def Phase.holds : Phase α W → Bool
  | .held => true
  | .worked _ => true
  | _ => false

/-- The value that the thread gives, after its work. -/
def Phase.result : Phase α W → Option (Option α)
  | .worked r => some r
  | .done r => some r
  | _ => none

/-- A thread: the element that it claims, its call and its step. -/
structure Thread (α : Type) (W : Nat) where
  elem : Nat
  kind : Kind α
  phase : Phase α W

/-- The state of a run: the claim words, the cells, the threads and the model. It also holds, for each reader, the
value of the model when its claim committed, and the threads in the order of their claims. -/
structure State (α : Type) (W : Nat) where
  words : Nat → BitVec W
  cells : Nat → α
  threads : Nat → Thread α W
  model : Nat → α
  lin : Nat → Option α
  log : List Nat

/-- The bit of element `n` in the claim words. -/
def bitSet (words : Nat → BitVec W) (n : Nat) : Bool := (words (n / W)).getLsbD (n % W)

/-- One step of thread `t`. -/
def step (s : State α W) (t : Nat) : State α W :=
  match (s.threads t).phase with
  | .idle => { s with threads := update s.threads t { s.threads t with phase := .seen (s.words ((s.threads t).elem / W)) } }
  | .seen w =>
    if (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2 then
      { s with
        words := update s.words ((s.threads t).elem / W)
          (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).1
        threads := update s.threads t { s.threads t with phase := .held }
        model :=
          match (s.threads t).kind with
          | .put x => update s.model (s.threads t).elem x
          | .get => s.model
        lin :=
          match (s.threads t).kind with
          | .put _ => s.lin
          | .get => update s.lin t (some (s.model (s.threads t).elem))
        log := s.log ++ [t] }
    else { s with threads := update s.threads t { s.threads t with phase := .idle } }
  | .held =>
    match (s.threads t).kind with
    | .put x =>
      { s with
        cells := update s.cells (s.threads t).elem x
        threads := update s.threads t { s.threads t with phase := .worked none } }
    | .get =>
      { s with threads := update s.threads t { s.threads t with phase := .worked (some (s.cells (s.threads t).elem)) } }
  | .worked r =>
    { s with
      words := update s.words ((s.threads t).elem / W) (release (s.words ((s.threads t).elem / W)) (bit W (s.threads t).elem))
      threads := update s.threads t { s.threads t with phase := .done r } }
  | .done _ => s

/-! ## The bits of the claim words -/

theorem bit_get (hW : 0 < W) (n m : Nat) (hj : m / W = n / W) : (bit W n).getLsbD (m % W) = decide (m = n) := by
  unfold bit
  rw [BitVec.getLsbD_twoPow]
  have hlt := Nat.mod_lt n hW
  have hiff : (n % W = m % W) ↔ m = n := by
    constructor
    · intro h
      rw [← Nat.div_add_mod m W, ← Nat.div_add_mod n W, hj, h]
    · intro h
      rw [h]
  simp [hlt, hiff]

/-- A claim sets the bit of its element and no other. -/
theorem bitSet_claim (hW : 0 < W) (words : Nat → BitVec W) (n m : Nat) :
    bitSet (update words (n / W) (words (n / W) ||| bit W n)) m = (bitSet words m || decide (m = n)) := by
  unfold bitSet
  by_cases hj : m / W = n / W
  · simp only [update, hj, ↓reduceIte]
    rw [BitVec.getLsbD_or, bit_get hW n m hj]
  · have hne : m ≠ n := fun h => hj (by rw [h])
    simp [update, hj, hne]

/-- A release clears the bit of its element and no other. -/
theorem bitSet_release (hW : 0 < W) (words : Nat → BitVec W) (n m : Nat) :
    bitSet (update words (n / W) (release (words (n / W)) (bit W n))) m = (bitSet words m && !decide (m = n)) := by
  unfold bitSet
  by_cases hj : m / W = n / W
  · simp only [update, hj, ↓reduceIte, release]
    rw [BitVec.getLsbD_and, BitVec.getLsbD_not, bit_get hW n m hj]
    simp [Nat.mod_lt m hW]
  · have hne : m ≠ n := fun h => hj (by rw [h])
    simp [update, hj, hne]

/-- A claim commits only where the bit of its element is clear. -/
theorem claimed_clear (hW : 0 < W) (words : Nat → BitVec W) (n : Nat) (w : BitVec W)
    (h : (claim (words (n / W)) w (bit W n)).2 = true) : bitSet words n = false := by
  obtain ⟨_, hz⟩ := (claim_commits _ _ _).1 h
  have := congrArg (fun x => x.getLsbD (n % W)) hz
  simp only [BitVec.getLsbD_and, bit_get hW n n rfl, decide_true, Bool.and_true] at this
  unfold bitSet
  rw [this]
  simp

/-! ## The invariant -/

/-- The invariant of a run.

* The claim words have the bit of an element where a thread holds the element, and only there.
* No two threads hold one element.
* An element that no claimed write holds has its cell equal to the model.
* A claimed write put its value in the model.
* A reader that holds its element has the model value of the element.
* A reader gives the value of the model at its claim. -/
structure Inv (s : State α W) : Prop where
  bits : ∀ m, bitSet s.words m = true ↔ ∃ u, (s.threads u).phase.holds = true ∧ (s.threads u).elem = m
  apart : ∀ u v, (s.threads u).phase.holds = true → (s.threads v).phase.holds = true →
    (s.threads u).elem = (s.threads v).elem → u = v
  cells : ∀ m, (∀ u, (s.threads u).elem = m → (s.threads u).phase.claimed = true → (s.threads u).kind.isPut = false) →
    s.cells m = s.model m
  pending : ∀ u x, (s.threads u).phase.claimed = true → (s.threads u).kind = .put x → s.model (s.threads u).elem = x
  reader : ∀ u, (s.threads u).kind = .get → (s.threads u).phase.holds = true →
    s.lin u = some (s.model (s.threads u).elem)
  result : ∀ u r, (s.threads u).kind = .get → (s.threads u).phase.result = some r → r = s.lin u

/-- A step that changes one thread between two steps before its claim, and changes nothing else, keeps the
invariant. -/
theorem inv_before (s : State α W) (hs : Inv s) (t : Nat) (th : Thread α W) (he : th.elem = (s.threads t).elem)
    (hk : th.kind = (s.threads t).kind) (h1 : (s.threads t).phase.holds = false)
    (h2 : th.phase.holds = false) (h3 : (s.threads t).phase.result = none) (h4 : th.phase.result = none) :
    Inv { s with threads := update s.threads t th } := by
  have hc1 : (s.threads t).phase.claimed = false := by
    cases hp : (s.threads t).phase <;> simp_all [Phase.holds, Phase.claimed]
  have hc2 : th.phase.claimed = false := by
    cases hp : th.phase <;> simp_all [Phase.holds, Phase.claimed]
  have hold : ∀ u, (update s.threads t th u).phase.holds = (s.threads u).phase.holds := by
    intro u; by_cases hu : u = t
    · subst hu; simp [h1, h2]
    · simp [hu]
  have helem : ∀ u, (update s.threads t th u).elem = (s.threads u).elem := by
    intro u; by_cases hu : u = t
    · subst hu; simp [he]
    · simp [hu]
  have hclaim : ∀ u, (update s.threads t th u).phase.claimed = (s.threads u).phase.claimed := by
    intro u; by_cases hu : u = t
    · subst hu; simp [hc1, hc2]
    · simp [hu]
  have hkind : ∀ u, (update s.threads t th u).kind = (s.threads u).kind := by
    intro u; by_cases hu : u = t
    · subst hu; simp [hk]
    · simp [hu]
  have hres : ∀ u, (update s.threads t th u).phase.result = (s.threads u).phase.result := by
    intro u; by_cases hu : u = t
    · subst hu; simp [h3, h4]
    · simp [hu]
  refine ⟨?_, ?_, ?_, ?_, ?_, ?_⟩
  · intro m; simp only [hold, helem]; exact hs.bits m
  · intro u v hu hv huv; simp only [hold, helem] at hu hv huv; exact hs.apart u v hu hv huv
  · intro m hm; exact hs.cells m fun u hu hc => by rw [← hkind]; exact hm u (by rw [helem]; exact hu) (by rw [hclaim]; exact hc)
  · intro u x hc hx; simp only [hclaim, hkind, helem] at hc hx ⊢; exact hs.pending u x hc hx
  · intro u hu hh; simp only [hold, hkind, helem] at hu hh ⊢; exact hs.reader u hu hh
  · intro u r hu hr; simp only [hkind, hres] at hu hr; exact hs.result u r hu hr

/-- A claim that commits keeps the invariant. -/
theorem inv_claim (hW : 0 < W) (s : State α W) (hs : Inv s) (t : Nat) (w : BitVec W)
    (hph : (s.threads t).phase = .seen w)
    (hc : (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2 = true) :
    Inv (step s t) := by
  have hst : step s t = { s with
      words := update s.words ((s.threads t).elem / W)
        (s.words ((s.threads t).elem / W) ||| bit W (s.threads t).elem)
      threads := update s.threads t { s.threads t with phase := .held }
      model :=
        match (s.threads t).kind with
        | .put x => update s.model (s.threads t).elem x
        | .get => s.model
      lin :=
        match (s.threads t).kind with
        | .put _ => s.lin
        | .get => update s.lin t (some (s.model (s.threads t).elem))
      log := s.log ++ [t] } := by
    simp only [step, hph, hc, ↓reduceIte, claim_sets _ _ _ hc]
  rw [hst]
  have hclear := claimed_clear hW s.words _ w hc
  have hfree : ∀ u, (s.threads u).phase.holds = true → (s.threads u).elem ≠ (s.threads t).elem := by
    intro u hu he
    have := (hs.bits (s.threads t).elem).2 ⟨u, hu, he⟩
    rw [hclear] at this
    cases this
  have hnot : (s.threads t).phase.holds = false := by rw [hph]; rfl
  have hne : ∀ u, (s.threads u).phase.holds = true → u ≠ t := fun u hu he => by rw [he] at hu; rw [hu] at hnot; cases hnot
  have hmodel : ∀ m, m ≠ (s.threads t).elem →
      (match (s.threads t).kind with
        | .put x => update s.model (s.threads t).elem x
        | .get => s.model) m = s.model m := by
    intro m hm
    cases (s.threads t).kind <;> simp [hm]
  refine ⟨?_, ?_, ?_, ?_, ?_, ?_⟩
  · intro m
    rw [bitSet_claim hW, Bool.or_eq_true, hs.bits m, decide_eq_true_eq]
    constructor
    · rintro (⟨u, hu, he⟩ | rfl)
      · refine ⟨u, ?_, ?_⟩ <;> simp [hne u hu, hu, he]
      · exact ⟨t, by simp [Phase.holds], by simp⟩
    · rintro ⟨u, hu, he⟩
      by_cases hut : u = t
      · subst hut; right; simp at he; exact he.symm
      · left; simp [hut] at hu he; exact ⟨u, hu, he⟩
  · intro u v hu hv huv
    by_cases hut : u = t <;> by_cases hvt : v = t
    · rw [hut, hvt]
    · subst hut; simp [hvt] at hv huv; exact absurd huv.symm (hfree v hv)
    · subst hvt; simp [hut] at hu huv; exact absurd huv (hfree u hu)
    · simp [hut, hvt] at hu hv huv; exact hs.apart u v hu hv huv
  · intro m hm
    by_cases hmn : m = (s.threads t).elem
    · subst hmn
      cases hk : (s.threads t).kind with
      | put x =>
        have := hm t (by simp) (by simp [Phase.claimed])
        simp [hk, Kind.isPut] at this
      | get =>
        apply hs.cells
        intro u hu hcl
        by_cases hut : u = t
        · subst hut; rw [hph] at hcl; cases hcl
        · have := hm u (by simp [hut, hu]) (by simp [hut, hcl]); simpa [hut] using this
    · dsimp only
      rw [hmodel m hmn]
      apply hs.cells
      intro u hu hcl
      by_cases hut : u = t
      · subst hut; rw [hph] at hcl; cases hcl
      · have := hm u (by simp [hut, hu]) (by simp [hut, hcl]); simpa [hut] using this
  · intro u x hcl hx
    by_cases hut : u = t
    · subst hut
      simp at hx
      simp [hx]
    · simp [hut] at hcl hx ⊢
      have hh : (s.threads u).phase.holds = true := by
        cases hp : (s.threads u).phase <;> simp_all [Phase.claimed, Phase.holds]
      rw [hmodel _ (hfree u hh)]
      exact hs.pending u x hcl hx
  · intro u hu hh
    by_cases hut : u = t
    · subst hut
      simp at hu
      simp [hu]
    · simp [hut] at hu hh ⊢
      have hl : (match (s.threads t).kind with
          | .put _ => s.lin
          | .get => update s.lin t (some (s.model (s.threads t).elem))) u = s.lin u := by
        cases (s.threads t).kind <;> simp [hut]
      rw [hl, hmodel _ (hfree u hh)]
      exact hs.reader u hu hh
  · intro u r hu hr
    by_cases hut : u = t
    · subst hut; simp [Phase.result] at hr
    · simp [hut] at hu hr ⊢
      have hl : (match (s.threads t).kind with
          | .put _ => s.lin
          | .get => update s.lin t (some (s.model (s.threads t).elem))) u = s.lin u := by
        cases (s.threads t).kind <;> simp [hut]
      rw [hl]
      exact hs.result u r hu hr

/-- The work of a claimed thread keeps the invariant. -/
theorem inv_work (s : State α W) (hs : Inv s) (t : Nat) (hph : (s.threads t).phase = .held) : Inv (step s t) := by
  have hheld : (s.threads t).phase.holds = true := by rw [hph]; rfl
  have honly : ∀ u, (s.threads u).phase.holds = true → (s.threads u).elem = (s.threads t).elem → u = t :=
    fun u hu he => hs.apart u t hu hheld he
  cases hk : (s.threads t).kind with
  | put x =>
    have hst : step s t = { s with
        cells := update s.cells (s.threads t).elem x
        threads := update s.threads t { s.threads t with phase := .worked none } } := by
      simp only [step, hph, hk]
    rw [hst]
    have hx := hs.pending t x (by rw [hph]; rfl) hk
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro m
      rw [hs.bits m]
      constructor
      · rintro ⟨u, hu, he⟩
        by_cases hut : u = t
        · subst hut; exact ⟨u, by simp [Phase.holds], by simp [he]⟩
        · exact ⟨u, by simp [hut, hu], by simp [hut, he]⟩
      · rintro ⟨u, hu, he⟩
        by_cases hut : u = t
        · subst hut; exact ⟨u, hheld, by simpa using he⟩
        · simp [hut] at hu he; exact ⟨u, hu, he⟩
    · intro u v hu hv huv
      have hu' : (s.threads u).phase.holds = true := by
        by_cases hut : u = t
        · rw [hut]; exact hheld
        · simpa [hut] using hu
      have hv' : (s.threads v).phase.holds = true := by
        by_cases hvt : v = t
        · rw [hvt]; exact hheld
        · simpa [hvt] using hv
      have he : (s.threads u).elem = (s.threads v).elem := by
        by_cases hut : u = t <;> by_cases hvt : v = t <;> simp_all
      exact hs.apart u v hu' hv' he
    · intro m hm
      by_cases hmn : m = (s.threads t).elem
      · subst hmn; simp [hx]
      · simp only [update_other _ _ _ _ hmn]
        apply hs.cells
        intro u hu hcl
        by_cases hut : u = t
        · subst hut; exact absurd hu.symm hmn
        · have := hm u (by simp [hut, hu]) (by simp [hut, hcl]); simpa [hut] using this
    · intro u y hcl hy
      by_cases hut : u = t
      · subst hut; simp [Phase.claimed] at hcl
      · simp [hut] at hcl hy ⊢; exact hs.pending u y hcl hy
    · intro u hu hh
      by_cases hut : u = t
      · subst hut; simp [hk] at hu
      · simp [hut] at hu hh ⊢; exact hs.reader u hu hh
    · intro u r hu hr
      by_cases hut : u = t
      · subst hut; simp [hk] at hu
      · simp [hut] at hu hr ⊢; exact hs.result u r hu hr
  | get =>
    have hst : step s t = { s with
        threads := update s.threads t { s.threads t with phase := .worked (some (s.cells (s.threads t).elem)) } } := by
      simp only [step, hph, hk]
    rw [hst]
    have hcell : s.cells (s.threads t).elem = s.model (s.threads t).elem := by
      apply hs.cells
      intro u hu hcl
      have hh : (s.threads u).phase.holds = true := by
        cases hp : (s.threads u).phase <;> simp_all [Phase.claimed, Phase.holds]
      rw [honly u hh hu, hk]
      rfl
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro m
      rw [hs.bits m]
      constructor
      · rintro ⟨u, hu, he⟩
        by_cases hut : u = t
        · subst hut; exact ⟨u, by simp [Phase.holds], by simp [he]⟩
        · exact ⟨u, by simp [hut, hu], by simp [hut, he]⟩
      · rintro ⟨u, hu, he⟩
        by_cases hut : u = t
        · subst hut; exact ⟨u, hheld, by simpa using he⟩
        · simp [hut] at hu he; exact ⟨u, hu, he⟩
    · intro u v hu hv huv
      have hu' : (s.threads u).phase.holds = true := by
        by_cases hut : u = t
        · rw [hut]; exact hheld
        · simpa [hut] using hu
      have hv' : (s.threads v).phase.holds = true := by
        by_cases hvt : v = t
        · rw [hvt]; exact hheld
        · simpa [hvt] using hv
      have he : (s.threads u).elem = (s.threads v).elem := by
        by_cases hut : u = t <;> by_cases hvt : v = t <;> simp_all
      exact hs.apart u v hu' hv' he
    · intro m hm
      apply hs.cells
      intro u hu hcl
      by_cases hut : u = t
      · subst hut; rw [hk]; rfl
      · have := hm u (by simp [hut, hu]) (by simp [hut, hcl]); simpa [hut] using this
    · intro u y hcl hy
      by_cases hut : u = t
      · subst hut; simp [Phase.claimed] at hcl
      · simp [hut] at hcl hy ⊢; exact hs.pending u y hcl hy
    · intro u hu hh
      by_cases hut : u = t
      · subst hut; simp at hu ⊢; exact hs.reader u hk hheld
      · simp [hut] at hu hh ⊢; exact hs.reader u hu hh
    · intro u r hu hr
      by_cases hut : u = t
      · subst hut
        simp [Phase.result] at hr
        rw [← hr, hs.reader u hk hheld, hcell]
      · simp [hut] at hu hr ⊢; exact hs.result u r hu hr

/-- A release keeps the invariant. -/
theorem inv_release (hW : 0 < W) (s : State α W) (hs : Inv s) (t : Nat) (r : Option α)
    (hph : (s.threads t).phase = .worked r) : Inv (step s t) := by
  have hheld : (s.threads t).phase.holds = true := by rw [hph]; rfl
  have honly : ∀ u, (s.threads u).phase.holds = true → (s.threads u).elem = (s.threads t).elem → u = t :=
    fun u hu he => hs.apart u t hu hheld he
  have hst : step s t = { s with
      words := update s.words ((s.threads t).elem / W)
        (release (s.words ((s.threads t).elem / W)) (bit W (s.threads t).elem))
      threads := update s.threads t { s.threads t with phase := .done r } } := by
    simp only [step, hph]
  rw [hst]
  refine ⟨?_, ?_, ?_, ?_, ?_, ?_⟩
  · intro m
    rw [bitSet_release hW, Bool.and_eq_true, hs.bits m, Bool.not_eq_true', decide_eq_false_iff_not]
    constructor
    · rintro ⟨⟨u, hu, he⟩, hm⟩
      have hut : u ≠ t := fun h => by subst h; exact hm he.symm
      exact ⟨u, by simp [hut, hu], by simp [hut, he]⟩
    · rintro ⟨u, hu, he⟩
      by_cases hut : u = t
      · subst hut; simp [Phase.holds] at hu
      · simp [hut] at hu he
        refine ⟨⟨u, hu, he⟩, fun hm => hut (honly u hu (by rw [he, hm]))⟩
  · intro u v hu hv huv
    by_cases hut : u = t
    · subst hut; simp [Phase.holds] at hu
    · by_cases hvt : v = t
      · subst hvt; simp [Phase.holds] at hv
      · simp [hut, hvt] at hu hv huv; exact hs.apart u v hu hv huv
  · intro m hm
    apply hs.cells
    intro u hu hcl
    by_cases hut : u = t
    · subst hut; rw [hph] at hcl; cases hcl
    · have := hm u (by simp [hut, hu]) (by simp [hut, hcl]); simpa [hut] using this
  · intro u y hcl hy
    by_cases hut : u = t
    · subst hut; simp [Phase.claimed] at hcl
    · simp [hut] at hcl hy ⊢; exact hs.pending u y hcl hy
  · intro u hu hh
    by_cases hut : u = t
    · subst hut; simp [Phase.holds] at hh
    · simp [hut] at hu hh ⊢; exact hs.reader u hu hh
  · intro u y hu hy
    by_cases hut : u = t
    · subst hut
      simp [Phase.result] at hy
      simp at hu
      exact hs.result u y hu (by rw [hph, hy]; rfl)
    · simp [hut] at hu hy ⊢; exact hs.result u y hu hy

/-- Each step keeps the invariant. -/
theorem step_inv (hW : 0 < W) (s : State α W) (hs : Inv s) (t : Nat) : Inv (step s t) := by
  cases hph : (s.threads t).phase with
  | idle =>
    have hst : step s t =
        { s with threads := update s.threads t ({ s.threads t with phase := .seen (s.words ((s.threads t).elem / W)) }) } := by
      simp only [step, hph]
    rw [hst]
    exact inv_before s hs t _ rfl rfl (by rw [hph]; rfl) rfl (by rw [hph]; rfl) rfl
  | seen w =>
    by_cases hc : (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2 = true
    · exact inv_claim hW s hs t w hph hc
    · have hst : step s t = { s with threads := update s.threads t { s.threads t with phase := .idle } } := by
        simp only [step, hph, hc, Bool.false_eq_true, ↓reduceIte]
      rw [hst]
      exact inv_before s hs t _ rfl rfl (by rw [hph]; rfl) rfl (by rw [hph]; rfl) rfl
  | held => exact inv_work s hs t hph
  | worked r => exact inv_release hW s hs t r hph
  | done r =>
    have hst : step s t = s := by simp only [step, hph]
    rw [hst]
    exact hs

/-! ## The run -/

/-- The state at the start: no claim word set, each thread before its read, and the model equal to the cells. -/
def start (cells : Nat → α) (threads : Nat → Thread α W) : State α W :=
  ⟨fun _ => 0, cells, threads, cells, fun _ => none, []⟩

/-- The start keeps the invariant where each thread is before its read. -/
theorem start_inv (cells : Nat → α) (threads : Nat → Thread α W) (h : ∀ t, (threads t).phase = .idle) :
    Inv (start cells threads) := by
  refine ⟨?_, ?_, ?_, ?_, ?_, ?_⟩
  · intro m
    have h0 : (0 : BitVec W).getLsbD (m % W) = false := BitVec.getLsbD_zero
    simp only [start, bitSet]
    rw [h0]
    simp only [Bool.false_eq_true, false_iff, not_exists, not_and]
    intro u hu
    rw [h u] at hu
    cases hu
  · intro u v hu
    simp only [start, h u] at hu
    cases hu
  · intro m _; rfl
  · intro u x hcl
    simp only [start, h u] at hcl
    cases hcl
  · intro u _ hh
    simp only [start, h u] at hh
    cases hh
  · intro u r _ hr
    simp only [start, h u] at hr
    cases hr

/-- A schedule run from a state: the steps of the threads that it names, in its order. -/
def run (s : State α W) (sched : List Nat) : State α W := sched.foldl step s

/-- Each schedule keeps the invariant, for each count of threads. -/
theorem run_inv (hW : 0 < W) (s : State α W) (hs : Inv s) (sched : List Nat) : Inv (run s sched) := by
  induction sched generalizing s with
  | nil => exact hs
  | cons t rest ih => exact ih (step s t) (step_inv hW s hs t)

/-! ## The step of one thread -/

/-- The thread is before its claim. -/
def Phase.before : Phase α W → Bool
  | .idle => true
  | .seen _ => true
  | _ => false

/-- The thread gave its claim back. -/
def Phase.isDone : Phase α W → Bool
  | .done _ => true
  | _ => false

theorem done_not_before (p : Phase α W) (h : p.isDone = true) : p.before = false := by
  cases p <;> simp_all [Phase.isDone, Phase.before]

theorem done_not_holds (p : Phase α W) (h : p.isDone = true) : p.holds = false := by
  cases p <;> simp_all [Phase.isDone, Phase.holds]

/-- The step of thread `t` is a claim that commits. -/
def commits (s : State α W) (t : Nat) : Bool :=
  match (s.threads t).phase with
  | .seen w => (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2
  | _ => false

/-- The effect of the call of a thread on the model. -/
def effect (th : Thread α W) (m : Nat → α) : Nat → α :=
  match th.kind with
  | .put x => update m th.elem x
  | .get => m

/-- A step changes no other thread. -/
theorem step_other (s : State α W) (t u : Nat) (hu : u ≠ t) : (step s t).threads u = s.threads u := by
  cases hph : (s.threads t).phase with
  | idle => simp [step, hph, hu]
  | seen w =>
    by_cases hc : (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2 = true
    · simp [step, hph, hc, hu]
    · simp [step, hph, hc, hu]
  | held => cases hk : (s.threads t).kind <;> simp [step, hph, hk, hu]
  | worked r => simp [step, hph, hu]
  | done r => simp [step, hph]

/-- A step keeps the element and the call of its thread. -/
theorem step_call (s : State α W) (t : Nat) :
    ((step s t).threads t).elem = (s.threads t).elem ∧ ((step s t).threads t).kind = (s.threads t).kind := by
  cases hph : (s.threads t).phase with
  | idle => simp [step, hph]
  | seen w =>
    by_cases hc : (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2 = true
    · simp [step, hph, hc]
    · simp [step, hph, hc]
  | held => cases hk : (s.threads t).kind <;> simp [step, hph, hk]
  | worked r => simp [step, hph]
  | done r => simp [step, hph]

/-- A step changes the model by the effect of its call where its claim commits, and not in another step. -/
theorem step_model (s : State α W) (t : Nat) :
    (step s t).model = if commits s t then effect (s.threads t) s.model else s.model := by
  cases hph : (s.threads t).phase with
  | idle => simp [step, commits, hph]
  | seen w =>
    by_cases hc : (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2 = true
    · cases hk : (s.threads t).kind <;> simp [step, commits, effect, hph, hc, hk]
    · simp [step, commits, hph, hc]
  | held => cases hk : (s.threads t).kind <;> simp [step, commits, hph, hk]
  | worked r => simp [step, commits, hph]
  | done r => simp [step, commits, hph]

/-- A claim that commits takes its thread from before its claim to after it. Another step keeps the thread on its
side of the claim. -/
theorem step_phase (s : State α W) (t : Nat) :
    (commits s t = true → (s.threads t).phase.before = true ∧ ((step s t).threads t).phase.before = false) ∧
      (commits s t = false → ((step s t).threads t).phase.before = (s.threads t).phase.before) := by
  cases hph : (s.threads t).phase with
  | idle => simp [step, commits, hph, Phase.before]
  | seen w =>
    by_cases hc : (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2 = true
    · simp [step, commits, hph, hc, Phase.before]
    · simp [step, commits, hph, hc, Phase.before]
  | held => cases hk : (s.threads t).kind <;> simp [step, commits, hph, hk, Phase.before]
  | worked r => simp [step, commits, hph, Phase.before]
  | done r => simp [step, commits, hph, Phase.before]

/-! ## What the run gives -/

/-- A claim that does not commit changes no word, no cell and no model. -/
theorem refused_keeps (s : State α W) (t : Nat) (w : BitVec W) (hph : (s.threads t).phase = .seen w)
    (hc : (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2 = false) :
    (step s t).words = s.words ∧ (step s t).cells = s.cells ∧ (step s t).model = s.model := by
  simp [step, hph, hc]

/-- Only a claim that commits changes the model. -/
theorem model_at_claim (s : State α W) (t : Nat) (h : ∀ w, (s.threads t).phase ≠ .seen w) :
    (step s t).model = s.model := by
  cases hph : (s.threads t).phase with
  | seen w => exact absurd hph (h w)
  | idle => simp [step, hph]
  | held => cases hk : (s.threads t).kind <;> simp [step, hph, hk]
  | worked r => simp [step, hph]
  | done r => simp [step, hph]

/-- Two threads that hold claims at the same time hold two different elements. -/
theorem exclusive (hW : 0 < W) (s : State α W) (hs : Inv s) (sched : List Nat) (u v : Nat) (huv : u ≠ v)
    (hu : ((run s sched).threads u).phase.holds = true) (hv : ((run s sched).threads v).phase.holds = true) :
    ((run s sched).threads u).elem ≠ ((run s sched).threads v).elem :=
  fun he => huv ((run_inv hW s hs sched).apart u v hu hv he)

/-- While a thread holds its element, a step of another thread does not change the cell of the element. -/
theorem held_alone (s : State α W) (hs : Inv s) (t u : Nat) (hut : u ≠ t)
    (ht : (s.threads t).phase.holds = true) : (step s u).cells (s.threads t).elem = s.cells (s.threads t).elem := by
  cases hph : (s.threads u).phase with
  | idle => simp [step, hph]
  | seen w =>
    by_cases hc : (claim (s.words ((s.threads u).elem / W)) w (bit W (s.threads u).elem)).2 = true
    · simp only [step, hph, hc, ↓reduceIte]
    · simp only [step, hph, hc, Bool.false_eq_true, ↓reduceIte]
  | held =>
    have hh : (s.threads u).phase.holds = true := by rw [hph]; rfl
    have hne : (s.threads t).elem ≠ (s.threads u).elem := fun he => hut (hs.apart u t hh ht he.symm)
    cases hk : (s.threads u).kind <;> simp [step, hph, hk, hne]
  | worked r => simp [step, hph]
  | done r => simp [step, hph]

/-- A reader after its work gives the value that the model held when its claim committed. -/
theorem reader_linear (hW : 0 < W) (s : State α W) (hs : Inv s) (sched : List Nat) (u : Nat) (r : Option α)
    (hu : ((run s sched).threads u).kind = .get) (hr : ((run s sched).threads u).phase.result = some r) :
    r = (run s sched).lin u :=
  (run_inv hW s hs sched).result u r hu hr

/-- With no claim held, the cells are the model: the memory of the one step commit, with each call at its claim. -/
theorem quiescent (hW : 0 < W) (s : State α W) (hs : Inv s) (sched : List Nat)
    (h : ∀ u, ((run s sched).threads u).phase.holds = false) : (run s sched).cells = (run s sched).model := by
  funext m
  apply (run_inv hW s hs sched).cells m
  intro u _ hcl
  have := h u
  cases hp : ((run s sched).threads u).phase <;> simp_all [Phase.claimed, Phase.holds]

/-! ## A claim that no thread gives back -/

/-- A claim commits where its read is the word as it is now, and no thread holds its element. So a claim that a dead
thread holds blocks the element of that claim alone, and each claim on another element still commits. -/
theorem free_claim_commits (s : State α W) (hs : Inv s) (u : Nat) (w : BitVec W)
    (hph : (s.threads u).phase = .seen w) (hread : w = s.words ((s.threads u).elem / W))
    (hfree : ∀ v, (s.threads v).phase.holds = true → (s.threads v).elem ≠ (s.threads u).elem) :
    commits s u = true := by
  have hclear : bitSet s.words (s.threads u).elem = false := by
    cases hb : bitSet s.words (s.threads u).elem
    · rfl
    · obtain ⟨v, hv, he⟩ := (hs.bits _).1 hb
      exact absurd he (hfree v hv)
  simp only [commits, hph]
  rw [claim_commits]
  refine ⟨hread.symm, ?_⟩
  apply BitVec.eq_of_getLsbD_eq
  intro i hi
  simp only [bitSet] at hclear
  simp only [BitVec.getLsbD_and, bit, BitVec.getLsbD_twoPow]
  by_cases hik : (s.threads u).elem % W = i
  · subst hik
    simp [hclear]
  · simp [hik]

/-- The end of a call that holds its claim: its work and its release, which the supervisor runs for a dead thread. A
call that holds no claim stays as it is. -/
def finish (s : State α W) (t : Nat) : State α W :=
  match (s.threads t).phase with
  | .held => step (step s t) t
  | .worked _ => step s t
  | _ => s

/-- The finish keeps the invariant. -/
theorem finish_inv (hW : 0 < W) (s : State α W) (hs : Inv s) (t : Nat) : Inv (finish s t) := by
  unfold finish
  split
  · exact step_inv hW _ (step_inv hW s hs t) t
  · exact step_inv hW s hs t
  · exact hs

/-- After the finish, the call holds no claim. So the element of a dead thread is free again. -/
theorem finish_releases (s : State α W) (t : Nat) : ((finish s t).threads t).phase.holds = false := by
  cases hph : (s.threads t).phase with
  | held =>
    have e : finish s t = step (step s t) t := by simp [finish, hph]
    rw [e]
    obtain ⟨r, h1⟩ : ∃ r, ((step s t).threads t).phase = .worked r := by
      cases hk : (s.threads t).kind with
      | put x => exact ⟨none, by simp [step, hph, hk]⟩
      | get => exact ⟨some (s.cells (s.threads t).elem), by simp [step, hph, hk]⟩
    generalize step s t = s1 at h1 ⊢
    simp [step, h1, Phase.holds]
  | worked r =>
    have e : finish s t = step s t := by simp [finish, hph]
    rw [e]
    simp [step, hph, Phase.holds]
  | idle => simp [finish, hph, Phase.holds]
  | seen w => simp [finish, hph, Phase.holds]
  | done r => simp [finish, hph, Phase.holds]

/-- The finish changes no other thread. -/
theorem finish_other (s : State α W) (t u : Nat) (hu : u ≠ t) : (finish s t).threads u = s.threads u := by
  unfold finish
  split
  · rw [step_other _ _ _ hu, step_other _ _ _ hu]
  · rw [step_other _ _ _ hu]
  · rfl

/-- The finish keeps the element and the call of its thread. -/
theorem finish_call (s : State α W) (t : Nat) :
    ((finish s t).threads t).elem = (s.threads t).elem ∧ ((finish s t).threads t).kind = (s.threads t).kind := by
  unfold finish
  split
  · obtain ⟨e1, k1⟩ := step_call (step s t) t
    obtain ⟨e0, k0⟩ := step_call s t
    exact ⟨e1.trans e0, k1.trans k0⟩
  · exact step_call s t
  · exact ⟨rfl, rfl⟩

/-- The finish is zero, one or two steps of its thread. -/
theorem finish_steps (s : State α W) (t : Nat) :
    finish s t = s ∨ finish s t = step s t ∨ finish s t = step (step s t) t := by
  unfold finish
  split
  · exact Or.inr (Or.inr rfl)
  · exact Or.inr (Or.inl rfl)
  · exact Or.inl rfl

/-! ## The sequential order

The source is Herlihy and Wing, `Linearizability: A Correctness Condition for Concurrent Objects`,
`ACM Transactions on Programming Languages and Systems` 12(3), 463 to 492, 1990, `doi:10.1145/78969.78972`. Its Section 2.2,
`Definition of Linearizability`, has two conditions on a history H that some extension H' meets.

* L1: `complete(H')` is equivalent to some legal sequential history S.

* L2: `<H ⊆ <S`, where `e0 <H e1` holds if the response of `e0` precedes the invocation of `e1` in H.

Here S is the log, the order of the claims. An invocation is the step out of `idle`, and a response is the step to
`done`. A call with a claim and no response is in the log, as the extension H' adds its response. A call with no claim
and no response is not in the log, as `complete(H')` drops it.

* L1 is `linearizable`. The model is the calls of the log replayed one at a time. A reader gives the value of that
  replay at its place in the log.

* L2 is `realtime`. A call that ended before another call made its claim comes first in the log.

Section 4.2 of the same paper names the method of this file: an abstraction function where each operation takes
effect at one step of its run. That section shows a queue where no such step exists. Here such a step exists, the
claim CAS, and the model is the abstraction function. -/

/-- The memory of the calls of `log`, applied one at a time in that order. -/
def replay (calls : Nat → Thread α W) (init : Nat → α) (log : List Nat) : Nat → α :=
  log.foldl (fun m k => effect (calls k) m) init

/-- The calls of the log before thread `t`. -/
def before (log : List Nat) (t : Nat) : List Nat := log.takeWhile (fun k => k ≠ t)

theorem before_append_new (log : List Nat) (t : Nat) (h : t ∉ log) : before (log ++ [t]) t = log := by
  induction log with
  | nil => simp [before]
  | cons k rest ih =>
    have hk : k ≠ t := fun he => h (by simp [he])
    have hr : t ∉ rest := fun hm => h (by simp [hm])
    simp only [before, List.cons_append, List.takeWhile_cons, hk, ne_eq, not_false_eq_true, decide_true,
      ↓reduceIte] at ih ⊢
    rw [ih hr]

theorem before_append_old (log more : List Nat) (k : Nat) (h : k ∈ log) : before (log ++ more) k = before log k := by
  induction log with
  | nil => simp at h
  | cons j rest ih =>
    simp only [before, List.cons_append, List.takeWhile_cons] at ih ⊢
    by_cases hj : j = k
    · simp [hj]
    · have hr : k ∈ rest := by
        rcases List.mem_cons.1 h with he | he
        · exact absurd he.symm hj
        · exact he
      have := ih hr
      simp only [ne_eq, decide_not] at this
      simp [hj, this]

theorem effect_congr (th th' : Thread α W) (he : th.elem = th'.elem) (hk : th.kind = th'.kind) :
    effect th = effect th' := by
  funext m
  unfold effect
  rw [hk, he]

/-- A step appends its thread to the log where its claim commits, and leaves the log as it was in another step. -/
theorem step_log (s : State α W) (t : Nat) : (step s t).log = if commits s t then s.log ++ [t] else s.log := by
  cases hph : (s.threads t).phase with
  | idle => simp [step, commits, hph]
  | seen w =>
    by_cases hc : (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2 = true
    · simp [step, commits, hph, hc]
    · simp [step, commits, hph, hc]
  | held => cases hk : (s.threads t).kind <;> simp [step, commits, hph, hk]
  | worked r => simp [step, commits, hph]
  | done r => simp [step, commits, hph]

/-- A step sets the value of a reader where its claim commits, and changes no other value. -/
theorem step_lin (s : State α W) (t : Nat) :
    (step s t).lin =
      if commits s t then
        (match (s.threads t).kind with
          | .put _ => s.lin
          | .get => update s.lin t (some (s.model (s.threads t).elem)))
      else s.lin := by
  cases hph : (s.threads t).phase with
  | idle => simp [step, commits, hph]
  | seen w =>
    by_cases hc : (claim (s.words ((s.threads t).elem / W)) w (bit W (s.threads t).elem)).2 = true
    · cases hk : (s.threads t).kind <;> simp [step, commits, hph, hc, hk]
    · simp [step, commits, hph, hc]
  | held => cases hk : (s.threads t).kind <;> simp [step, commits, hph, hk]
  | worked r => simp [step, commits, hph]
  | done r => simp [step, commits, hph]

/-- The invariant of the order. Each thread does its call. The model is the calls of the log replayed. Each thread is
in the log once at max, and there exactly after its claim. A reader in the log has the replay of the calls before it
at its element. -/
structure LogInv (calls : Nat → Thread α W) (init : Nat → α) (s : State α W) : Prop where
  call : ∀ k, (s.threads k).elem = (calls k).elem ∧ (s.threads k).kind = (calls k).kind
  model : s.model = replay calls init s.log
  nodup : s.log.Nodup
  mem : ∀ k, k ∈ s.log ↔ (s.threads k).phase.before = false
  reads : ∀ k, (s.threads k).kind = .get → k ∈ s.log →
    s.lin k = some (replay calls init (before s.log k) (s.threads k).elem)

theorem start_log (cells : Nat → α) (threads : Nat → Thread α W) (h : ∀ t, (threads t).phase = .idle) :
    LogInv threads cells (start cells threads) := by
  refine ⟨fun _ => ⟨rfl, rfl⟩, rfl, List.nodup_nil, ?_, ?_⟩
  · intro k
    simp only [start, List.not_mem_nil, false_iff, h k, Phase.before]
    simp
  · intro k _ hk
    simp [start] at hk

/-- Each step keeps the invariant of the order. -/
theorem step_loginv (calls : Nat → Thread α W) (init : Nat → α) (s : State α W) (hl : LogInv calls init s) (t : Nat) :
    LogInv calls init (step s t) := by
  obtain ⟨hcommit, hkeep⟩ := step_phase s t
  have hcall : ∀ k, ((step s t).threads k).elem = (calls k).elem ∧ ((step s t).threads k).kind = (calls k).kind := by
    intro k
    by_cases hk : k = t
    · subst hk
      obtain ⟨he, hkd⟩ := step_call s k
      rw [he, hkd]
      exact hl.call k
    · rw [step_other _ _ _ hk]
      exact hl.call k
  have heff : effect (s.threads t) = effect (calls t) := effect_congr _ _ (hl.call t).1 (hl.call t).2
  cases hc : commits s t with
  | false =>
    have hph := hkeep hc
    refine ⟨hcall, ?_, ?_, ?_, ?_⟩
    · rw [step_model, step_log, hc]
      exact hl.model
    · rw [step_log, hc]
      exact hl.nodup
    · intro k
      rw [step_log, hc]
      by_cases hk : k = t
      · subst hk; rw [hph]; exact hl.mem k
      · rw [step_other _ _ _ hk]; exact hl.mem k
    · intro k hk hm
      rw [step_log, hc] at hm ⊢
      rw [step_lin, hc]
      simp only [Bool.false_eq_true, ↓reduceIte]
      by_cases hkt : k = t
      · subst hkt
        obtain ⟨he, hkd⟩ := step_call s k
        rw [hkd] at hk
        rw [he, hl.reads k hk hm]
      · rw [step_other _ _ _ hkt] at hk ⊢
        exact hl.reads k hk hm
  | true =>
    obtain ⟨hb, ha⟩ := hcommit hc
    have hnew : t ∉ s.log := fun hm => by rw [(hl.mem t).1 hm] at hb; cases hb
    refine ⟨hcall, ?_, ?_, ?_, ?_⟩
    · rw [step_model, step_log, hc]
      simp only [↓reduceIte, replay, List.foldl_append, List.foldl_cons, List.foldl_nil]
      rw [heff, hl.model]
      rfl
    · rw [step_log, hc]
      simp only [↓reduceIte]
      rw [List.nodup_append]
      refine ⟨hl.nodup, by simp, ?_⟩
      intro x hx y hy hxy
      simp at hy
      subst hy
      subst hxy
      exact hnew hx
    · intro k
      rw [step_log, hc]
      simp only [↓reduceIte, List.mem_append, List.mem_singleton]
      by_cases hk : k = t
      · subst hk; simp [ha]
      · rw [step_other _ _ _ hk]
        simp [hk, hl.mem k]
    · intro k hk hm
      rw [step_log, hc] at hm ⊢
      simp only [↓reduceIte, List.mem_append, List.mem_singleton] at hm ⊢
      rw [step_lin, hc]
      simp only [↓reduceIte]
      by_cases hkt : k = t
      · subst hkt
        obtain ⟨he, hkd⟩ := step_call s k
        rw [hkd] at hk
        rw [hk, he, before_append_new _ _ hnew, ← hl.model]
        simp
      · have hold : k ∈ s.log := by
          rcases hm with hm | hm
          · exact hm
          · exact absurd hm hkt
        rw [step_other _ _ _ hkt] at hk ⊢
        rw [before_append_old _ _ _ hold]
        have hl' : (match (s.threads t).kind with
            | .put _ => s.lin
            | .get => update s.lin t (some (s.model (s.threads t).elem))) k = s.lin k := by
          cases (s.threads t).kind <;> simp [hkt]
        rw [hl']
        exact hl.reads k hk hold

theorem run_loginv (calls : Nat → Thread α W) (init : Nat → α) (s : State α W) (hl : LogInv calls init s)
    (sched : List Nat) : LogInv calls init (run s sched) := by
  induction sched generalizing s with
  | nil => exact hl
  | cons t rest ih => exact ih _ (step_loginv calls init s hl t)

/-- A run only appends to the log. -/
theorem run_log_prefix (s : State α W) (sched : List Nat) : ∃ more, (run s sched).log = s.log ++ more := by
  induction sched generalizing s with
  | nil => exact ⟨[], by simp [run]⟩
  | cons t rest ih =>
    obtain ⟨more, hm⟩ := ih (step s t)
    refine ⟨(if commits s t then [t] else []) ++ more, ?_⟩
    show (run (step s t) rest).log = _
    rw [hm, step_log]
    cases commits s t <;> simp

/-- The run is linearizable. The model is the calls of the log replayed one at a time. Each thread is in the log once
at max, and there exactly after its claim. A reader after its work gives the replay of the calls before it. -/
theorem linearizable (cells : Nat → α) (threads : Nat → Thread α W) (h : ∀ t, (threads t).phase = .idle)
    (hW : 0 < W) (sched : List Nat) :
    (run (start cells threads) sched).model = replay threads cells (run (start cells threads) sched).log ∧
      (run (start cells threads) sched).log.Nodup ∧
      (∀ k, k ∈ (run (start cells threads) sched).log ↔
        ((run (start cells threads) sched).threads k).phase.before = false) ∧
      (∀ k r, (threads k).kind = .get → ((run (start cells threads) sched).threads k).phase.result = some r →
        r = some (replay threads cells (before (run (start cells threads) sched).log k) (threads k).elem)) := by
  have hl := run_loginv threads cells _ (start_log cells threads h) sched
  have hi := run_inv hW _ (start_inv cells threads h) sched
  refine ⟨hl.model, hl.nodup, hl.mem, ?_⟩
  intro k r hk hr
  have hk' : ((run (start cells threads) sched).threads k).kind = .get := by rw [(hl.call k).2]; exact hk
  have hm : k ∈ (run (start cells threads) sched).log := by
    rw [hl.mem k]
    cases hp : ((run (start cells threads) sched).threads k).phase <;> simp_all [Phase.result, Phase.before]
  rw [hi.result k r hk' hr, hl.reads k hk' hm, (hl.call k).1]

/-- The order keeps real time. A call that ended before another call made its claim comes first in the log. -/
theorem realtime (calls : Nat → Thread α W) (init : Nat → α) (s : State α W) (hl : LogInv calls init s)
    (sched : List Nat) (a b : Nat) (ha : (s.threads a).phase.isDone = true) (hb : (s.threads b).phase.before = true) :
    ∃ pre post, (run s sched).log = pre ++ post ∧ a ∈ pre ∧ b ∉ pre := by
  obtain ⟨more, hm⟩ := run_log_prefix s sched
  refine ⟨s.log, more, hm, (hl.mem a).2 (done_not_before _ ha), fun h => ?_⟩
  rw [(hl.mem b).1 h] at hb
  cases hb

end LocusCarcerum.Linearizable
