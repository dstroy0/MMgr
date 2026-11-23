/-!
# LocusCarcerum, the MPMC queue of Folly, proved linearizable

A port to Lean 4 of the proof in Vindum, Frumin and Birkedal, `Mechanized Verification of a Fine-Grained Concurrent
Queue from Meta's Folly Library`, CPP 2022, pages 100 to 115, `doi:10.1145/3497775.3503689`. The section numbers below
are those of the same text as Chapter 3 of Vindum, `Separation Logic for Concurrency and Persistency`, PhD
dissertation, Aarhus University, 2023. The Coq proof is the artifact `doi:10.5281/zenodo.5770778`, and
`theories/examples/folly_queue` of the ReLoC repository at commit `b6df47f9`. The queue is `folly/MPMCQueue.h` of
`https://github.com/facebook/folly`.

The source proves in Coq, with ReLoC and Iris, that the queue refines a queue with one lock, and so that it is
linearizable. Lean has no ReLoC. This file keeps the algorithm, the places of the linearization points and the
invariant of the source. It proves the conditions L1 and L2 of Herlihy and Wing over an operational model, as
`Linearizable.lean` does. The model is the one of the source, Section 3.2.4: sequentially consistent, with each
counter a natural number that does not roll.

* The algorithm, Section 3.2 and Figure 3.1. Two ticket counters, `pushTicket` and `popTicket`, each taken with a FAA.
  A slot of `q` slots for each ticket `t`, at `t mod q`, guarded by a turn sequencer. An enqueue waits for the turn
  `2 * (t / q)`, writes its value and completes the turn. A dequeue waits for the turn `2 * (t / q) + 1`, reads the
  value and completes the turn.

* The linearization points, Section 3.3. An enqueue takes effect at its FAA. A dequeue takes effect at its FAA where
  its enqueue took its FAA first. Where the dequeue came first, it takes effect in the enqueue, right after the FAA of
  that enqueue: the external linearization point. `step` puts each point in the log at that place.

* The invariant, Section 3.6 and Figure 3.6. The ghost list holds each value that an enqueue took a ticket for, so its
  length is `pushTicket`. The queue of the lock is that list with its first `popTicket` values dropped. Each slot at an
  odd turn holds the value of the ghost list at its ticket.

The theorems.

* `fifo`: the log is a legal history of the queue with one lock, its state the ghost list with `min popTicket
  pushTicket` values dropped. This is L1.

* `dequeue_value`: a dequeue gives the value of the enqueue of its ticket, and that value is its entry in the log.

* `read_finds_value`: a dequeue that waited for its turn finds a value, so the `assert(false)` of the source never runs.

* `slot_exclusive`: no two threads hold one slot at the same time, which the turn sequencer gives.

* `external_point`: an enqueue whose dequeue came first puts the point of that dequeue in the log after its own.

* `realtime`: a call that ended before another took effect comes first in the log. This is L2.
-/

namespace LocusCarcerum.MPMCQueue

/-- A function with the value at one point changed. -/
def update {β : Type} (f : Nat → β) (k : Nat) (v : β) : Nat → β := fun j => if j = k then v else f j

@[simp] theorem update_same {β : Type} (f : Nat → β) (k : Nat) (v : β) : update f k v k = v := by simp [update]

@[simp] theorem update_other {β : Type} (f : Nat → β) (k j : Nat) (v : β) (h : j ≠ k) : update f k v j = f j := by
  simp [update, h]

variable {α : Type}

/-- The call of a thread: an enqueue of a value, or a dequeue. -/
inductive Kind (α : Type) where
  | enq (v : α)
  | deq

def Kind.isEnq : Kind α → Bool
  | .enq _ => true
  | .deq => false

/-- The step that a thread is at. It is before its FAA, or it has its ticket, or it waited for its turn. Then it wrote
or read, and then it completed its turn. -/
inductive Phase (α : Type) where
  | idle
  | ticket (t : Nat)
  | waited (t : Nat)
  | wrote (t : Nat)
  | read (t : Nat) (v : α)
  | done (t : Nat) (r : Option α)

/-- The ticket of a thread after its FAA. -/
def Phase.ticketOf : Phase α → Option Nat
  | .idle => none
  | .ticket t => some t
  | .waited t => some t
  | .wrote t => some t
  | .read t _ => some t
  | .done t _ => some t

/-- The thread holds its slot: it waited for its turn and has not completed it. -/
def Phase.inside : Phase α → Option Nat
  | .waited t => some t
  | .wrote t => some t
  | .read t _ => some t
  | _ => none

structure Thread (α : Type) where
  kind : Kind α
  phase : Phase α

/-- A point of the log: the enqueue or the dequeue of a ticket, with its value. -/
inductive LinOp (α : Type) where
  | enq (t : Nat) (x : α)
  | deq (t : Nat) (x : α)

/-- The state: the two ticket counters, and the turn and the value of each slot. It also holds the threads, the ghost
list, and the log of the linearization points. -/
structure State (α : Type) where
  push : Nat
  pop : Nat
  turn : Nat → Nat
  val : Nat → Option α
  threads : Nat → Thread α
  ghost : List α
  log : List (LinOp α)

/-- The points that the FAA of an enqueue puts in the log. They are its own point, and the point of a dequeue of its
ticket that took its FAA first. -/
def enqPoints (s : State α) (v : α) : List (LinOp α) :=
  [.enq s.push v] ++ (if s.push < s.pop then [.deq s.push v] else [])

/-- The point that the FAA of a dequeue puts in the log: its own, where the enqueue of its ticket took its FAA first. -/
def deqPoints (s : State α) : List (LinOp α) :=
  if s.pop < s.push then
    match s.ghost[s.pop]? with
    | some x => [.deq s.pop x]
    | none => []
  else []

/-- One step of thread `u`, with `q` slots. -/
def step (q : Nat) (s : State α) (u : Nat) : State α :=
  match (s.threads u).kind, (s.threads u).phase with
  | .enq v, .idle =>
    { s with
      push := s.push + 1
      ghost := s.ghost ++ [v]
      log := s.log ++ enqPoints s v
      threads := update s.threads u ⟨.enq v, .ticket s.push⟩ }
  | .deq, .idle =>
    { s with
      pop := s.pop + 1
      log := s.log ++ deqPoints s
      threads := update s.threads u ⟨.deq, .ticket s.pop⟩ }
  | .enq v, .ticket t =>
    if s.turn (t % q) = 2 * (t / q) then { s with threads := update s.threads u ⟨.enq v, .waited t⟩ } else s
  | .deq, .ticket t =>
    if s.turn (t % q) = 2 * (t / q) + 1 then { s with threads := update s.threads u ⟨.deq, .waited t⟩ } else s
  | .enq v, .waited t =>
    { s with val := update s.val (t % q) (some v), threads := update s.threads u ⟨.enq v, .wrote t⟩ }
  | .deq, .waited t =>
    match s.val (t % q) with
    | some v => { s with threads := update s.threads u ⟨.deq, .read t v⟩ }
    | none => s
  | .enq v, .wrote t =>
    { s with
      turn := update s.turn (t % q) (2 * (t / q) + 1)
      threads := update s.threads u ⟨.enq v, .done t none⟩ }
  | .deq, .read t v =>
    { s with
      turn := update s.turn (t % q) (2 * (t / q) + 2)
      threads := update s.threads u ⟨.deq, .done t (some v)⟩ }
  | _, _ => s

/-- A schedule run from a state. -/
def run (q : Nat) (s : State α) (sched : List Nat) : State α := sched.foldl (step q) s

/-- The start: both counters at 0, each turn at 0, no value, each thread before its FAA. -/
def start (calls : Nat → Kind α) : State α :=
  ⟨0, 0, fun _ => 0, fun _ => none, fun u => ⟨calls u, .idle⟩, [], []⟩

/-! ## The queue with one lock, Figure 3.1d -/

section Spec

variable [DecidableEq α]

/-- One step of the queue with one lock. An enqueue appends its value. A dequeue removes the head, which must be its
value. A dequeue of the empty queue has no step, as `assert(false)` of `dequeueCG`. -/
def seqStep : Option (List α) → LinOp α → Option (List α)
  | none, _ => none
  | some xs, .enq _ x => some (xs ++ [x])
  | some [], .deq _ _ => none
  | some (y :: rest), .deq _ x => if y = x then some rest else none

/-- The log run on the queue with one lock, from the empty queue. -/
def seqRun (log : List (LinOp α)) : Option (List α) := log.foldl seqStep (some [])

theorem seqRun_append (l m : List (LinOp α)) : seqRun (l ++ m) = m.foldl seqStep (seqRun l) := by
  simp [seqRun, List.foldl_append]

end Spec

/-! ## The invariant, Figure 3.6 -/

/-- The turn that the ticket of a thread waits for: even for an enqueue, odd for a dequeue. -/
def want (q : Nat) (k : Kind α) (t : Nat) : Nat := 2 * (t / q) + (if k.isEnq then 0 else 1)

/-- The invariant.

* The ghost list has one value for each ticket of `pushTicket`. The log runs on the queue with one lock to that list,
  with its first `popTicket` values dropped. Each point of the log has the value of its ticket. This is
  `length m = pushTicket` and `drop popTicket m = xsᵢ` of `mpmcInv` in `refinement.v`.

* Each enqueue ticket below `pushTicket` has its point, and each dequeue ticket below `min popTicket pushTicket` has
  its point.

* A thread with a ticket of an enqueue has its value in the ghost list. A dequeue ticket is below `popTicket`. No two
  threads of one kind have one ticket.

* A thread that holds its slot has the turn of the slot at its own turn. An enqueue after its write has its value in
  its slot. A slot at an odd turn holds the value of its ticket.

* A dequeue after its read has the value of its ticket. A dequeue that waited has a ticket below `pushTicket`. -/
structure Inv [DecidableEq α] (q : Nat) (s : State α) : Prop where
  len : s.ghost.length = s.push
  fifo : seqRun s.log = some (s.ghost.drop s.pop)
  logEnq : ∀ t x, LinOp.enq t x ∈ s.log → s.ghost[t]? = some x
  logDeq : ∀ t x, LinOp.deq t x ∈ s.log → s.ghost[t]? = some x
  memEnq : ∀ t, (∃ x, LinOp.enq t x ∈ s.log) ↔ t < s.push
  memDeq : ∀ t, (∃ x, LinOp.deq t x ∈ s.log) ↔ t < min s.pop s.push
  enqT : ∀ u v t, (s.threads u).kind = .enq v → (s.threads u).phase.ticketOf = some t → s.ghost[t]? = some v
  deqT : ∀ u t, (s.threads u).kind = .deq → (s.threads u).phase.ticketOf = some t → t < s.pop
  uniq : ∀ u w t, u ≠ w → (s.threads u).kind.isEnq = (s.threads w).kind.isEnq →
    (s.threads u).phase.ticketOf = some t → (s.threads w).phase.ticketOf = some t → False
  turnIn : ∀ u t, (s.threads u).phase.inside = some t → s.turn (t % q) = want q (s.threads u).kind t
  wrote : ∀ u v t, (s.threads u).kind = .enq v → (s.threads u).phase = .wrote t → s.val (t % q) = some v
  odd : ∀ i j, s.turn i = 2 * j + 1 → ∃ x, s.ghost[j * q + i]? = some x ∧ s.val i = some x
  readV : ∀ u t v, (s.threads u).kind = .deq → ((s.threads u).phase = .read t v ∨ (s.threads u).phase = .done t (some v)) →
    s.ghost[t]? = some v
  served : ∀ u t, (s.threads u).kind = .deq → (s.threads u).phase.ticketOf = some t →
    (s.threads u).phase ≠ .ticket t → t < s.push

/-- The turn sequencer gives one holder to a slot: two threads that hold one slot are one thread. -/
theorem slot_exclusive [DecidableEq α] (q : Nat) (s : State α) (hs : Inv q s) (u w tu tw : Nat)
    (hu : (s.threads u).phase.inside = some tu) (hw : (s.threads w).phase.inside = some tw) (hslot : tu % q = tw % q) :
    u = w := by
  apply Classical.byContradiction
  intro hne
  have h1 := hs.turnIn u tu hu
  have h2 := hs.turnIn w tw hw
  rw [hslot, h2] at h1
  have hk : (s.threads u).kind.isEnq = (s.threads w).kind.isEnq := by
    unfold want at h1
    cases hku : (s.threads u).kind.isEnq <;> cases hkw : (s.threads w).kind.isEnq <;> simp_all <;> omega
  have hdiv : tu / q = tw / q := by
    unfold want at h1
    rw [hk] at h1
    split at h1 <;> omega
  have htt : tu = tw := by rw [← Nat.div_add_mod' tu q, ← Nat.div_add_mod' tw q, hdiv, hslot]
  subst htt
  have hti : ∀ x, (s.threads x).phase.inside = some tu → (s.threads x).phase.ticketOf = some tu := by
    intro x hx
    cases hp : (s.threads x).phase <;> simp_all [Phase.inside, Phase.ticketOf]
  exact hs.uniq u w tu hne hk (hti u hu) (hti w hw)

theorem get_ext {l m : List α} {t : Nat} {x : α} (h : l[t]? = some x) : (l ++ m)[t]? = some x := by
  obtain ⟨ht, _⟩ := List.getElem?_eq_some_iff.1 h
  rw [List.getElem?_append_left ht]
  exact h

theorem get_lt {l : List α} {t : Nat} {x : α} (h : l[t]? = some x) : t < l.length :=
  (List.getElem?_eq_some_iff.1 h).1

theorem inside_ticket (p : Phase α) (t : Nat) (h : p.inside = some t) : p.ticketOf = some t := by
  cases p <;> simp_all [Phase.inside, Phase.ticketOf]

/-- The points of the FAA of an enqueue: its own, and the point of a dequeue that came first. -/
theorem mem_enqPoints (s : State α) (v : α) (o : LinOp α) :
    o ∈ enqPoints s v ↔ o = .enq s.push v ∨ (s.push < s.pop ∧ o = .deq s.push v) := by
  unfold enqPoints
  by_cases h : s.push < s.pop
  · simp [h]
  · simp [h]

/-- The point of the FAA of a dequeue, where its enqueue came first. -/
theorem mem_deqPoints (s : State α) (o : LinOp α) :
    o ∈ deqPoints s ↔ ∃ x, s.pop < s.push ∧ s.ghost[s.pop]? = some x ∧ o = .deq s.pop x := by
  unfold deqPoints
  split
  · next hlt =>
    split
    · next x hx => simp [hlt, hx]
    · next hx => simp [hx]
  · next hlt => simp [hlt]

section Steps

variable [DecidableEq α] (q : Nat)

/-- The FAA of an enqueue keeps the invariant. -/
theorem inv_enqFAA (s : State α) (hs : Inv q s) (u : Nat) (v : α) (hk : (s.threads u).kind = .enq v)
    (hp : (s.threads u).phase = .idle) : Inv q (step q s u) := by
  have e : step q s u = { s with
      push := s.push + 1
      ghost := s.ghost ++ [v]
      log := s.log ++ enqPoints s v
      threads := update s.threads u ⟨.enq v, .ticket s.push⟩ } := by
    simp only [step, hk, hp]
  rw [e]
  have hlen := hs.len
  refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
  · simp [hlen]
  · show seqRun (s.log ++ enqPoints s v) = some ((s.ghost ++ [v]).drop s.pop)
    rw [seqRun_append, hs.fifo]
    unfold enqPoints
    by_cases hlt : s.push < s.pop
    · have h0 : s.ghost.drop s.pop = [] := List.drop_eq_nil_of_le (by omega)
      have h1 : (s.ghost ++ [v]).drop s.pop = [] := List.drop_eq_nil_of_le (by simp; omega)
      simp [hlt, h0, h1, seqStep]
    · have h1 : (s.ghost ++ [v]).drop s.pop = s.ghost.drop s.pop ++ [v] :=
        List.drop_append_of_le_length (by omega)
      simp [hlt, h1, seqStep]
  · intro t x hm
    rw [List.mem_append, mem_enqPoints] at hm
    rcases hm with hm | hm | ⟨_, hm⟩
    · exact get_ext (hs.logEnq t x hm)
    · cases hm; simp [hlen]
    · cases hm
  · intro t x hm
    rw [List.mem_append, mem_enqPoints] at hm
    rcases hm with hm | hm | ⟨_, hm⟩
    · exact get_ext (hs.logDeq t x hm)
    · cases hm
    · cases hm; simp [hlen]
  · intro t
    show (∃ x, LinOp.enq t x ∈ s.log ++ enqPoints s v) ↔ t < s.push + 1
    constructor
    · rintro ⟨x, hm⟩
      rw [List.mem_append, mem_enqPoints] at hm
      rcases hm with hm | hm | ⟨_, hm⟩
      · have := (hs.memEnq t).1 ⟨x, hm⟩; omega
      · cases hm; omega
      · cases hm
    · intro ht
      by_cases htp : t = s.push
      · rw [htp]; exact ⟨v, List.mem_append.2 (Or.inr ((mem_enqPoints s v _).2 (Or.inl rfl)))⟩
      · obtain ⟨x, hx⟩ := (hs.memEnq t).2 (by omega)
        exact ⟨x, List.mem_append.2 (Or.inl hx)⟩
  · intro t
    show (∃ x, LinOp.deq t x ∈ s.log ++ enqPoints s v) ↔ t < min s.pop (s.push + 1)
    constructor
    · rintro ⟨x, hm⟩
      rw [List.mem_append, mem_enqPoints] at hm
      rcases hm with hm | hm | ⟨hlt, hm⟩
      · have := (hs.memDeq t).1 ⟨x, hm⟩; omega
      · cases hm
      · cases hm; omega
    · intro ht
      by_cases htp : t = s.push
      · have hlt : s.push < s.pop := by omega
        rw [htp]
        exact ⟨v, List.mem_append.2 (Or.inr ((mem_enqPoints s v _).2 (Or.inr ⟨hlt, rfl⟩)))⟩
      · obtain ⟨x, hx⟩ := (hs.memDeq t).2 (by omega)
        exact ⟨x, List.mem_append.2 (Or.inl hx)⟩
  · intro w v' t hkw htw
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at hkw htw
      subst hkw htw
      simp [hlen]
    · simp [hwu] at hkw htw
      exact get_ext (hs.enqT w v' t hkw htw)
  · intro w t hkw htw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw
      exact hs.deqT w t hkw htw
  · intro w1 w2 t hne hkind h1 h2
    by_cases h1u : w1 = u
    · subst h1u
      have h2u : w2 ≠ w1 := Ne.symm hne
      simp [Phase.ticketOf, h2u, Kind.isEnq] at h1 h2 hkind
      subst h1
      cases hk2 : (s.threads w2).kind with
      | enq v2 =>
        have := get_lt (hs.enqT w2 v2 s.push hk2 h2)
        omega
      | deq => rw [hk2] at hkind; simp at hkind
    · by_cases h2u : w2 = u
      · subst h2u
        simp [Phase.ticketOf, h1u, Kind.isEnq] at h1 h2 hkind
        subst h2
        cases hk1 : (s.threads w1).kind with
        | enq v1 =>
          have := get_lt (hs.enqT w1 v1 s.push hk1 h1)
          omega
        | deq => rw [hk1] at hkind; simp at hkind
      · simp [h1u, h2u] at h1 h2 hkind
        exact hs.uniq w1 w2 t hne hkind h1 h2
  · intro w t hin
    by_cases hwu : w = u
    · subst hwu; simp [Phase.inside] at hin
    · simp [hwu] at hin ⊢
      exact hs.turnIn w t hin
  · intro w v' t hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hpw
    · simp [hwu] at hkw hpw
      exact hs.wrote w v' t hkw hpw
  · intro i j hij
    obtain ⟨x, hx, hv⟩ := hs.odd i j hij
    exact ⟨x, get_ext hx, hv⟩
  · intro w t v' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw hpw
      exact get_ext (hs.readV w t v' hkw hpw)
  · intro w t hkw htw hnt
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw hnt
      have := hs.served w t hkw htw hnt
      show t < s.push + 1
      omega

/-- The FAA of a dequeue keeps the invariant. -/
theorem inv_deqFAA (s : State α) (hs : Inv q s) (u : Nat) (hk : (s.threads u).kind = .deq)
    (hp : (s.threads u).phase = .idle) : Inv q (step q s u) := by
  have e : step q s u = { s with
      pop := s.pop + 1
      log := s.log ++ deqPoints s
      threads := update s.threads u ⟨.deq, .ticket s.pop⟩ } := by
    simp only [step, hk, hp]
  rw [e]
  have hlen := hs.len
  refine ⟨hlen, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, hs.odd, ?_, ?_⟩
  · show seqRun (s.log ++ deqPoints s) = some (s.ghost.drop (s.pop + 1))
    rw [seqRun_append, hs.fifo]
    by_cases hlt : s.pop < s.push
    · have hl : s.pop < s.ghost.length := by omega
      have hd : deqPoints s = [.deq s.pop s.ghost[s.pop]] := by
        unfold deqPoints
        simp only [hlt, ↓reduceIte, List.getElem?_eq_getElem hl]
      rw [hd, List.drop_eq_getElem_cons hl]
      simp only [List.foldl_cons, List.foldl_nil, seqStep, ↓reduceIte]
    · have hd : deqPoints s = [] := by unfold deqPoints; simp [hlt]
      have h0 : s.ghost.drop s.pop = [] := List.drop_eq_nil_of_le (by omega)
      have h1 : s.ghost.drop (s.pop + 1) = [] := List.drop_eq_nil_of_le (by omega)
      simp [hd, h0, h1]
  · intro t x hm
    rw [List.mem_append, mem_deqPoints] at hm
    rcases hm with hm | ⟨x', _, _, hm⟩
    · exact hs.logEnq t x hm
    · cases hm
  · intro t x hm
    rw [List.mem_append, mem_deqPoints] at hm
    rcases hm with hm | ⟨x', _, hx', hm⟩
    · exact hs.logDeq t x hm
    · cases hm; exact hx'
  · intro t
    show (∃ x, LinOp.enq t x ∈ s.log ++ deqPoints s) ↔ t < s.push
    constructor
    · rintro ⟨x, hm⟩
      rw [List.mem_append, mem_deqPoints] at hm
      rcases hm with hm | ⟨x', _, _, hm⟩
      · exact (hs.memEnq t).1 ⟨x, hm⟩
      · cases hm
    · intro ht
      obtain ⟨x, hx⟩ := (hs.memEnq t).2 ht
      exact ⟨x, List.mem_append.2 (Or.inl hx)⟩
  · intro t
    show (∃ x, LinOp.deq t x ∈ s.log ++ deqPoints s) ↔ t < min (s.pop + 1) s.push
    constructor
    · rintro ⟨x, hm⟩
      rw [List.mem_append, mem_deqPoints] at hm
      rcases hm with hm | ⟨x', hlt, _, hm⟩
      · have := (hs.memDeq t).1 ⟨x, hm⟩; omega
      · cases hm; omega
    · intro ht
      by_cases hto : t = s.pop
      · have hlt : s.pop < s.push := by omega
        have hl : s.pop < s.ghost.length := by omega
        rw [hto]
        exact ⟨s.ghost[s.pop], List.mem_append.2 (Or.inr ((mem_deqPoints s _).2
          ⟨s.ghost[s.pop], hlt, List.getElem?_eq_getElem hl, rfl⟩))⟩
      · obtain ⟨x, hx⟩ := (hs.memDeq t).2 (by omega)
        exact ⟨x, List.mem_append.2 (Or.inl hx)⟩
  · intro w v' t hkw htw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw
      exact hs.enqT w v' t hkw htw
  · intro w t hkw htw
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at htw
      show t < s.pop + 1
      omega
    · simp [hwu] at hkw htw
      have := hs.deqT w t hkw htw
      show t < s.pop + 1
      omega
  · intro w1 w2 t hne hkind h1 h2
    by_cases h1u : w1 = u
    · subst h1u
      have h2u : w2 ≠ w1 := Ne.symm hne
      simp [Phase.ticketOf, h2u, Kind.isEnq] at h1 h2 hkind
      subst h1
      cases hk2 : (s.threads w2).kind with
      | deq => have := hs.deqT w2 s.pop hk2 h2; omega
      | enq v2 => rw [hk2] at hkind; simp at hkind
    · by_cases h2u : w2 = u
      · subst h2u
        simp [Phase.ticketOf, h1u, Kind.isEnq] at h1 h2 hkind
        subst h2
        cases hk1 : (s.threads w1).kind with
        | deq => have := hs.deqT w1 s.pop hk1 h1; omega
        | enq v1 => rw [hk1] at hkind; simp at hkind
      · simp [h1u, h2u] at h1 h2 hkind
        exact hs.uniq w1 w2 t hne hkind h1 h2
  · intro w t hin
    by_cases hwu : w = u
    · subst hwu; simp [Phase.inside] at hin
    · simp [hwu] at hin ⊢
      exact hs.turnIn w t hin
  · intro w v' t hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw hpw
      exact hs.wrote w v' t hkw hpw
  · intro w t v' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hpw
    · simp [hwu] at hkw hpw
      exact hs.readV w t v' hkw hpw
  · intro w t hkw htw hnt
    by_cases hwu : w = u
    · subst hwu; simp [Phase.ticketOf] at htw hnt; exact absurd htw hnt
    · simp [hwu] at hkw htw hnt
      exact hs.served w t hkw htw hnt

/-- The slot of an odd turn holds the value of its ticket: the ticket `t` at turn `2 * (t / q) + 1`. -/
theorem odd_at (s : State α) (hs : Inv q s) (t : Nat) (h : s.turn (t % q) = 2 * (t / q) + 1) :
    ∃ x, s.ghost[t]? = some x ∧ s.val (t % q) = some x := by
  obtain ⟨x, hx, hv⟩ := hs.odd (t % q) (t / q) h
  rw [Nat.div_add_mod'] at hx
  exact ⟨x, hx, hv⟩

/-- The wait of an enqueue for its turn keeps the invariant. -/
theorem inv_enqWait (s : State α) (hs : Inv q s) (u : Nat) (v : α) (t : Nat) (hk : (s.threads u).kind = .enq v)
    (hp : (s.threads u).phase = .ticket t) (hturn : s.turn (t % q) = 2 * (t / q)) : Inv q (step q s u) := by
  have e : step q s u = { s with threads := update s.threads u ⟨.enq v, .waited t⟩ } := by
    simp [step, hk, hp, hturn]
  rw [e]
  refine ⟨hs.len, hs.fifo, hs.logEnq, hs.logDeq, hs.memEnq, hs.memDeq, ?_, ?_, ?_, ?_, ?_, hs.odd, ?_, ?_⟩
  · intro w v' t' hkw htw
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at hkw htw
      subst hkw htw
      exact hs.enqT w v t hk (by simp [hp, Phase.ticketOf])
    · simp [hwu] at hkw htw; exact hs.enqT w v' t' hkw htw
  · intro w t' hkw htw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw; exact hs.deqT w t' hkw htw
  · intro w1 w2 t' hne hkind h1 h2
    have hti : ∀ x, ((update s.threads u ⟨.enq v, .waited t⟩) x).kind = (s.threads x).kind ∧
        ((update s.threads u ⟨.enq v, .waited t⟩) x).phase.ticketOf = (s.threads x).phase.ticketOf := by
      intro x
      by_cases hx : x = u
      · subst hx; simp [hk, hp, Phase.ticketOf]
      · simp [hx]
    rw [(hti w1).1, (hti w2).1] at hkind
    rw [(hti w1).2] at h1
    rw [(hti w2).2] at h2
    exact hs.uniq w1 w2 t' hne hkind h1 h2
  · intro w t' hin
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.inside] at hin
      subst hin
      simp [want, Kind.isEnq, hturn]
    · simp [hwu] at hin ⊢; exact hs.turnIn w t' hin
  · intro w v' t' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hpw
    · simp [hwu] at hkw hpw; exact hs.wrote w v' t' hkw hpw
  · intro w t' v' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw hpw; exact hs.readV w t' v' hkw hpw
  · intro w t' hkw htw hnt
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw hnt; exact hs.served w t' hkw htw hnt

/-- The wait of a dequeue for its turn keeps the invariant. -/
theorem inv_deqWait (s : State α) (hs : Inv q s) (u : Nat) (t : Nat) (hk : (s.threads u).kind = .deq)
    (hp : (s.threads u).phase = .ticket t) (hturn : s.turn (t % q) = 2 * (t / q) + 1) : Inv q (step q s u) := by
  have e : step q s u = { s with threads := update s.threads u ⟨.deq, .waited t⟩ } := by
    simp [step, hk, hp, hturn]
  rw [e]
  refine ⟨hs.len, hs.fifo, hs.logEnq, hs.logDeq, hs.memEnq, hs.memDeq, ?_, ?_, ?_, ?_, ?_, hs.odd, ?_, ?_⟩
  · intro w v' t' hkw htw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw; exact hs.enqT w v' t' hkw htw
  · intro w t' hkw htw
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at htw
      subst htw
      exact hs.deqT w t hk (by simp [hp, Phase.ticketOf])
    · simp [hwu] at hkw htw; exact hs.deqT w t' hkw htw
  · intro w1 w2 t' hne hkind h1 h2
    have hti : ∀ x, ((update s.threads u ⟨.deq, .waited t⟩) x).kind = (s.threads x).kind ∧
        ((update s.threads u ⟨.deq, .waited t⟩) x).phase.ticketOf = (s.threads x).phase.ticketOf := by
      intro x
      by_cases hx : x = u
      · subst hx; simp [hk, hp, Phase.ticketOf]
      · simp [hx]
    rw [(hti w1).1, (hti w2).1] at hkind
    rw [(hti w1).2] at h1
    rw [(hti w2).2] at h2
    exact hs.uniq w1 w2 t' hne hkind h1 h2
  · intro w t' hin
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.inside] at hin
      subst hin
      simp [want, Kind.isEnq, hturn]
    · simp [hwu] at hin ⊢; exact hs.turnIn w t' hin
  · intro w v' t' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw hpw; exact hs.wrote w v' t' hkw hpw
  · intro w t' v' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hpw
    · simp [hwu] at hkw hpw; exact hs.readV w t' v' hkw hpw
  · intro w t' hkw htw hnt
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at htw
      subst htw
      obtain ⟨x, hx, _⟩ := odd_at q s hs t hturn
      have := get_lt hx
      rw [hs.len] at this
      exact this
    · simp [hwu] at hkw htw hnt; exact hs.served w t' hkw htw hnt

/-- The write of an enqueue keeps the invariant. -/
theorem inv_enqWrite (s : State α) (hs : Inv q s) (u : Nat) (v : α) (t : Nat) (hk : (s.threads u).kind = .enq v)
    (hp : (s.threads u).phase = .waited t) : Inv q (step q s u) := by
  have e : step q s u =
      { s with val := update s.val (t % q) (some v), threads := update s.threads u ⟨.enq v, .wrote t⟩ } := by
    simp [step, hk, hp]
  rw [e]
  have hinu : (s.threads u).phase.inside = some t := by simp [hp, Phase.inside]
  have hturn := hs.turnIn u t hinu
  rw [hk] at hturn
  simp only [want, Kind.isEnq, ↓reduceIte, Nat.add_zero] at hturn
  refine ⟨hs.len, hs.fifo, hs.logEnq, hs.logDeq, hs.memEnq, hs.memDeq, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
  · intro w v' t' hkw htw
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at hkw htw
      subst hkw htw
      exact hs.enqT w v t hk (by simp [hp, Phase.ticketOf])
    · simp [hwu] at hkw htw; exact hs.enqT w v' t' hkw htw
  · intro w t' hkw htw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw; exact hs.deqT w t' hkw htw
  · intro w1 w2 t' hne hkind h1 h2
    have hti : ∀ x, ((update s.threads u ⟨.enq v, .wrote t⟩) x).kind = (s.threads x).kind ∧
        ((update s.threads u ⟨.enq v, .wrote t⟩) x).phase.ticketOf = (s.threads x).phase.ticketOf := by
      intro x
      by_cases hx : x = u
      · subst hx; simp [hk, hp, Phase.ticketOf]
      · simp [hx]
    rw [(hti w1).1, (hti w2).1] at hkind
    rw [(hti w1).2] at h1
    rw [(hti w2).2] at h2
    exact hs.uniq w1 w2 t' hne hkind h1 h2
  · intro w t' hin
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.inside] at hin
      subst hin
      simp [want, Kind.isEnq, hturn]
    · simp [hwu] at hin ⊢; exact hs.turnIn w t' hin
  · intro w v' t' hkw hpw
    by_cases hwu : w = u
    · subst hwu
      simp at hkw hpw
      subst hkw hpw
      simp
    · simp [hwu] at hkw hpw
      have hslot : t' % q ≠ t % q := fun hsl =>
        hwu (slot_exclusive q s hs w u t' t (by simp [hpw, Phase.inside]) hinu hsl)
      simp [hslot]
      exact hs.wrote w v' t' hkw hpw
  · intro i j hij
    by_cases hi : i = t % q
    · subst hi; rw [hturn] at hij; omega
    · obtain ⟨x, hx, hv⟩ := hs.odd i j hij
      exact ⟨x, hx, by simp [hi, hv]⟩
  · intro w t' v' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw hpw; exact hs.readV w t' v' hkw hpw
  · intro w t' hkw htw hnt
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw hnt; exact hs.served w t' hkw htw hnt

/-- The read of a dequeue keeps the invariant. -/
theorem inv_deqRead (s : State α) (hs : Inv q s) (u : Nat) (t : Nat) (v : α) (hk : (s.threads u).kind = .deq)
    (hp : (s.threads u).phase = .waited t) (hv : s.val (t % q) = some v) : Inv q (step q s u) := by
  have e : step q s u = { s with threads := update s.threads u ⟨.deq, .read t v⟩ } := by
    simp [step, hk, hp, hv]
  rw [e]
  have hinu : (s.threads u).phase.inside = some t := by simp [hp, Phase.inside]
  have hturn := hs.turnIn u t hinu
  rw [hk] at hturn
  simp only [want, Kind.isEnq, Bool.false_eq_true, ↓reduceIte] at hturn
  obtain ⟨x, hx, hvx⟩ := odd_at q s hs t hturn
  rw [hv] at hvx
  cases hvx
  refine ⟨hs.len, hs.fifo, hs.logEnq, hs.logDeq, hs.memEnq, hs.memDeq, ?_, ?_, ?_, ?_, ?_, hs.odd, ?_, ?_⟩
  · intro w v' t' hkw htw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw; exact hs.enqT w v' t' hkw htw
  · intro w t' hkw htw
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at htw
      subst htw
      exact hs.deqT w t hk (by simp [hp, Phase.ticketOf])
    · simp [hwu] at hkw htw; exact hs.deqT w t' hkw htw
  · intro w1 w2 t' hne hkind h1 h2
    have hti : ∀ y, ((update s.threads u ⟨.deq, .read t v⟩) y).kind = (s.threads y).kind ∧
        ((update s.threads u ⟨.deq, .read t v⟩) y).phase.ticketOf = (s.threads y).phase.ticketOf := by
      intro y
      by_cases hy : y = u
      · subst hy; simp [hk, hp, Phase.ticketOf]
      · simp [hy]
    rw [(hti w1).1, (hti w2).1] at hkind
    rw [(hti w1).2] at h1
    rw [(hti w2).2] at h2
    exact hs.uniq w1 w2 t' hne hkind h1 h2
  · intro w t' hin
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.inside] at hin
      subst hin
      simp [want, Kind.isEnq, hturn]
    · simp [hwu] at hin ⊢; exact hs.turnIn w t' hin
  · intro w v' t' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw hpw; exact hs.wrote w v' t' hkw hpw
  · intro w t' v' hkw hpw
    by_cases hwu : w = u
    · subst hwu
      simp at hpw
      obtain ⟨rfl, rfl⟩ := hpw
      exact hx
    · simp [hwu] at hkw hpw; exact hs.readV w t' v' hkw hpw
  · intro w t' hkw htw hnt
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at htw
      subst htw
      exact hs.served w t hk (by simp [hp, Phase.ticketOf]) (by simp [hp])
    · simp [hwu] at hkw htw hnt; exact hs.served w t' hkw htw hnt

/-- The completion of the turn of an enqueue keeps the invariant. -/
theorem inv_enqComplete (s : State α) (hs : Inv q s) (u : Nat) (v : α) (t : Nat) (hk : (s.threads u).kind = .enq v)
    (hp : (s.threads u).phase = .wrote t) : Inv q (step q s u) := by
  have e : step q s u = { s with
      turn := update s.turn (t % q) (2 * (t / q) + 1)
      threads := update s.threads u ⟨.enq v, .done t none⟩ } := by
    simp [step, hk, hp]
  rw [e]
  have hinu : (s.threads u).phase.inside = some t := by simp [hp, Phase.inside]
  have hval := hs.wrote u v t hk hp
  have hg := hs.enqT u v t hk (by simp [hp, Phase.ticketOf])
  refine ⟨hs.len, hs.fifo, hs.logEnq, hs.logDeq, hs.memEnq, hs.memDeq, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
  · intro w v' t' hkw htw
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at hkw htw
      subst hkw htw
      exact hg
    · simp [hwu] at hkw htw; exact hs.enqT w v' t' hkw htw
  · intro w t' hkw htw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw; exact hs.deqT w t' hkw htw
  · intro w1 w2 t' hne hkind h1 h2
    have hti : ∀ y, ((update s.threads u ⟨.enq v, .done t none⟩) y).kind = (s.threads y).kind ∧
        ((update s.threads u ⟨.enq v, .done t none⟩) y).phase.ticketOf = (s.threads y).phase.ticketOf := by
      intro y
      by_cases hy : y = u
      · subst hy; simp [hk, hp, Phase.ticketOf]
      · simp [hy]
    rw [(hti w1).1, (hti w2).1] at hkind
    rw [(hti w1).2] at h1
    rw [(hti w2).2] at h2
    exact hs.uniq w1 w2 t' hne hkind h1 h2
  · intro w t' hin
    by_cases hwu : w = u
    · subst hwu; simp [Phase.inside] at hin
    · simp [hwu] at hin ⊢
      have hslot : t' % q ≠ t % q := fun hsl => hwu (slot_exclusive q s hs w u t' t hin hinu hsl)
      simp [hslot]
      exact hs.turnIn w t' hin
  · intro w v' t' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hpw
    · simp [hwu] at hkw hpw; exact hs.wrote w v' t' hkw hpw
  · intro i j hij
    by_cases hi : i = t % q
    · subst hi
      simp at hij
      have hj : j = t / q := by omega
      subst hj
      rw [Nat.div_add_mod']
      exact ⟨v, hg, hval⟩
    · simp [hi] at hij
      exact hs.odd i j hij
  · intro w t' v' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw hpw; exact hs.readV w t' v' hkw hpw
  · intro w t' hkw htw hnt
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw hnt; exact hs.served w t' hkw htw hnt

/-- The completion of the turn of a dequeue keeps the invariant. -/
theorem inv_deqComplete (s : State α) (hs : Inv q s) (u : Nat) (t : Nat) (v : α) (hk : (s.threads u).kind = .deq)
    (hp : (s.threads u).phase = .read t v) : Inv q (step q s u) := by
  have e : step q s u = { s with
      turn := update s.turn (t % q) (2 * (t / q) + 2)
      threads := update s.threads u ⟨.deq, .done t (some v)⟩ } := by
    simp [step, hk, hp]
  rw [e]
  have hinu : (s.threads u).phase.inside = some t := by simp [hp, Phase.inside]
  refine ⟨hs.len, hs.fifo, hs.logEnq, hs.logDeq, hs.memEnq, hs.memDeq, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
  · intro w v' t' hkw htw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw htw; exact hs.enqT w v' t' hkw htw
  · intro w t' hkw htw
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at htw
      subst htw
      exact hs.deqT w t hk (by simp [hp, Phase.ticketOf])
    · simp [hwu] at hkw htw; exact hs.deqT w t' hkw htw
  · intro w1 w2 t' hne hkind h1 h2
    have hti : ∀ y, ((update s.threads u ⟨.deq, .done t (some v)⟩) y).kind = (s.threads y).kind ∧
        ((update s.threads u ⟨.deq, .done t (some v)⟩) y).phase.ticketOf = (s.threads y).phase.ticketOf := by
      intro y
      by_cases hy : y = u
      · subst hy; simp [hk, hp, Phase.ticketOf]
      · simp [hy]
    rw [(hti w1).1, (hti w2).1] at hkind
    rw [(hti w1).2] at h1
    rw [(hti w2).2] at h2
    exact hs.uniq w1 w2 t' hne hkind h1 h2
  · intro w t' hin
    by_cases hwu : w = u
    · subst hwu; simp [Phase.inside] at hin
    · simp [hwu] at hin ⊢
      have hslot : t' % q ≠ t % q := fun hsl => hwu (slot_exclusive q s hs w u t' t hin hinu hsl)
      simp [hslot]
      exact hs.turnIn w t' hin
  · intro w v' t' hkw hpw
    by_cases hwu : w = u
    · subst hwu; simp at hkw
    · simp [hwu] at hkw hpw; exact hs.wrote w v' t' hkw hpw
  · intro i j hij
    by_cases hi : i = t % q
    · subst hi; simp at hij; omega
    · simp [hi] at hij
      exact hs.odd i j hij
  · intro w t' v' hkw hpw
    by_cases hwu : w = u
    · subst hwu
      simp at hpw
      obtain ⟨rfl, rfl⟩ := hpw
      exact hs.readV w t v hk (Or.inl hp)
    · simp [hwu] at hkw hpw; exact hs.readV w t' v' hkw hpw
  · intro w t' hkw htw hnt
    by_cases hwu : w = u
    · subst hwu
      simp [Phase.ticketOf] at htw
      subst htw
      exact hs.served w t hk (by simp [hp, Phase.ticketOf]) (by simp [hp])
    · simp [hwu] at hkw htw hnt; exact hs.served w t' hkw htw hnt

/-- Each step keeps the invariant. -/
theorem step_inv (s : State α) (hs : Inv q s) (u : Nat) : Inv q (step q s u) := by
  cases hk : (s.threads u).kind with
  | enq v =>
    cases hp : (s.threads u).phase with
    | idle => exact inv_enqFAA q s hs u v hk hp
    | ticket t =>
      by_cases ht : s.turn (t % q) = 2 * (t / q)
      · exact inv_enqWait q s hs u v t hk hp ht
      · have e : step q s u = s := by simp [step, hk, hp, ht]
        rw [e]; exact hs
    | waited t => exact inv_enqWrite q s hs u v t hk hp
    | wrote t => exact inv_enqComplete q s hs u v t hk hp
    | read t x =>
      have e : step q s u = s := by simp [step, hk, hp]
      rw [e]; exact hs
    | done t r =>
      have e : step q s u = s := by simp [step, hk, hp]
      rw [e]; exact hs
  | deq =>
    cases hp : (s.threads u).phase with
    | idle => exact inv_deqFAA q s hs u hk hp
    | ticket t =>
      by_cases ht : s.turn (t % q) = 2 * (t / q) + 1
      · exact inv_deqWait q s hs u t hk hp ht
      · have e : step q s u = s := by simp [step, hk, hp, ht]
        rw [e]; exact hs
    | waited t =>
      cases hv : s.val (t % q) with
      | some v => exact inv_deqRead q s hs u t v hk hp hv
      | none =>
        have e : step q s u = s := by simp [step, hk, hp, hv]
        rw [e]; exact hs
    | read t v => exact inv_deqComplete q s hs u t v hk hp
    | wrote t =>
      have e : step q s u = s := by simp [step, hk, hp]
      rw [e]; exact hs
    | done t r =>
      have e : step q s u = s := by simp [step, hk, hp]
      rw [e]; exact hs

theorem run_inv (s : State α) (hs : Inv q s) (sched : List Nat) : Inv q (run q s sched) := by
  induction sched generalizing s with
  | nil => exact hs
  | cons u rest ih => exact ih _ (step_inv q s hs u)

/-- The start keeps the invariant. -/
theorem start_inv (calls : Nat → Kind α) : Inv q (start calls) := by
  refine ⟨rfl, rfl, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;>
    intros <;> simp_all [start, Phase.ticketOf, Phase.inside]

end Steps

/-! ## What the queue gives -/

section Results

variable [DecidableEq α] (q : Nat)

/-- L1: the log is a legal history of the queue with one lock. Its state at the end is the ghost list with its first
`popTicket` values dropped. -/
theorem fifo (calls : Nat → Kind α) (sched : List Nat) :
    seqRun (run q (start calls) sched).log = some ((run q (start calls) sched).ghost.drop (run q (start calls) sched).pop) :=
  (run_inv q _ (start_inv q calls) sched).fifo

/-- A dequeue gives the value of the enqueue of its ticket, and its point in the log has that value. -/
theorem dequeue_value (s : State α) (hs : Inv q s) (u t : Nat) (v : α) (hk : (s.threads u).kind = .deq)
    (hp : (s.threads u).phase = .done t (some v)) : s.ghost[t]? = some v ∧ LinOp.deq t v ∈ s.log := by
  have hg := hs.readV u t v hk (Or.inr hp)
  have hpop := hs.deqT u t hk (by simp [hp, Phase.ticketOf])
  have hpush := hs.served u t hk (by simp [hp, Phase.ticketOf]) (by simp [hp])
  obtain ⟨x, hx⟩ := (hs.memDeq t).2 (by omega)
  have hgx := hs.logDeq t x hx
  rw [hg] at hgx
  cases hgx
  exact ⟨hg, hx⟩

/-- A dequeue that waited for its turn finds a value in its slot. So the `assert(false)` of `dequeueSEQ` never runs. -/
theorem read_finds_value (s : State α) (hs : Inv q s) (u t : Nat) (hk : (s.threads u).kind = .deq)
    (hp : (s.threads u).phase = .waited t) : ∃ v, s.val (t % q) = some v := by
  have hturn := hs.turnIn u t (by simp [hp, Phase.inside])
  rw [hk] at hturn
  simp only [want, Kind.isEnq, Bool.false_eq_true, ↓reduceIte] at hturn
  obtain ⟨x, _, hv⟩ := odd_at q s hs t hturn
  exact ⟨x, hv⟩

omit [DecidableEq α] in
/-- The external linearization point. An enqueue whose dequeue took its FAA first puts the point of that dequeue in
the log, right after its own point. -/
theorem external_point (s : State α) (u : Nat) (v : α) (hk : (s.threads u).kind = .enq v)
    (hp : (s.threads u).phase = .idle) (hlt : s.push < s.pop) :
    (step q s u).log = s.log ++ [.enq s.push v, .deq s.push v] := by
  simp [step, hk, hp, enqPoints, hlt]

omit [DecidableEq α] in
/-- A run only appends to the log. -/
theorem run_log_prefix (s : State α) (sched : List Nat) : ∃ more, (run q s sched).log = s.log ++ more := by
  induction sched generalizing s with
  | nil => exact ⟨[], by simp [run]⟩
  | cons u rest ih =>
    obtain ⟨more, hm⟩ := ih (step q s u)
    have hstep : ∃ d, (step q s u).log = s.log ++ d := by
      unfold step
      split
      · exact ⟨_, rfl⟩
      · exact ⟨_, rfl⟩
      · split <;> exact ⟨[], by simp⟩
      · split <;> exact ⟨[], by simp⟩
      · exact ⟨[], by simp⟩
      · split <;> exact ⟨[], by simp⟩
      · exact ⟨[], by simp⟩
      · exact ⟨[], by simp⟩
      · exact ⟨[], by simp⟩
    obtain ⟨d, hd⟩ := hstep
    refine ⟨d ++ more, ?_⟩
    show (run q (step q s u) rest).log = _
    rw [hm, hd, List.append_assoc]

/-- A call that ended has its point in the log. -/
theorem done_in_log (s : State α) (hs : Inv q s) (u t : Nat) (r : Option α) (hp : (s.threads u).phase = .done t r) :
    ∃ o ∈ s.log, (∃ x, o = .enq t x ∧ (s.threads u).kind.isEnq = true) ∨
      (∃ x, o = .deq t x ∧ (s.threads u).kind.isEnq = false) := by
  cases hk : (s.threads u).kind with
  | enq v =>
    have hg := hs.enqT u v t hk (by simp [hp, Phase.ticketOf])
    have hlt := get_lt hg
    rw [hs.len] at hlt
    obtain ⟨x, hx⟩ := (hs.memEnq t).2 hlt
    exact ⟨_, hx, Or.inl ⟨x, rfl, rfl⟩⟩
  | deq =>
    have hpop := hs.deqT u t hk (by simp [hp, Phase.ticketOf])
    have hpush := hs.served u t hk (by simp [hp, Phase.ticketOf]) (by simp [hp])
    obtain ⟨x, hx⟩ := (hs.memDeq t).2 (by omega)
    exact ⟨_, hx, Or.inr ⟨x, rfl, rfl⟩⟩

omit [DecidableEq α] in
/-- L2: a call that ended before another call took effect comes first in the log. -/
theorem realtime (s : State α) (sched : List Nat) (a b : LinOp α) (ha : a ∈ s.log) (hb : b ∉ s.log) :
    ∃ pre post, (run q s sched).log = pre ++ post ∧ a ∈ pre ∧ b ∉ pre := by
  obtain ⟨more, hm⟩ := run_log_prefix q s sched
  exact ⟨s.log, more, hm, ha, hb⟩

end Results

/-! ## The two runs of Section 3.3 -/

/-- Two enqueues of `1` and `2`, and a dequeue. -/
def twoEnqueues : Nat → Kind Nat := fun u => if u = 0 then .enq 1 else if u = 1 then .enq 2 else .deq

/-- The first run of Section 3.3. The enqueue of `1` takes ticket 0, the enqueue of `2` takes ticket 1 and writes
first, and then the enqueue of `1` writes. The dequeue takes ticket 0 and gives `1`. So the point of an enqueue is its
FAA and not its write. -/
theorem first_run :
    ((run 2 (start twoEnqueues) [0, 1, 1, 1, 1, 0, 0, 0, 2, 2, 2, 2]).threads 2).phase matches .done 0 (some 1) := by
  decide

/-- A dequeue, then an enqueue of `1`. -/
def deqFirst : Nat → Kind Nat := fun u => if u = 0 then .deq else .enq 1

/-- The second run of Section 3.3. The dequeue takes ticket 0 before the enqueue takes its own. The FAA of the
enqueue puts both points in the log, the enqueue and then the dequeue: the external linearization point. The dequeue
then gives `1`. -/
theorem second_run :
    (run 2 (start deqFirst) [0, 1]).log.length = 2 ∧
      (run 2 (start deqFirst) [0, 1]).log.head? matches some (.enq 0 1) ∧
      (run 2 (start deqFirst) [0, 1]).log.getLast? matches some (.deq 0 1) ∧
      ((run 2 (start deqFirst) [0, 1, 1, 1, 1, 0, 0, 0]).threads 0).phase matches .done 0 (some 1) := by
  decide

end LocusCarcerum.MPMCQueue
