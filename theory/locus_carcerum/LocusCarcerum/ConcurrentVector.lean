import LocusCarcerum.Guard

/-!
# LocusCarcerum, the calls of a vector on many threads at once

`Vector.lean` commits each call of a vector in one step. This file runs the calls on any count of threads, one atomic
step at a time. The record behind the token holds the count of the vector, and one claim bit for each element. The
caller holds the token alone, so this is a choice of the internals and no concern of the caller.

* `at` and `set` claim the bit of their element. Then they read the count, which is the span test of `vec[n]`
  (`Guard.legal`): the call is legal where `n` is below the count. Then they read or write the cell and give the bit
  back.

* `push_back` reads the count `s`, and refuses at the ceiling. It claims the bit of element `s`, and reads the count
  again. Where the count is still `s`, it writes the cell, and one CAS takes the count from `s` to `s + 1`.

* `pop_back` reads the count `s`, and refuses at 0. It claims the bit of element `s - 1`, and one CAS takes the count
  from `s` to `s - 1`. Then it reads the cell and gives the bit back.

Each call takes effect at one step: the read of the count for `at` and `set`, and the CAS of the count for
`push_back` and `pop_back`. A refusal takes effect at the read of the count that refuses it. The log holds the calls
in that order, as `Linearizable.lean` does. The model of memory is sequentially consistent.

* `legal`: the log is a legal history of the vector with one lock, as Herlihy and Wing state L1.

* `answer_logged`: each call that ended gave the answer of its point in the log.

* `realtime`: a call that ended before another took effect comes first in the log, which is L2.

* `legal_is_span`: the test of `at` and `set` is the span test of `Guard.lean`.
-/

namespace LocusCarcerum.ConcurrentVector

/-- A function with the value at one point changed. -/
def update {β : Type} (f : Nat → β) (k : Nat) (v : β) : Nat → β := fun j => if j = k then v else f j

@[simp] theorem update_same {β : Type} (f : Nat → β) (k : Nat) (v : β) : update f k v k = v := by simp [update]

@[simp] theorem update_other {β : Type} (f : Nat → β) (k j : Nat) (v : β) (h : j ≠ k) : update f k v j = f j := by
  simp [update, h]

variable {α : Type}

/-- A call of the vector. -/
inductive Call (α : Type) where
  | get (n : Nat)
  | set (n : Nat) (x : α)
  | push (x : α)
  | pop

/-- What a call gives back: a value or none, or a commit or a refusal. -/
inductive Answer (α : Type) where
  | value (r : Option α)
  | ok (b : Bool)

/-- The step that a thread is at. `sized` holds the count that a push or a pop read. The other steps but `idle` and
`done` hold the bit of element `i`. They come before the test, after the check of a push, and after its write. They
also come after the point of the call, after its work, and before a give back that retries or refuses. -/
inductive Phase (α : Type) where
  | idle
  | sized (k : Nat)
  | held (i : Nat)
  | valid (i : Nat)
  | wrote (i : Nat)
  | live (i : Nat) (r : Option α)
  | worked (i : Nat) (r : Option α)
  | backoff (i : Nat)
  | refusing (i : Nat)
  | done (a : Answer α)

/-- The element whose bit the thread holds. -/
def Phase.holds : Phase α → Option Nat
  | .held i => some i
  | .valid i => some i
  | .wrote i => some i
  | .live i _ => some i
  | .worked i _ => some i
  | .backoff i => some i
  | .refusing i => some i
  | _ => none

structure Thread (α : Type) where
  call : Call α
  phase : Phase α

/-- A point of the log: the thread, its call, and the answer of the call. -/
inductive LinOp (α : Type) where
  | get (u n : Nat) (r : Option α)
  | set (u n : Nat) (x : α) (ok : Bool)
  | push (u : Nat) (x : α) (ok : Bool)
  | pop (u : Nat) (r : Option α)

/-- The state: the count and the claim bits of the record, the cells and the threads. It also holds the elements that
the calls in the log leave, and the log. -/
structure State (α : Type) where
  size : Nat
  bits : Nat → Bool
  cell : Nat → α
  threads : Nat → Thread α
  list : List α
  log : List (LinOp α)

/-- The thread `u` at a new step. -/
def State.at (s : State α) (u : Nat) (p : Phase α) : Nat → Thread α := update s.threads u ⟨(s.threads u).call, p⟩

/-- One step of thread `u`, with the ceiling `cap`. -/
def step (cap : Nat) (s : State α) (u : Nat) : State α :=
  match (s.threads u).call, (s.threads u).phase with
  | .get n, .idle =>
    if s.bits n then s else { s with bits := update s.bits n true, threads := s.at u (.held n) }
  | .set n _, .idle =>
    if s.bits n then s else { s with bits := update s.bits n true, threads := s.at u (.held n) }
  | .get n, .held _ =>
    if n < s.size then { s with log := s.log ++ [.get u n s.list[n]?], threads := s.at u (.live n s.list[n]?) }
    else { s with log := s.log ++ [.get u n none], threads := s.at u (.refusing n) }
  | .set n x, .held _ =>
    if n < s.size then
      { s with list := s.list.set n x, log := s.log ++ [.set u n x true], threads := s.at u (.live n none) }
    else { s with log := s.log ++ [.set u n x false], threads := s.at u (.refusing n) }
  | .get n, .live _ _ => { s with threads := s.at u (.worked n (some (s.cell n))) }
  | .set n x, .live _ none => { s with cell := update s.cell n x, threads := s.at u (.worked n none) }
  | .get n, .worked _ r => { s with bits := update s.bits n false, threads := s.at u (.done (.value r)) }
  | .set n _, .worked _ _ => { s with bits := update s.bits n false, threads := s.at u (.done (.ok true)) }
  | .get n, .refusing _ => { s with bits := update s.bits n false, threads := s.at u (.done (.value none)) }
  | .set n _, .refusing _ => { s with bits := update s.bits n false, threads := s.at u (.done (.ok false)) }
  | .push x, .idle =>
    if s.size < cap then { s with threads := s.at u (.sized s.size) }
    else { s with log := s.log ++ [.push u x false], threads := s.at u (.done (.ok false)) }
  | .push _, .sized k =>
    if s.bits k then { s with threads := s.at u .idle }
    else { s with bits := update s.bits k true, threads := s.at u (.held k) }
  | .push _, .held i =>
    if s.size = i then { s with threads := s.at u (.valid i) } else { s with threads := s.at u (.backoff i) }
  | .push x, .valid i => { s with cell := update s.cell i x, threads := s.at u (.wrote i) }
  | .push x, .wrote i =>
    if s.size = i then
      { s with size := i + 1, list := s.list ++ [x], log := s.log ++ [.push u x true], threads := s.at u (.worked i none) }
    else { s with threads := s.at u (.backoff i) }
  | .push _, .worked i _ => { s with bits := update s.bits i false, threads := s.at u (.done (.ok true)) }
  | .push _, .backoff i => { s with bits := update s.bits i false, threads := s.at u .idle }
  | .pop, .idle =>
    if s.size = 0 then { s with log := s.log ++ [.pop u none], threads := s.at u (.done (.value none)) }
    else { s with threads := s.at u (.sized s.size) }
  | .pop, .sized k =>
    if s.bits (k - 1) then { s with threads := s.at u .idle }
    else { s with bits := update s.bits (k - 1) true, threads := s.at u (.held (k - 1)) }
  | .pop, .held i =>
    if s.size = i + 1 then
      { s with
        size := i
        list := s.list.dropLast
        log := s.log ++ [.pop u s.list.getLast?]
        threads := s.at u (.live i s.list.getLast?) }
    else { s with threads := s.at u (.backoff i) }
  | .pop, .live i _ => { s with threads := s.at u (.worked i (some (s.cell i))) }
  | .pop, .worked i r => { s with bits := update s.bits i false, threads := s.at u (.done (.value r)) }
  | .pop, .backoff i => { s with bits := update s.bits i false, threads := s.at u .idle }
  | _, _ => s

/-- A schedule run from a state. -/
def run (cap : Nat) (s : State α) (sched : List Nat) : State α := sched.foldl (step cap) s

/-- The start: an empty vector, no bit held, each thread before its first step. -/
def start (cells : Nat → α) (calls : Nat → Call α) : State α :=
  ⟨0, fun _ => false, cells, fun u => ⟨calls u, .idle⟩, [], []⟩

/-! ## The vector with one lock -/

section Spec

variable [DecidableEq α]

/-- One call on the vector with one lock, with the ceiling `cap`. The answer in the log must be the answer of the
call on the elements. Where it is not, the history is not legal. -/
def seqStep (cap : Nat) : Option (List α) → LinOp α → Option (List α)
  | none, _ => none
  | some l, .get _ n r => if l[n]? = r then some l else none
  | some l, .set _ n x ok => if ok = decide (n < l.length) then some (if ok then l.set n x else l) else none
  | some l, .push _ x ok => if ok = decide (l.length < cap) then some (if ok then l ++ [x] else l) else none
  | some l, .pop _ r => if r = l.getLast? then some l.dropLast else none

def seqRun (cap : Nat) (log : List (LinOp α)) : Option (List α) := log.foldl (seqStep cap) (some [])

theorem seqRun_snoc (cap : Nat) (log : List (LinOp α)) (o : LinOp α) :
    seqRun cap (log ++ [o]) = seqStep cap (seqRun cap log) o := by
  simp [seqRun, List.foldl_append]

end Spec

/-! ## The invariant -/

/-- The point in the log of a thread after its call took effect. -/
def entryOf (u : Nat) (th : Thread α) : Option (LinOp α) :=
  match th.call, th.phase with
  | .get n, .live _ r => some (.get u n r)
  | .get n, .worked _ r => some (.get u n r)
  | .get n, .done (.value r) => some (.get u n r)
  | .get n, .refusing _ => some (.get u n none)
  | .set n x, .live _ _ => some (.set u n x true)
  | .set n x, .worked _ _ => some (.set u n x true)
  | .set n x, .refusing _ => some (.set u n x false)
  | .set n x, .done (.ok b) => some (.set u n x b)
  | .push x, .worked _ _ => some (.push u x true)
  | .push x, .done (.ok b) => some (.push u x b)
  | .pop, .live _ r => some (.pop u r)
  | .pop, .worked _ r => some (.pop u r)
  | .pop, .done (.value r) => some (.pop u r)
  | _, _ => none

/-- What holds of one thread. Its bit is the element of its call. A `set` after its point has its value in the list,
and an `at` after its point has the element it gives. A push that checked the count is at or past the count, and
after its write its cell holds its value. A pop after its point is at or past the count, and its cell holds what it
gives. Each point of the thread is in the log. -/
structure Good (cap : Nat) (s : State α) (u : Nat) (th : Thread α) : Prop where
  idx : ∀ n i, (th.call = .get n ∨ ∃ x, th.call = .set n x) → th.phase.holds = some i → i = n
  setLive : ∀ n x i, th.call = .set n x → th.phase = .live i none → i < s.size ∧ s.list[i]? = some x
  getLive : ∀ n i r, th.call = .get n → th.phase = .live i r → i < s.size ∧ s.list[i]? = r
  pushTop : ∀ x i, th.call = .push x → (th.phase = .valid i ∨ th.phase = .wrote i) → s.size ≤ i
  pushCell : ∀ x i, th.call = .push x → th.phase = .wrote i → s.cell i = x
  pushCap : ∀ x k, th.call = .push x → (th.phase = .sized k ∨ th.phase.holds = some k) → k < cap
  popTop : ∀ i r, th.call = .pop → (th.phase = .live i r ∨ th.phase = .worked i r) → s.size ≤ i
  popCell : ∀ i r, th.call = .pop → th.phase = .live i r → r = some (s.cell i)
  popSized : ∀ k, th.call = .pop → th.phase = .sized k → 0 < k
  logged : ∀ o, entryOf u th = some o → o ∈ s.log

/-- The invariant. The count is the length of the list, at max the ceiling, and the log runs on the vector with one
lock to the list. A bit is on where one thread holds its element, and only there. Each cell below the count holds
its element of the list, but where a `set` took effect and has not written yet. Each thread is good. -/
structure Inv [DecidableEq α] (cap : Nat) (s : State α) : Prop where
  len : s.list.length = s.size
  ceil : s.size ≤ cap
  legal : seqRun cap s.log = some s.list
  bits : ∀ i, s.bits i = true ↔ ∃ u, (s.threads u).phase.holds = some i
  uniq : ∀ u w i, (s.threads u).phase.holds = some i → (s.threads w).phase.holds = some i → u = w
  cells : ∀ i, i < s.size → s.list[i]? = some (s.cell i) ∨
    ∃ u x, (s.threads u).call = .set i x ∧ (s.threads u).phase = .live i none
  good : ∀ u, Good cap s u (s.threads u)

theorem at_same (s : State α) (u : Nat) (p : Phase α) : s.at u p u = ⟨(s.threads u).call, p⟩ := by
  simp [State.at]

theorem at_other (s : State α) (u w : Nat) (p : Phase α) (h : w ≠ u) : s.at u p w = s.threads w := by
  simp [State.at, h]

/-- A thread stays good where the count, the list and the cells stay, and the log only grows. -/
theorem good_mono (cap : Nat) (s s' : State α) (u : Nat) (th : Thread α) (h1 : s'.size = s.size)
    (h2 : s'.list = s.list) (h3 : s'.cell = s.cell) (h4 : ∀ o ∈ s.log, o ∈ s'.log) (hg : Good cap s u th) :
    Good cap s' u th := by
  refine ⟨hg.idx, ?_, ?_, ?_, ?_, hg.pushCap, ?_, ?_, hg.popSized, fun o ho => h4 o (hg.logged o ho)⟩
  · intro n x i hc hp; rw [h1, h2]; exact hg.setLive n x i hc hp
  · intro n i r hc hp; rw [h1, h2]; exact hg.getLive n i r hc hp
  · intro x i hc hp; rw [h1]; exact hg.pushTop x i hc hp
  · intro x i hc hp; rw [h3]; exact hg.pushCell x i hc hp
  · intro i r hc hp; rw [h1]; exact hg.popTop i r hc hp
  · intro i r hc hp; rw [h3]; exact hg.popCell i r hc hp

theorem start_inv [DecidableEq α] (cap : Nat) (cells : Nat → α) (calls : Nat → Call α) :
    Inv cap (start cells calls) := by
  refine ⟨rfl, Nat.zero_le _, rfl, ?_, ?_, ?_, ?_⟩
  · intro i; simp [start, Phase.holds]
  · intro u w i h; simp [start, Phase.holds] at h
  · intro i h; simp [start] at h
  · intro u
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;> intros <;> simp_all [start, Phase.holds, entryOf]

section Steps

variable [DecidableEq α] (cap : Nat)

/-- The cells of a state where a thread `u`, which is no `set` with its point passed before or after, changes its
step. -/
theorem cells_frame (s : State α) (hs : Inv cap s) (u : Nat) (p : Phase α) (s' : State α)
    (hsize : s'.size = s.size) (hlist : s'.list = s.list) (hcell : s'.cell = s.cell) (hth : s'.threads = s.at u p)
    (hsetl : ∀ n x i, (s.threads u).call = .set n x → (s.threads u).phase ≠ .live i none) :
    ∀ i, i < s'.size → s'.list[i]? = some (s'.cell i) ∨
      ∃ w x, (s'.threads w).call = .set i x ∧ (s'.threads w).phase = .live i none := by
  intro i hi
  rw [hsize] at hi
  rcases hs.cells i hi with h | ⟨w, x, hc, hp⟩
  · left; rw [hlist, hcell]; exact h
  · right
    have hwu : w ≠ u := fun he => by subst he; exact hsetl i x i hc hp
    refine ⟨w, x, ?_, ?_⟩ <;> rw [hth, at_other _ _ _ _ hwu]
    · exact hc
    · exact hp

/-- A step that changes the step of thread `u` alone, with the bit it holds as it was. -/
theorem inv_phase (s : State α) (hs : Inv cap s) (u : Nat) (p : Phase α)
    (hholds : p.holds = (s.threads u).phase.holds)
    (hsetl : ∀ n x i, (s.threads u).call = .set n x → (s.threads u).phase ≠ .live i none)
    (hgood : Good cap s u ⟨(s.threads u).call, p⟩) : Inv cap { s with threads := s.at u p } := by
  have hh : ∀ w, ((s.at u p) w).phase.holds = (s.threads w).phase.holds := by
    intro w
    by_cases hwu : w = u
    · subst hwu; rw [at_same]; exact hholds
    · rw [at_other _ _ _ _ hwu]
  refine ⟨hs.len, hs.ceil, hs.legal, ?_, ?_, cells_frame cap s hs u p _ rfl rfl rfl rfl hsetl, ?_⟩
  · intro i
    show s.bits i = true ↔ ∃ w, ((s.at u p) w).phase.holds = some i
    simp only [hh]
    exact hs.bits i
  · intro w1 w2 i h1 h2
    simp only [hh] at h1 h2
    exact hs.uniq w1 w2 i h1 h2
  · intro w
    by_cases hwu : w = u
    · subst hwu
      show Good cap { s with threads := s.at w p } w (s.at w p w)
      rw [at_same]
      exact good_mono cap s _ w _ rfl rfl rfl (fun _ h => h) hgood
    · show Good cap { s with threads := s.at u p } w (s.at u p w)
      rw [at_other _ _ _ _ hwu]
      exact good_mono cap s _ w _ rfl rfl rfl (fun _ h => h) (hs.good w)

/-- A claim of a free bit: thread `u`, which holds no bit, takes the bit of element `i`. -/
theorem inv_claim (s : State α) (hs : Inv cap s) (u i : Nat) (p : Phase α) (hfree : s.bits i = false)
    (hnone : (s.threads u).phase.holds = none) (hp : p.holds = some i)
    (hsetl : ∀ n x i, (s.threads u).call = .set n x → (s.threads u).phase ≠ .live i none)
    (hgood : Good cap s u ⟨(s.threads u).call, p⟩) :
    Inv cap { s with bits := update s.bits i true, threads := s.at u p } := by
  have hnoholder : ∀ w, (s.threads w).phase.holds ≠ some i := fun w hw => by
    have := (hs.bits i).2 ⟨w, hw⟩
    rw [hfree] at this
    cases this
  refine ⟨hs.len, hs.ceil, hs.legal, ?_, ?_, cells_frame cap s hs u p _ rfl rfl rfl rfl hsetl, ?_⟩
  · intro j
    show update s.bits i true j = true ↔ ∃ w, ((s.at u p) w).phase.holds = some j
    by_cases hj : j = i
    · subst hj
      simp only [update_same, true_iff]
      exact ⟨u, by rw [at_same]; exact hp⟩
    · rw [update_other _ _ _ _ hj, hs.bits j]
      constructor
      · rintro ⟨w, hw⟩
        have hwu : w ≠ u := fun he => by subst he; rw [hnone] at hw; cases hw
        exact ⟨w, by rw [at_other _ _ _ _ hwu]; exact hw⟩
      · rintro ⟨w, hw⟩
        by_cases hwu : w = u
        · subst hwu; rw [at_same] at hw; rw [hp] at hw; cases hw; exact absurd rfl hj
        · rw [at_other _ _ _ _ hwu] at hw; exact ⟨w, hw⟩
  · intro w1 w2 j h1 h2
    change ((s.at u p) w1).phase.holds = some j at h1
    change ((s.at u p) w2).phase.holds = some j at h2
    by_cases h1u : w1 = u <;> by_cases h2u : w2 = u
    · rw [h1u, h2u]
    · subst h1u
      rw [at_same, hp] at h1
      cases h1
      rw [at_other _ _ _ _ h2u] at h2
      exact absurd h2 (hnoholder w2)
    · subst h2u
      rw [at_same, hp] at h2
      cases h2
      rw [at_other _ _ _ _ h1u] at h1
      exact absurd h1 (hnoholder w1)
    · rw [at_other _ _ _ _ h1u] at h1
      rw [at_other _ _ _ _ h2u] at h2
      exact hs.uniq w1 w2 j h1 h2
  · intro w
    by_cases hwu : w = u
    · subst hwu
      show Good cap { s with bits := update s.bits i true, threads := s.at w p } w (s.at w p w)
      rw [at_same]
      exact good_mono cap s _ w _ rfl rfl rfl (fun _ h => h) hgood
    · show Good cap { s with bits := update s.bits i true, threads := s.at u p } w (s.at u p w)
      rw [at_other _ _ _ _ hwu]
      exact good_mono cap s _ w _ rfl rfl rfl (fun _ h => h) (hs.good w)

/-- A release: thread `u` gives back the bit of element `i` that it holds. -/
theorem inv_release (s : State α) (hs : Inv cap s) (u i : Nat) (p : Phase α)
    (hheld : (s.threads u).phase.holds = some i) (hp : p.holds = none)
    (hsetl : ∀ n x i, (s.threads u).call = .set n x → (s.threads u).phase ≠ .live i none)
    (hgood : Good cap s u ⟨(s.threads u).call, p⟩) :
    Inv cap { s with bits := update s.bits i false, threads := s.at u p } := by
  refine ⟨hs.len, hs.ceil, hs.legal, ?_, ?_, cells_frame cap s hs u p _ rfl rfl rfl rfl hsetl, ?_⟩
  · intro j
    show update s.bits i false j = true ↔ ∃ w, ((s.at u p) w).phase.holds = some j
    by_cases hj : j = i
    · subst hj
      simp only [update_same, Bool.false_eq_true, false_iff, not_exists]
      intro w hw
      by_cases hwu : w = u
      · subst hwu; rw [at_same, hp] at hw; cases hw
      · rw [at_other _ _ _ _ hwu] at hw
        exact hwu (hs.uniq w u j hw hheld)
    · rw [update_other _ _ _ _ hj, hs.bits j]
      constructor
      · rintro ⟨w, hw⟩
        have hwu : w ≠ u := fun he => by subst he; rw [hheld] at hw; cases hw; exact hj rfl
        exact ⟨w, by rw [at_other _ _ _ _ hwu]; exact hw⟩
      · rintro ⟨w, hw⟩
        by_cases hwu : w = u
        · subst hwu; rw [at_same, hp] at hw; cases hw
        · rw [at_other _ _ _ _ hwu] at hw; exact ⟨w, hw⟩
  · intro w1 w2 j h1 h2
    change ((s.at u p) w1).phase.holds = some j at h1
    change ((s.at u p) w2).phase.holds = some j at h2
    by_cases h1u : w1 = u
    · subst h1u; rw [at_same, hp] at h1; cases h1
    · by_cases h2u : w2 = u
      · subst h2u; rw [at_same, hp] at h2; cases h2
      · rw [at_other _ _ _ _ h1u] at h1
        rw [at_other _ _ _ _ h2u] at h2
        exact hs.uniq w1 w2 j h1 h2
  · intro w
    by_cases hwu : w = u
    · subst hwu
      show Good cap { s with bits := update s.bits i false, threads := s.at w p } w (s.at w p w)
      rw [at_same]
      exact good_mono cap s _ w _ rfl rfl rfl (fun _ h => h) hgood
    · show Good cap { s with bits := update s.bits i false, threads := s.at u p } w (s.at u p w)
      rw [at_other _ _ _ _ hwu]
      exact good_mono cap s _ w _ rfl rfl rfl (fun _ h => h) (hs.good w)

/-- A point that only appends to the log: the call of thread `u` takes effect with the list as it was. -/
theorem inv_logged (s : State α) (hs : Inv cap s) (u : Nat) (p : Phase α) (o : LinOp α)
    (hholds : p.holds = (s.threads u).phase.holds)
    (hsetl : ∀ n x i, (s.threads u).call = .set n x → (s.threads u).phase ≠ .live i none)
    (hstep : seqStep cap (some s.list) o = some s.list)
    (hgood : Good cap { s with log := s.log ++ [o], threads := s.at u p } u ⟨(s.threads u).call, p⟩) :
    Inv cap { s with log := s.log ++ [o], threads := s.at u p } := by
  have hh : ∀ w, ((s.at u p) w).phase.holds = (s.threads w).phase.holds := by
    intro w
    by_cases hwu : w = u
    · subst hwu; rw [at_same]; exact hholds
    · rw [at_other _ _ _ _ hwu]
  refine ⟨hs.len, hs.ceil, ?_, ?_, ?_, cells_frame cap s hs u p _ rfl rfl rfl rfl hsetl, ?_⟩
  · show seqRun cap (s.log ++ [o]) = some s.list
    rw [seqRun_snoc, hs.legal, hstep]
  · intro i
    show s.bits i = true ↔ ∃ w, ((s.at u p) w).phase.holds = some i
    simp only [hh]
    exact hs.bits i
  · intro w1 w2 i h1 h2
    change ((s.at u p) w1).phase.holds = some i at h1
    change ((s.at u p) w2).phase.holds = some i at h2
    rw [hh] at h1 h2
    exact hs.uniq w1 w2 i h1 h2
  · intro w
    by_cases hwu : w = u
    · subst hwu
      show Good cap _ w (s.at w p w)
      rw [at_same]
      exact hgood
    · show Good cap _ w (s.at u p w)
      rw [at_other _ _ _ _ hwu]
      exact good_mono cap s _ w _ rfl rfl rfl (fun o' h => List.mem_append_left _ h) (hs.good w)

omit [DecidableEq α] in
/-- A thread that holds no bit of element `i` stays good where the list and the cells change at `i` alone and the
count stays. -/
theorem good_at (s s' : State α) (u i : Nat) (th : Thread α) (hhold : th.phase.holds ≠ some i)
    (hsize : s'.size = s.size) (hlist : ∀ j, j ≠ i → s'.list[j]? = s.list[j]?)
    (hcell : ∀ j, j ≠ i → s'.cell j = s.cell j) (hlog : ∀ o ∈ s.log, o ∈ s'.log) (hg : Good cap s u th) :
    Good cap s' u th := by
  refine ⟨hg.idx, ?_, ?_, ?_, ?_, hg.pushCap, ?_, ?_, hg.popSized, fun o ho => hlog o (hg.logged o ho)⟩
  · intro n x j hc hp
    have hj : j ≠ i := fun he => hhold (by rw [hp, he]; rfl)
    rw [hsize, hlist j hj]
    exact hg.setLive n x j hc hp
  · intro n j r hc hp
    have hj : j ≠ i := fun he => hhold (by rw [hp, he]; rfl)
    rw [hsize, hlist j hj]
    exact hg.getLive n j r hc hp
  · intro x j hc hp; rw [hsize]; exact hg.pushTop x j hc hp
  · intro x j hc hp
    have hj : j ≠ i := fun he => hhold (by rcases hp with hp; rw [hp, he]; rfl)
    rw [hcell j hj]
    exact hg.pushCell x j hc hp
  · intro j r hc hp; rw [hsize]; exact hg.popTop j r hc hp
  · intro j r hc hp
    have hj : j ≠ i := fun he => hhold (by rw [hp, he]; rfl)
    rw [hcell j hj]
    exact hg.popCell j r hc hp

omit [DecidableEq α] in
/-- A thread that holds no bit of the element at the count stays good where a push appends that element. -/
theorem good_grow (s s' : State α) (u : Nat) (x : α) (th : Thread α) (hhold : th.phase.holds ≠ some s.size)
    (hlen : s.list.length = s.size) (hsize : s'.size = s.size + 1) (hlist : s'.list = s.list ++ [x])
    (hcell : s'.cell = s.cell) (hlog : ∀ o ∈ s.log, o ∈ s'.log) (hg : Good cap s u th) : Good cap s' u th := by
  refine ⟨hg.idx, ?_, ?_, ?_, ?_, hg.pushCap, ?_, ?_, hg.popSized, fun o ho => hlog o (hg.logged o ho)⟩
  · intro n y j hc hp
    obtain ⟨hj, hv⟩ := hg.setLive n y j hc hp
    rw [hsize, hlist, List.getElem?_append_left (by omega)]
    exact ⟨by omega, hv⟩
  · intro n j r hc hp
    obtain ⟨hj, hv⟩ := hg.getLive n j r hc hp
    rw [hsize, hlist, List.getElem?_append_left (by omega)]
    exact ⟨by omega, hv⟩
  · intro y j hc hp
    have h1 := hg.pushTop y j hc hp
    have hj : j ≠ s.size := fun he => hhold (by rcases hp with hp | hp <;> rw [hp, he] <;> rfl)
    rw [hsize]
    omega
  · intro y j hc hp; rw [hcell]; exact hg.pushCell y j hc hp
  · intro j r hc hp
    have h1 := hg.popTop j r hc hp
    have hj : j ≠ s.size := fun he => hhold (by rcases hp with hp | hp <;> rw [hp, he] <;> rfl)
    rw [hsize]
    omega
  · intro j r hc hp; rw [hcell]; exact hg.popCell j r hc hp

omit [DecidableEq α] in
theorem dropLast_get (l : List α) (j : Nat) (h : j + 1 < l.length) : l.dropLast[j]? = l[j]? := by
  rw [List.dropLast_eq_take, List.getElem?_take]
  simp [show j < l.length - 1 by omega]

omit [DecidableEq α] in
/-- A thread that holds no bit of the last element stays good where a pop removes that element. -/
theorem good_shrink (s s' : State α) (u i : Nat) (th : Thread α) (hhold : th.phase.holds ≠ some i)
    (hlen : s.list.length = s.size) (hsz : s.size = i + 1) (hsize : s'.size = i) (hlist : s'.list = s.list.dropLast)
    (hcell : s'.cell = s.cell) (hlog : ∀ o ∈ s.log, o ∈ s'.log) (hg : Good cap s u th) : Good cap s' u th := by
  refine ⟨hg.idx, ?_, ?_, ?_, ?_, hg.pushCap, ?_, ?_, hg.popSized, fun o ho => hlog o (hg.logged o ho)⟩
  · intro n y j hc hp
    obtain ⟨hj, hv⟩ := hg.setLive n y j hc hp
    have hji : j ≠ i := fun he => hhold (by rw [hp, he]; rfl)
    rw [hsize, hlist, dropLast_get _ _ (by omega)]
    exact ⟨by omega, hv⟩
  · intro n j r hc hp
    obtain ⟨hj, hv⟩ := hg.getLive n j r hc hp
    have hji : j ≠ i := fun he => hhold (by rw [hp, he]; rfl)
    rw [hsize, hlist, dropLast_get _ _ (by omega)]
    exact ⟨by omega, hv⟩
  · intro y j hc hp
    have := hg.pushTop y j hc hp
    rw [hsize]
    omega
  · intro y j hc hp; rw [hcell]; exact hg.pushCell y j hc hp
  · intro j r hc hp
    have := hg.popTop j r hc hp
    rw [hsize]
    omega
  · intro j r hc hp; rw [hcell]; exact hg.popCell j r hc hp

omit [DecidableEq α] in
/-- The holds of the threads where thread `u` takes a new step that holds the same bit. -/
theorem holds_same (s : State α) (u : Nat) (p : Phase α) (hholds : p.holds = (s.threads u).phase.holds) (w : Nat) :
    ((s.at u p) w).phase.holds = (s.threads w).phase.holds := by
  by_cases hwu : w = u
  · subst hwu; rw [at_same]; exact hholds
  · rw [at_other _ _ _ _ hwu]

/-- The point of a `set` that is legal: the list changes at its element, and the cell waits for the write. -/
theorem inv_setPoint (s : State α) (hs : Inv cap s) (u n : Nat) (x : α) (i : Nat) (hc : (s.threads u).call = .set n x)
    (hp : (s.threads u).phase = .held i) (hn : n < s.size) :
    Inv cap { s with list := s.list.set n x, log := s.log ++ [.set u n x true], threads := s.at u (.live n none) } := by
  have hin : i = n := (hs.good u).idx n i (Or.inr ⟨x, hc⟩) (by rw [hp]; rfl)
  subst hin
  have hholds : (Phase.live i none : Phase α).holds = (s.threads u).phase.holds := by rw [hp]; rfl
  have hh := holds_same s u _ hholds
  have hlen := hs.len
  have hother : ∀ w, w ≠ u → (s.threads w).phase.holds ≠ some i := fun w hwu hw =>
    hwu (hs.uniq w u i hw (by rw [hp]; rfl))
  refine ⟨by simp [hlen], hs.ceil, ?_, ?_, ?_, ?_, ?_⟩
  · show seqRun cap (s.log ++ [.set u i x true]) = some (s.list.set i x)
    rw [seqRun_snoc, hs.legal]
    simp [seqStep, hlen, hn]
  · intro j
    show s.bits j = true ↔ ∃ w, ((s.at u (.live i none)) w).phase.holds = some j
    simp only [hh]
    exact hs.bits j
  · intro w1 w2 j h1 h2
    change ((s.at u (.live i none)) w1).phase.holds = some j at h1
    change ((s.at u (.live i none)) w2).phase.holds = some j at h2
    rw [hh] at h1 h2
    exact hs.uniq w1 w2 j h1 h2
  · intro j hj
    by_cases hji : j = i
    · subst hji
      right
      refine ⟨u, x, ?_, ?_⟩
      · show (s.at u _ u).call = _
        rw [at_same]
        exact hc
      · show (s.at u _ u).phase = _
        rw [at_same]
    · show (s.list.set i x)[j]? = some (s.cell j) ∨
        ∃ w y, ((s.at u (.live i none)) w).call = .set j y ∧ ((s.at u (.live i none)) w).phase = .live j none
      rw [List.getElem?_set_ne (Ne.symm hji)]
      rcases hs.cells j hj with h | ⟨w, y, hcw, hpw⟩
      · exact Or.inl h
      · have hwu : w ≠ u := fun he => by subst he; rw [hp] at hpw; cases hpw
        exact Or.inr ⟨w, y, by rw [at_other _ _ _ _ hwu]; exact hcw, by rw [at_other _ _ _ _ hwu]; exact hpw⟩
  · intro w
    by_cases hwu : w = u
    · subst hwu
      show Good cap _ w (s.at w (.live i none) w)
      rw [at_same, hc]
      refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
      · intro n' j _ hj; simp [Phase.holds] at hj; exact hj.symm.trans (by
          rcases (by assumption : (Call.set i x = Call.get n' ∨ ∃ y, Call.set i x = Call.set n' y)) with h | ⟨y, h⟩
          · cases h
          · cases h; rfl)
      · intro n' y j hc' hp'
        cases hc'
        simp at hp'
        subst hp'
        exact ⟨by simp [hn], by simp [hlen, hn]⟩
      · intro n' j r hc' _; cases hc'
      · intro y j hc' _; cases hc'
      · intro y j hc' _; cases hc'
      · intro y k hc' _; cases hc'
      · intro j r hc' _; cases hc'
      · intro j r hc' _; cases hc'
      · intro k hc' _; cases hc'
      · intro o ho
        simp [entryOf] at ho
        subst ho
        simp
    · show Good cap _ w (s.at u (.live i none) w)
      rw [at_other _ _ _ _ hwu]
      exact good_at cap s _ w i _ (hother w hwu) rfl (fun j hj => List.getElem?_set_ne (Ne.symm hj))
        (fun _ _ => rfl) (fun o h => List.mem_append_left _ h) (hs.good w)

/-- The write of a `set` after its point: the cell takes the value that the list took at the point. -/
theorem inv_setWrite (s : State α) (hs : Inv cap s) (u n : Nat) (x : α) (i : Nat) (hc : (s.threads u).call = .set n x)
    (hp : (s.threads u).phase = .live i none) :
    Inv cap { s with cell := update s.cell n x, threads := s.at u (.worked n none) } := by
  have hin : i = n := (hs.good u).idx n i (Or.inr ⟨x, hc⟩) (by rw [hp]; rfl)
  subst hin
  have hholds : (Phase.worked i none : Phase α).holds = (s.threads u).phase.holds := by rw [hp]; rfl
  have hh := holds_same s u _ hholds
  obtain ⟨_, hlx⟩ := (hs.good u).setLive i x i hc hp
  have hother : ∀ w, w ≠ u → (s.threads w).phase.holds ≠ some i := fun w hwu hw =>
    hwu (hs.uniq w u i hw (by rw [hp]; rfl))
  refine ⟨hs.len, hs.ceil, hs.legal, ?_, ?_, ?_, ?_⟩
  · intro j
    show s.bits j = true ↔ ∃ w, ((s.at u (.worked i none)) w).phase.holds = some j
    simp only [hh]
    exact hs.bits j
  · intro w1 w2 j h1 h2
    change ((s.at u (.worked i none)) w1).phase.holds = some j at h1
    change ((s.at u (.worked i none)) w2).phase.holds = some j at h2
    rw [hh] at h1 h2
    exact hs.uniq w1 w2 j h1 h2
  · intro j hj
    by_cases hji : j = i
    · subst hji
      left
      show s.list[j]? = some (update s.cell j x j)
      rw [update_same]
      exact hlx
    · show s.list[j]? = some (update s.cell i x j) ∨
        ∃ w y, ((s.at u (.worked i none)) w).call = .set j y ∧ ((s.at u (.worked i none)) w).phase = .live j none
      rw [update_other _ _ _ _ hji]
      rcases hs.cells j hj with h | ⟨w, y, hcw, hpw⟩
      · exact Or.inl h
      · have hwu : w ≠ u := fun he => by subst he; rw [hp] at hpw; cases hpw; exact hji rfl
        exact Or.inr ⟨w, y, by rw [at_other _ _ _ _ hwu]; exact hcw, by rw [at_other _ _ _ _ hwu]; exact hpw⟩
  · intro w
    by_cases hwu : w = u
    · subst hwu
      show Good cap _ w (s.at w (.worked i none) w)
      rw [at_same, hc]
      have hlog := (hs.good w).logged (.set w i x true) (by simp [entryOf, hc, hp])
      refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
      · intro n' j hn' hj
        simp [Phase.holds] at hj
        rcases hn' with h | ⟨y, h⟩
        · cases h
        · cases h; exact hj.symm
      · intro n' y j _ hp'; cases hp'
      · intro n' j r hc' _; cases hc'
      · intro y j hc' _; cases hc'
      · intro y j hc' _; cases hc'
      · intro y k hc' _; cases hc'
      · intro j r hc' _; cases hc'
      · intro j r hc' _; cases hc'
      · intro k hc' _; cases hc'
      · intro o ho
        simp [entryOf] at ho
        subst ho
        exact hlog
    · show Good cap _ w (s.at u (.worked i none) w)
      rw [at_other _ _ _ _ hwu]
      exact good_at cap s _ w i _ (hother w hwu) rfl (fun _ _ => rfl)
        (fun j hj => update_other _ _ _ _ hj) (fun _ h => h) (hs.good w)

/-- The write of a push that checked the count: its cell is at or past the count, so no element of the list changes. -/
theorem inv_pushWrite (s : State α) (hs : Inv cap s) (u : Nat) (x : α) (i : Nat) (hc : (s.threads u).call = .push x)
    (hp : (s.threads u).phase = .valid i) :
    Inv cap { s with cell := update s.cell i x, threads := s.at u (.wrote i) } := by
  have hholds : (Phase.wrote i : Phase α).holds = (s.threads u).phase.holds := by rw [hp]; rfl
  have hh := holds_same s u _ hholds
  have htop := (hs.good u).pushTop x i hc (Or.inl hp)
  have hcap := (hs.good u).pushCap x i hc (Or.inr (by rw [hp]; rfl))
  have hother : ∀ w, w ≠ u → (s.threads w).phase.holds ≠ some i := fun w hwu hw =>
    hwu (hs.uniq w u i hw (by rw [hp]; rfl))
  refine ⟨hs.len, hs.ceil, hs.legal, ?_, ?_, ?_, ?_⟩
  · intro j
    show s.bits j = true ↔ ∃ w, ((s.at u (.wrote i)) w).phase.holds = some j
    simp only [hh]
    exact hs.bits j
  · intro w1 w2 j h1 h2
    change ((s.at u (.wrote i)) w1).phase.holds = some j at h1
    change ((s.at u (.wrote i)) w2).phase.holds = some j at h2
    rw [hh] at h1 h2
    exact hs.uniq w1 w2 j h1 h2
  · intro j hj
    have hj' : j < s.size := hj
    have hji : j ≠ i := by omega
    show s.list[j]? = some (update s.cell i x j) ∨
      ∃ w y, ((s.at u (.wrote i)) w).call = .set j y ∧ ((s.at u (.wrote i)) w).phase = .live j none
    rw [update_other _ _ _ _ hji]
    rcases hs.cells j hj with h | ⟨w, y, hcw, hpw⟩
    · exact Or.inl h
    · have hwu : w ≠ u := fun he => by subst he; rw [hc] at hcw; cases hcw
      exact Or.inr ⟨w, y, by rw [at_other _ _ _ _ hwu]; exact hcw, by rw [at_other _ _ _ _ hwu]; exact hpw⟩
  · intro w
    by_cases hwu : w = u
    · subst hwu
      show Good cap _ w (s.at w (.wrote i) w)
      rw [at_same, hc]
      refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
      · intro n' j hn' _
        rcases hn' with h | ⟨y, h⟩ <;> cases h
      · intro n' y j hc' _; cases hc'
      · intro n' j r hc' _; cases hc'
      · intro y j hc' hp'
        cases hc'
        rcases hp' with hp' | hp' <;> cases hp'
        exact htop
      · intro y j hc' hp'
        cases hc'
        cases hp'
        simp
      · intro y k hc' hp'
        cases hc'
        rcases hp' with hp' | hp'
        · cases hp'
        · simp [Phase.holds] at hp'; subst hp'; exact hcap
      · intro j r hc' _; cases hc'
      · intro j r hc' _; cases hc'
      · intro k hc' _; cases hc'
      · intro o ho; simp [entryOf] at ho
    · show Good cap _ w (s.at u (.wrote i) w)
      rw [at_other _ _ _ _ hwu]
      exact good_at cap s _ w i _ (hother w hwu) rfl (fun _ _ => rfl)
        (fun j hj => update_other _ _ _ _ hj) (fun _ h => h) (hs.good w)

/-- The CAS of a push: the count goes from `i` to `i + 1`, and the list takes the value that the cell holds. -/
theorem inv_pushCommit (s : State α) (hs : Inv cap s) (u : Nat) (x : α) (i : Nat) (hc : (s.threads u).call = .push x)
    (hp : (s.threads u).phase = .wrote i) (hsz : s.size = i) :
    Inv cap
      { s with
        size := i + 1
        list := s.list ++ [x]
        log := s.log ++ [.push u x true]
        threads := s.at u (.worked i none) } := by
  have hholds : (Phase.worked i none : Phase α).holds = (s.threads u).phase.holds := by rw [hp]; rfl
  have hh := holds_same s u _ hholds
  have hcell := (hs.good u).pushCell x i hc hp
  have hcap := (hs.good u).pushCap x i hc (Or.inr (by rw [hp]; rfl))
  have hlen := hs.len
  have hother : ∀ w, w ≠ u → (s.threads w).phase.holds ≠ some i := fun w hwu hw =>
    hwu (hs.uniq w u i hw (by rw [hp]; rfl))
  refine ⟨by simp [hlen, hsz], by show i + 1 ≤ cap; omega, ?_, ?_, ?_, ?_, ?_⟩
  · show seqRun cap (s.log ++ [.push u x true]) = some (s.list ++ [x])
    rw [seqRun_snoc, hs.legal]
    simp [seqStep, hlen, hsz, hcap]
  · intro j
    show s.bits j = true ↔ ∃ w, ((s.at u (.worked i none)) w).phase.holds = some j
    simp only [hh]
    exact hs.bits j
  · intro w1 w2 j h1 h2
    change ((s.at u (.worked i none)) w1).phase.holds = some j at h1
    change ((s.at u (.worked i none)) w2).phase.holds = some j at h2
    rw [hh] at h1 h2
    exact hs.uniq w1 w2 j h1 h2
  · intro j hj
    show (s.list ++ [x])[j]? = some (s.cell j) ∨
      ∃ w y, ((s.at u (.worked i none)) w).call = .set j y ∧ ((s.at u (.worked i none)) w).phase = .live j none
    by_cases hji : j = i
    · subst hji
      left
      rw [List.getElem?_append_right (by omega), hcell]
      simp [hlen, hsz]
    · have hj1 : j < i + 1 := hj
      have hj' : j < s.size := by omega
      rw [List.getElem?_append_left (by omega)]
      rcases hs.cells j hj' with h | ⟨w, y, hcw, hpw⟩
      · exact Or.inl h
      · have hwu : w ≠ u := fun he => by subst he; rw [hc] at hcw; cases hcw
        exact Or.inr ⟨w, y, by rw [at_other _ _ _ _ hwu]; exact hcw, by rw [at_other _ _ _ _ hwu]; exact hpw⟩
  · intro w
    by_cases hwu : w = u
    · subst hwu
      show Good cap _ w (s.at w (.worked i none) w)
      rw [at_same, hc]
      refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
      · intro n' j hn' _
        rcases hn' with h | ⟨y, h⟩ <;> cases h
      · intro n' y j hc' _; cases hc'
      · intro n' j r hc' _; cases hc'
      · intro y j hc' hp'; rcases hp' with hp' | hp' <;> cases hp'
      · intro y j hc' hp'; cases hp'
      · intro y k hc' hp'
        cases hc'
        rcases hp' with hp' | hp'
        · cases hp'
        · simp [Phase.holds] at hp'; subst hp'; exact hcap
      · intro j r hc' _; cases hc'
      · intro j r hc' _; cases hc'
      · intro k hc' _; cases hc'
      · intro o ho
        simp [entryOf] at ho
        subst ho
        simp
    · show Good cap _ w (s.at u (.worked i none) w)
      rw [at_other _ _ _ _ hwu]
      exact good_grow cap s _ w x (s.threads w) (by rw [hsz]; exact hother w hwu) hlen (by simp [hsz])
        rfl rfl (fun o h => List.mem_append_left _ h) (hs.good w)

/-- The CAS of a pop: the count goes from `i + 1` to `i`, and the last element leaves the list. The pop holds the bit
of that element, so no `set` waits to write it, and the cell holds it. -/
theorem inv_popCommit (s : State α) (hs : Inv cap s) (u : Nat) (i : Nat) (hc : (s.threads u).call = .pop)
    (hp : (s.threads u).phase = .held i) (hsz : s.size = i + 1) :
    Inv cap
      { s with
        size := i
        list := s.list.dropLast
        log := s.log ++ [.pop u s.list.getLast?]
        threads := s.at u (.live i s.list.getLast?) } := by
  have hholds : (Phase.live i s.list.getLast? : Phase α).holds = (s.threads u).phase.holds := by rw [hp]; rfl
  have hh := holds_same s u _ hholds
  have hlen := hs.len
  have hother : ∀ w, w ≠ u → (s.threads w).phase.holds ≠ some i := fun w hwu hw =>
    hwu (hs.uniq w u i hw (by rw [hp]; rfl))
  have hlast : s.list.getLast? = s.list[i]? := by
    rw [List.getLast?_eq_getElem?, hlen, hsz]
    rfl
  have hcelli : s.list[i]? = some (s.cell i) := by
    rcases hs.cells i (by omega) with h | ⟨w, y, hcw, hpw⟩
    · exact h
    · have hwu : w = u := hs.uniq w u i (by rw [hpw]; rfl) (by rw [hp]; rfl)
      subst hwu
      rw [hc] at hcw
      cases hcw
  refine ⟨by simp [hlen, hsz], by show i ≤ cap; have := hs.ceil; omega, ?_, ?_, ?_, ?_, ?_⟩
  · show seqRun cap (s.log ++ [.pop u s.list.getLast?]) = some s.list.dropLast
    rw [seqRun_snoc, hs.legal]
    simp [seqStep]
  · intro j
    show s.bits j = true ↔ ∃ w, ((s.at u (.live i s.list.getLast?)) w).phase.holds = some j
    simp only [hh]
    exact hs.bits j
  · intro w1 w2 j h1 h2
    change ((s.at u (.live i s.list.getLast?)) w1).phase.holds = some j at h1
    change ((s.at u (.live i s.list.getLast?)) w2).phase.holds = some j at h2
    rw [hh] at h1 h2
    exact hs.uniq w1 w2 j h1 h2
  · intro j hj
    show s.list.dropLast[j]? = some (s.cell j) ∨
      ∃ w y, ((s.at u (.live i s.list.getLast?)) w).call = .set j y ∧
        ((s.at u (.live i s.list.getLast?)) w).phase = .live j none
    have hj1 : j < i := hj
    rw [dropLast_get _ _ (by omega)]
    rcases hs.cells j (by omega) with h | ⟨w, y, hcw, hpw⟩
    · exact Or.inl h
    · have hwu : w ≠ u := fun he => by subst he; rw [hc] at hcw; cases hcw
      exact Or.inr ⟨w, y, by rw [at_other _ _ _ _ hwu]; exact hcw, by rw [at_other _ _ _ _ hwu]; exact hpw⟩
  · intro w
    by_cases hwu : w = u
    · subst hwu
      show Good cap _ w (s.at w (.live i s.list.getLast?) w)
      rw [at_same, hc]
      refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
      · intro n' j hn' _
        rcases hn' with h | ⟨y, h⟩ <;> cases h
      · intro n' y j hc' _; cases hc'
      · intro n' j r hc' _; cases hc'
      · intro y j hc' _; cases hc'
      · intro y j hc' _; cases hc'
      · intro y k hc' _; cases hc'
      · intro j r _ hp'
        rcases hp' with hp' | hp' <;> cases hp'
        exact Nat.le_refl _
      · intro j r _ hp'
        cases hp'
        rw [hlast, hcelli]
      · intro k _ hp'; cases hp'
      · intro o ho
        simp [entryOf] at ho
        subst ho
        simp
    · show Good cap _ w (s.at u (.live i s.list.getLast?) w)
      rw [at_other _ _ _ _ hwu]
      exact good_shrink cap s _ w i _ (hother w hwu) hlen hsz rfl rfl rfl
        (fun o h => List.mem_append_left _ h) (hs.good w)

/-! ### The thread at each step -/

omit [DecidableEq α] in
theorem good_idle (s : State α) (u : Nat) (c : Call α) : Good cap s u ⟨c, .idle⟩ := by
  cases c <;> refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;> intros <;> simp_all [Phase.holds, entryOf]

omit [DecidableEq α] in
theorem good_sized (s : State α) (u : Nat) (c : Call α) (k : Nat) (hcap : ∀ x, c = .push x → k < cap)
    (hpos : c = .pop → 0 < k) : Good cap s u ⟨c, .sized k⟩ := by
  cases c <;> refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;> intros <;> simp_all [Phase.holds, entryOf]

omit [DecidableEq α] in
theorem good_held (s : State α) (u : Nat) (c : Call α) (i : Nat) (hidx : ∀ n, (c = .get n ∨ ∃ x, c = .set n x) → i = n)
    (hcap : ∀ x, c = .push x → i < cap) : Good cap s u ⟨c, .held i⟩ := by
  cases c <;> refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;> intros <;> simp_all [Phase.holds, entryOf]

omit [DecidableEq α] in
theorem good_backoff (s : State α) (u : Nat) (c : Call α) (i : Nat)
    (hidx : ∀ n, (c = .get n ∨ ∃ x, c = .set n x) → i = n) (hcap : ∀ x, c = .push x → i < cap) :
    Good cap s u ⟨c, .backoff i⟩ := by
  cases c <;> refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;> intros <;> simp_all [Phase.holds, entryOf]

omit [DecidableEq α] in
theorem good_valid (s : State α) (u : Nat) (x : α) (i : Nat) (htop : s.size ≤ i) (hcap : i < cap) :
    Good cap s u ⟨.push x, .valid i⟩ := by
  refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;> intros <;> simp_all [Phase.holds, entryOf]

omit [DecidableEq α] in
theorem good_done (s : State α) (u : Nat) (c : Call α) (a : Answer α)
    (hlog : ∀ o, entryOf u ⟨c, .done a⟩ = some o → o ∈ s.log) : Good cap s u ⟨c, .done a⟩ := by
  refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, hlog⟩ <;> intros <;> simp_all [Phase.holds]

omit [DecidableEq α] in
theorem good_refusing (s : State α) (u : Nat) (c : Call α) (i : Nat)
    (hidx : ∀ n, (c = .get n ∨ ∃ x, c = .set n x) → i = n) (hcap : ∀ x, c = .push x → i < cap)
    (hlog : ∀ o, entryOf u ⟨c, .refusing i⟩ = some o → o ∈ s.log) : Good cap s u ⟨c, .refusing i⟩ := by
  refine ⟨fun n j hn hj => ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, hlog⟩
  · simp only [Phase.holds, Option.some.injEq] at hj
    subst hj
    exact hidx n hn
  all_goals (intros; simp_all [Phase.holds])

omit [DecidableEq α] in
theorem good_live_get (s : State α) (u n : Nat) (r : Option α) (hn : n < s.size) (hr : s.list[n]? = r)
    (hlog : LinOp.get u n r ∈ s.log) : Good cap s u ⟨.get n, .live n r⟩ := by
  refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;> intros <;> simp_all [Phase.holds, entryOf]

omit [DecidableEq α] in
theorem good_worked (s : State α) (u : Nat) (c : Call α) (i : Nat) (r : Option α)
    (hidx : ∀ n, (c = .get n ∨ ∃ x, c = .set n x) → i = n) (hcap : ∀ x, c = .push x → i < cap)
    (htop : c = .pop → s.size ≤ i) (hlog : ∀ o, entryOf u ⟨c, .worked i r⟩ = some o → o ∈ s.log) :
    Good cap s u ⟨c, .worked i r⟩ := by
  refine ⟨fun n j hn hj => ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, hlog⟩
  · simp only [Phase.holds, Option.some.injEq] at hj
    subst hj
    exact hidx n hn
  all_goals (intros; simp_all [Phase.holds])

omit [DecidableEq α] in
theorem idx_get (n i : Nat) (hi : i = n) : ∀ n', ((Call.get n : Call α) = .get n' ∨ ∃ x, (Call.get n : Call α) = .set n' x) → i = n' := by
  intro n' h
  rcases h with h | ⟨x, h⟩
  · cases h; exact hi
  · cases h

omit [DecidableEq α] in
theorem idx_set (n i : Nat) (x : α) (hi : i = n) : ∀ n', ((Call.set n x : Call α) = .get n' ∨ ∃ y, (Call.set n x : Call α) = .set n' y) → i = n' := by
  intro n' h
  rcases h with h | ⟨y, h⟩
  · cases h
  · cases h; exact hi

omit [DecidableEq α] in
theorem idx_other (c : Call α) (i : Nat) (hc : ∀ n, c ≠ .get n) (hc' : ∀ n x, c ≠ .set n x) :
    ∀ n', (c = .get n' ∨ ∃ y, c = .set n' y) → i = n' := by
  intro n' h
  rcases h with h | ⟨y, h⟩
  · exact absurd h (hc n')
  · exact absurd h (hc' n' y)

/-- A step that changes nothing keeps the invariant. -/
theorem inv_same (s : State α) (hs : Inv cap s) (s' : State α) (h : s' = s) : Inv cap s' := by rw [h]; exact hs

/-- Each step keeps the invariant. -/
theorem step_inv (s : State α) (hs : Inv cap s) (u : Nat) : Inv cap (step cap s u) := by
  have hg := hs.good u
  have hlen := hs.len
  cases hc : (s.threads u).call with
  | get n =>
    have nset : ∀ n' x i, (s.threads u).call = .set n' x → (s.threads u).phase ≠ .live i none := by
      intro n' x i h; rw [hc] at h; cases h
    cases hp : (s.threads u).phase with
    | idle =>
      by_cases hb : s.bits n = true
      · exact inv_same cap s hs _ (by simp [step, hc, hp, hb])
      · rw [show step cap s u = { s with bits := update s.bits n true, threads := s.at u (.held n) } by
          simp [step, hc, hp, hb]]
        refine inv_claim cap s hs u n _ (by simpa using hb) (by rw [hp]; rfl) rfl nset ?_
        rw [hc]
        exact good_held cap s u _ n (idx_get n n rfl) (fun _ h => by cases h)
    | held i =>
      have hin : i = n := hg.idx n i (Or.inl hc) (by rw [hp]; rfl)
      by_cases hn : n < s.size
      · rw [show step cap s u = { s with log := s.log ++ [.get u n s.list[n]?], threads := s.at u (.live n s.list[n]?) } by simp [step, hc, hp, hn]]
        refine inv_logged cap s hs u _ _ (by rw [hp]; simp [Phase.holds, hin]) nset (by simp [seqStep]) ?_
        rw [hc]
        exact good_live_get cap _ u n _ hn rfl (by simp)
      · rw [show step cap s u = { s with log := s.log ++ [.get u n none], threads := s.at u (.refusing n) } by
          simp [step, hc, hp, hn]]
        have hnone : s.list[n]? = none := List.getElem?_eq_none (by omega)
        refine inv_logged cap s hs u _ _ (by rw [hp]; simp [Phase.holds, hin]) nset (by simp [seqStep, hnone]) ?_
        rw [hc]
        exact good_refusing cap _ u _ n (idx_get n n rfl) (fun _ h => by cases h)
          (fun o ho => by simp [entryOf] at ho; subst ho; simp)
    | live i r =>
      have hin : i = n := hg.idx n i (Or.inl hc) (by rw [hp]; rfl)
      subst hin
      obtain ⟨hi, hr⟩ := hg.getLive i i r hc hp
      have hcell : s.list[i]? = some (s.cell i) := by
        rcases hs.cells i hi with h | ⟨w, y, hcw, hpw⟩
        · exact h
        · have hwu : w = u := hs.uniq w u i (by rw [hpw]; rfl) (by rw [hp]; rfl)
          subst hwu; rw [hc] at hcw; cases hcw
      rw [show step cap s u = { s with threads := s.at u (.worked i (some (s.cell i))) } by simp [step, hc, hp]]
      refine inv_phase cap s hs u _ (by rw [hp]; rfl) nset ?_
      rw [hc]
      refine good_worked cap s u _ i _ (idx_get i i rfl) (fun _ h => by cases h) (fun h => by cases h) ?_
      intro o ho
      simp [entryOf] at ho
      subst ho
      have := hg.logged (.get u i r) (by simp [entryOf, hc, hp])
      rw [← hr, hcell] at this
      exact this
    | worked i r =>
      have hin : i = n := hg.idx n i (Or.inl hc) (by rw [hp]; rfl)
      subst hin
      rw [show step cap s u = { s with bits := update s.bits i false, threads := s.at u (.done (.value r)) } by
        simp [step, hc, hp]]
      refine inv_release cap s hs u i _ (by rw [hp]; rfl) rfl nset ?_
      rw [hc]
      exact good_done cap s u _ _ (fun o ho => by
        simp [entryOf] at ho; subst ho; exact hg.logged _ (by simp [entryOf, hc, hp]))
    | refusing i =>
      have hin : i = n := hg.idx n i (Or.inl hc) (by rw [hp]; rfl)
      subst hin
      rw [show step cap s u = { s with bits := update s.bits i false, threads := s.at u (.done (.value none)) } by
        simp [step, hc, hp]]
      refine inv_release cap s hs u i _ (by rw [hp]; rfl) rfl nset ?_
      rw [hc]
      exact good_done cap s u _ _ (fun o ho => by
        simp [entryOf] at ho; subst ho; exact hg.logged _ (by simp [entryOf, hc, hp]))
    | sized k => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | valid i => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | wrote i => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | backoff i => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | done a => exact inv_same cap s hs _ (by simp [step, hc, hp])
  | set n x =>
    cases hp : (s.threads u).phase with
    | idle =>
      have nlive : ∀ n' y i, (s.threads u).call = .set n' y → (s.threads u).phase ≠ .live i none := by
        intro _ _ _ _; rw [hp]; intro h; cases h
      by_cases hb : s.bits n = true
      · exact inv_same cap s hs _ (by simp [step, hc, hp, hb])
      · rw [show step cap s u = { s with bits := update s.bits n true, threads := s.at u (.held n) } by
          simp [step, hc, hp, hb]]
        refine inv_claim cap s hs u n _ (by simpa using hb) (by rw [hp]; rfl) rfl nlive ?_
        rw [hc]
        exact good_held cap s u _ n (idx_set n n x rfl) (fun _ h => by cases h)
    | held i =>
      have nlive : ∀ n' y i, (s.threads u).call = .set n' y → (s.threads u).phase ≠ .live i none := by
        intro _ _ _ _; rw [hp]; intro h; cases h
      have hin : i = n := hg.idx n i (Or.inr ⟨x, hc⟩) (by rw [hp]; rfl)
      by_cases hn : n < s.size
      · rw [show step cap s u = { s with list := s.list.set n x, log := s.log ++ [.set u n x true], threads := s.at u (.live n none) } by simp [step, hc, hp, hn]]
        exact inv_setPoint cap s hs u n x i hc hp hn
      · rw [show step cap s u = { s with log := s.log ++ [.set u n x false], threads := s.at u (.refusing n) } by
          simp [step, hc, hp, hn]]
        refine inv_logged cap s hs u _ _ (by rw [hp]; simp [Phase.holds, hin]) nlive
          (by simp [seqStep, hlen]; omega) ?_
        rw [hc]
        exact good_refusing cap _ u _ n (idx_set n n x rfl) (fun _ h => by cases h)
          (fun o ho => by simp [entryOf] at ho; subst ho; simp)
    | live i r =>
      cases r with
      | none =>
        have hin : i = n := hg.idx n i (Or.inr ⟨x, hc⟩) (by rw [hp]; rfl)
        rw [show step cap s u = { s with cell := update s.cell n x, threads := s.at u (.worked n none) } by
          simp [step, hc, hp]]
        exact inv_setWrite cap s hs u n x i hc hp
      | some v => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | worked i r =>
      have nlive : ∀ n' y i, (s.threads u).call = .set n' y → (s.threads u).phase ≠ .live i none := by
        intro _ _ _ _; rw [hp]; intro h; cases h
      have hin : i = n := hg.idx n i (Or.inr ⟨x, hc⟩) (by rw [hp]; rfl)
      subst hin
      rw [show step cap s u = { s with bits := update s.bits i false, threads := s.at u (.done (.ok true)) } by
        simp [step, hc, hp]]
      refine inv_release cap s hs u i _ (by rw [hp]; rfl) rfl nlive ?_
      rw [hc]
      exact good_done cap s u _ _ (fun o ho => by
        simp [entryOf] at ho; subst ho; exact hg.logged _ (by simp [entryOf, hc, hp]))
    | refusing i =>
      have nlive : ∀ n' y i, (s.threads u).call = .set n' y → (s.threads u).phase ≠ .live i none := by
        intro _ _ _ _; rw [hp]; intro h; cases h
      have hin : i = n := hg.idx n i (Or.inr ⟨x, hc⟩) (by rw [hp]; rfl)
      subst hin
      rw [show step cap s u = { s with bits := update s.bits i false, threads := s.at u (.done (.ok false)) } by
        simp [step, hc, hp]]
      refine inv_release cap s hs u i _ (by rw [hp]; rfl) rfl nlive ?_
      rw [hc]
      exact good_done cap s u _ _ (fun o ho => by
        simp [entryOf] at ho; subst ho; exact hg.logged _ (by simp [entryOf, hc, hp]))
    | sized k => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | valid i => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | wrote i => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | backoff i => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | done a => exact inv_same cap s hs _ (by simp [step, hc, hp])
  | push x =>
    have nset : ∀ n' y i, (s.threads u).call = .set n' y → (s.threads u).phase ≠ .live i none := by
      intro n' y i h; rw [hc] at h; cases h
    have nidx : ∀ i, ∀ n', ((Call.push x : Call α) = .get n' ∨ ∃ y, (Call.push x : Call α) = .set n' y) → i = n' :=
      fun i => idx_other (Call.push x : Call α) i (fun _ h => by cases h) (fun _ _ h => by cases h)
    cases hp : (s.threads u).phase with
    | idle =>
      by_cases hn : s.size < cap
      · rw [show step cap s u = { s with threads := s.at u (.sized s.size) } by simp [step, hc, hp, hn]]
        refine inv_phase cap s hs u _ (by rw [hp]; rfl) nset ?_
        rw [hc]
        exact good_sized cap s u _ _ (fun _ _ => hn) (fun h => by cases h)
      · rw [show step cap s u = { s with log := s.log ++ [.push u x false], threads := s.at u (.done (.ok false)) } by
          simp [step, hc, hp, hn]]
        refine inv_logged cap s hs u _ _ (by rw [hp]; rfl) nset (by simp [seqStep, hlen]; omega) ?_
        rw [hc]
        exact good_done cap _ u _ _ (fun o ho => by simp [entryOf] at ho; subst ho; simp)
    | sized k =>
      have hk := hg.pushCap x k hc (Or.inl hp)
      by_cases hb : s.bits k = true
      · rw [show step cap s u = { s with threads := s.at u .idle } by simp [step, hc, hp, hb]]
        refine inv_phase cap s hs u _ (by rw [hp]; rfl) nset ?_
        exact good_idle cap s u _
      · rw [show step cap s u = { s with bits := update s.bits k true, threads := s.at u (.held k) } by
          simp [step, hc, hp, hb]]
        refine inv_claim cap s hs u k _ (by simpa using hb) (by rw [hp]; rfl) rfl nset ?_
        rw [hc]
        exact good_held cap s u _ k (nidx k) (fun _ _ => hk)
    | held i =>
      have hi := hg.pushCap x i hc (Or.inr (by rw [hp]; rfl))
      by_cases hsz : s.size = i
      · rw [show step cap s u = { s with threads := s.at u (.valid i) } by simp [step, hc, hp, hsz]]
        refine inv_phase cap s hs u _ (by rw [hp]; rfl) nset ?_
        rw [hc]
        exact good_valid cap s u x i (by omega) hi
      · rw [show step cap s u = { s with threads := s.at u (.backoff i) } by simp [step, hc, hp, hsz]]
        refine inv_phase cap s hs u _ (by rw [hp]; rfl) nset ?_
        rw [hc]
        exact good_backoff cap s u _ i (nidx i) (fun _ _ => hi)
    | valid i =>
      rw [show step cap s u = { s with cell := update s.cell i x, threads := s.at u (.wrote i) } by
        simp [step, hc, hp]]
      exact inv_pushWrite cap s hs u x i hc hp
    | wrote i =>
      have hi := hg.pushCap x i hc (Or.inr (by rw [hp]; rfl))
      by_cases hsz : s.size = i
      · rw [show step cap s u = { s with size := i + 1, list := s.list ++ [x], log := s.log ++ [.push u x true], threads := s.at u (.worked i none) } by simp [step, hc, hp, hsz]]
        exact inv_pushCommit cap s hs u x i hc hp hsz
      · rw [show step cap s u = { s with threads := s.at u (.backoff i) } by simp [step, hc, hp, hsz]]
        refine inv_phase cap s hs u _ (by rw [hp]; rfl) nset ?_
        rw [hc]
        exact good_backoff cap s u _ i (nidx i) (fun _ _ => hi)
    | worked i r =>
      rw [show step cap s u = { s with bits := update s.bits i false, threads := s.at u (.done (.ok true)) } by
        simp [step, hc, hp]]
      refine inv_release cap s hs u i _ (by rw [hp]; rfl) rfl nset ?_
      rw [hc]
      exact good_done cap s u _ _ (fun o ho => by
        simp [entryOf] at ho; subst ho; exact hg.logged _ (by simp [entryOf, hc, hp]))
    | backoff i =>
      rw [show step cap s u = { s with bits := update s.bits i false, threads := s.at u .idle } by
        simp [step, hc, hp]]
      exact inv_release cap s hs u i _ (by rw [hp]; rfl) rfl nset (good_idle cap s u _)
    | live i r => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | refusing i => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | done a => exact inv_same cap s hs _ (by simp [step, hc, hp])
  | pop =>
    have nset : ∀ n' y i, (s.threads u).call = .set n' y → (s.threads u).phase ≠ .live i none := by
      intro n' y i h; rw [hc] at h; cases h
    have nidx : ∀ i, ∀ n', ((Call.pop : Call α) = .get n' ∨ ∃ y, (Call.pop : Call α) = .set n' y) → i = n' :=
      fun i => idx_other (Call.pop : Call α) i (fun _ h => by cases h) (fun _ _ h => by cases h)
    cases hp : (s.threads u).phase with
    | idle =>
      by_cases hz : s.size = 0
      · rw [show step cap s u = { s with log := s.log ++ [.pop u none], threads := s.at u (.done (.value none)) } by
          simp [step, hc, hp, hz]]
        have hnil : s.list = [] := List.eq_nil_of_length_eq_zero (by omega)
        refine inv_logged cap s hs u _ _ (by rw [hp]; rfl) nset (by simp [seqStep, hnil]) ?_
        rw [hc]
        exact good_done cap _ u _ _ (fun o ho => by simp [entryOf] at ho; subst ho; simp)
      · rw [show step cap s u = { s with threads := s.at u (.sized s.size) } by simp [step, hc, hp, hz]]
        refine inv_phase cap s hs u _ (by rw [hp]; rfl) nset ?_
        rw [hc]
        exact good_sized cap s u _ _ (fun _ h => by cases h) (fun _ => by omega)
    | sized k =>
      by_cases hb : s.bits (k - 1) = true
      · rw [show step cap s u = { s with threads := s.at u .idle } by simp [step, hc, hp, hb]]
        exact inv_phase cap s hs u _ (by rw [hp]; rfl) nset (good_idle cap s u _)
      · rw [show step cap s u = { s with bits := update s.bits (k - 1) true, threads := s.at u (.held (k - 1)) } by
          simp [step, hc, hp, hb]]
        refine inv_claim cap s hs u (k - 1) _ (by simpa using hb) (by rw [hp]; rfl) rfl nset ?_
        rw [hc]
        exact good_held cap s u _ _ (nidx _) (fun _ h => by cases h)
    | held i =>
      by_cases hsz : s.size = i + 1
      · rw [show step cap s u = { s with size := i, list := s.list.dropLast, log := s.log ++ [.pop u s.list.getLast?], threads := s.at u (.live i s.list.getLast?) } by simp [step, hc, hp, hsz]]
        exact inv_popCommit cap s hs u i hc hp hsz
      · rw [show step cap s u = { s with threads := s.at u (.backoff i) } by simp [step, hc, hp, hsz]]
        refine inv_phase cap s hs u _ (by rw [hp]; rfl) nset ?_
        rw [hc]
        exact good_backoff cap s u _ i (nidx i) (fun _ h => by cases h)
    | live i r =>
      have htop := hg.popTop i r hc (Or.inl hp)
      have hr := hg.popCell i r hc hp
      rw [show step cap s u = { s with threads := s.at u (.worked i (some (s.cell i))) } by simp [step, hc, hp]]
      refine inv_phase cap s hs u _ (by rw [hp]; rfl) nset ?_
      rw [hc]
      refine good_worked cap s u _ i _ (nidx i) (fun _ h => by cases h) (fun _ => htop) ?_
      intro o ho
      simp [entryOf] at ho
      subst ho
      have := hg.logged (.pop u r) (by simp [entryOf, hc, hp])
      rw [hr] at this
      exact this
    | worked i r =>
      rw [show step cap s u = { s with bits := update s.bits i false, threads := s.at u (.done (.value r)) } by
        simp [step, hc, hp]]
      refine inv_release cap s hs u i _ (by rw [hp]; rfl) rfl nset ?_
      rw [hc]
      exact good_done cap s u _ _ (fun o ho => by
        simp [entryOf] at ho; subst ho; exact hg.logged _ (by simp [entryOf, hc, hp]))
    | backoff i =>
      rw [show step cap s u = { s with bits := update s.bits i false, threads := s.at u .idle } by
        simp [step, hc, hp]]
      exact inv_release cap s hs u i _ (by rw [hp]; rfl) rfl nset (good_idle cap s u _)
    | valid i => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | wrote i => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | refusing i => exact inv_same cap s hs _ (by simp [step, hc, hp])
    | done a => exact inv_same cap s hs _ (by simp [step, hc, hp])

theorem run_inv (s : State α) (hs : Inv cap s) (sched : List Nat) : Inv cap (run cap s sched) := by
  induction sched generalizing s with
  | nil => exact hs
  | cons u rest ih => exact ih _ (step_inv cap s hs u)

end Steps

/-! ## What the vector gives -/

section Results

variable [DecidableEq α] (cap : Nat)

/-- L1: the log is a legal history of the vector with one lock, for each count of threads and each schedule. -/
theorem legal (cells : Nat → α) (calls : Nat → Call α) (sched : List Nat) :
    seqRun cap (run cap (start cells calls) sched).log = some (run cap (start cells calls) sched).list :=
  (run_inv cap _ (start_inv cap cells calls) sched).legal

/-- A call that ended gave the answer of its point in the log. -/
theorem answer_logged (s : State α) (hs : Inv cap s) (u : Nat) (a : Answer α)
    (hp : (s.threads u).phase = .done a) :
    ∀ o, entryOf u ⟨(s.threads u).call, .done a⟩ = some o → o ∈ s.log := by
  intro o ho
  have heq : s.threads u = ⟨(s.threads u).call, .done a⟩ := by rw [← hp]
  rw [← heq] at ho
  exact (hs.good u).logged o ho

omit [DecidableEq α] in
/-- A run only appends to the log. -/
theorem run_log_prefix (s : State α) (sched : List Nat) : ∃ more, (run cap s sched).log = s.log ++ more := by
  induction sched generalizing s with
  | nil => exact ⟨[], by simp [run]⟩
  | cons u rest ih =>
    obtain ⟨more, hm⟩ := ih (step cap s u)
    have hstep : ∃ d, (step cap s u).log = s.log ++ d := by
      unfold step
      split <;> (try split) <;> (try split) <;> first
        | exact ⟨_, rfl⟩
        | exact ⟨[], (List.append_nil _).symm⟩
    obtain ⟨d, hd⟩ := hstep
    refine ⟨d ++ more, ?_⟩
    show (run cap (step cap s u) rest).log = _
    rw [hm, hd, List.append_assoc]

omit [DecidableEq α] in
/-- L2: a call that ended before another took effect comes first in the log. -/
theorem realtime (s : State α) (sched : List Nat) (a b : LinOp α) (ha : a ∈ s.log) (hb : b ∉ s.log) :
    ∃ pre post, (run cap s sched).log = pre ++ post ∧ a ∈ pre ∧ b ∉ pre := by
  obtain ⟨more, hm⟩ := run_log_prefix cap s sched
  exact ⟨s.log, more, hm, ha, hb⟩

/-- The test of `at` and `set` is the span test of `vec[n]` in `Guard.lean`. Take a stride above 0, and the bytes of
the count below the values of the word. Then it passes exactly the indices below the count. -/
theorem legal_is_span (Wd stride size n : Nat) (hs : 0 < stride) (hW : size * stride < Wd) :
    decide (n < size) = Guard.legal Wd stride size n := by
  cases h : Guard.legal Wd stride size n
  · have : ¬ n < size := fun hn => by rw [(Guard.legal_iff Wd stride size n hs hW).2 hn] at h; cases h
    simp [this]
  · simp [(Guard.legal_iff Wd stride size n hs hW).1 h]

end Results

/-! ## Runs on many threads -/

/-- The answer of a thread as a number: the value, or 0 for none, 1 for a commit and 2 for a refusal. -/
def answerCode (s : State Nat) (u : Nat) : Nat :=
  match (s.threads u).phase with
  | .done (.value (some v)) => v
  | .done (.value none) => 0
  | .done (.ok true) => 1
  | .done (.ok false) => 2
  | _ => 99

/-- Two pushes of `5` and `6` on two threads, a read of index 1, and a pop. -/
def fourCalls : Nat → Call Nat := fun u => if u = 0 then .push 5 else if u = 1 then .push 6 else if u = 2 then .get 1 else .pop

/-- The two pushes run step by step at once. Both commit, one of the two backs off and retries, and the list ends as
`[5, 6]`. The log is legal. -/
theorem pushes_at_once :
    (run 4 (start (fun _ => 0) fourCalls) [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]).list = [5, 6] ∧
      answerCode (run 4 (start (fun _ => 0) fourCalls) [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]) 0 = 1 ∧
      answerCode (run 4 (start (fun _ => 0) fourCalls) [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]) 1 = 1 := by
  decide

/-- A push of `5`, then a read of index 0 and a pop at once. The read holds the bit of element 0 first, so the pop
waits for it. Both give `5`, and the list ends empty. -/
def pushReadPop : Nat → Call Nat := fun u => if u = 0 then .push 5 else if u = 1 then .get 0 else .pop

theorem read_then_pop :
    answerCode (run 4 (start (fun _ => 0) pushReadPop) [0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2]) 1 = 5 ∧
      answerCode (run 4 (start (fun _ => 0) pushReadPop) [0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2]) 2 = 5 ∧
      (run 4 (start (fun _ => 0) pushReadPop) [0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2]).list = [] := by
  decide

/-- The vector refuses a push at the ceiling of 1. -/
def twoPushes : Nat → Call Nat := fun u => if u = 0 then .push 5 else .push 6

theorem push_at_ceiling :
    answerCode (run 1 (start (fun _ => 0) twoPushes) [0, 0, 0, 0, 0, 0, 1]) 1 = 2 := by
  decide

end LocusCarcerum.ConcurrentVector
