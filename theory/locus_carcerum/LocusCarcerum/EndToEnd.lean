import EmbeddedTypes.Integrated
import EmbeddedTypes.Manifold.Wake
import LocusCarcerum.Linearizable

/-!
# LocusCarcerum, a call from the ring to the cells, end to end

embedded_types is the host, and this file runs a call through all of it. The state of `EmbeddedTypes.Integrated`
holds the lanes of the turnstile, the workers and the slots of the ticket ring. Beside it, the state of
`Linearizable.lean` holds the claim words, the cells and the calls.

* A worker in a pass grants an empty slot the next ticket, and pushes a call to the granted slot, each with one CAS.
  Any count of workers can grant and push.

* A worker enters a free lane with the fetch-or of the turnstile. That lane is its thread handle.

* A worker in a pass pops a pushed slot with one CAS, which writes its lane into the slot. Any count of workers can
  pop, and of two pops of one slot, one lands. A job whose pointer fails its check does not run.

* The worker does the four steps of the call inside its pass, and then it leaves the lane.

* The supervisor kills a worker, and frees its lane on its behalf. The free also runs the rest of the call of the
  worker: its work and its release. The caller holds a token alone, so the internals are free to do this.

Each step of the host is a step of `EmbeddedTypes.Integrated`, so its invariant holds. Each change of a
lane word is a step of `EmbeddedTypes.Manifold.Drain`. Each such step is a run of `EmbeddedTypes.Manifold.Wake`, which
holds the futex contract. The theorems hold for each count of lanes, producers and workers, and each order of steps.

* `refused_never_runs`: a call whose job pointer failed its check never takes a step.

* `call_once`: one worker at max runs the call of a slot.

* `call_in_pass`: a worker that runs a call holds its lane, and the slot names that lane.

* `drain_waits_for_call`: a drain reads the lane of a call in progress, and later reads the lane over. It then finds
  the call done, or its worker freed by the supervisor. So a writer, the mover among them, never works on a cell that
  a call holds.

* `lane_wakes`: each lane step is a run of `Wake.lean`. So a drain that sleeps on the lane word loses no wake
  (`EmbeddedTypes.Manifold.Wake.no_lost_wake`).

* `exclusive`: the invariant of `Linearizable.lean` holds, and no two calls hold one element.

* `freed_holds_nothing`: after the free of a dead worker, its call holds no claim. So no element stays blocked.

* `linearizable`: the conditions L1 and L2 of Herlihy and Wing hold end to end, as `Linearizable.lean` states them.

The sources of the models that this file takes in.

* `EmbeddedTypes.Integrated`, `EmbeddedTypes.Manifold.Drain` and `EmbeddedTypes.Manifold.Wake`, in
  `deps/embedded_types/theory/embedded_types`. Their paper is D. Quigg, `The Turnstile-Oracle Causal Ring: A decoupled
  synchronization and storage architecture, proved in Lean 4`, embedded_types, `theory/papers/turnstile_ticket_ring`,
  2026.

* `Wake.lean` holds the futex contract: the compare and the sleep are one step, and a wake ends each sleep on the word.
  The contract is the one of `futex(2)` in the Linux Programmer's Manual, and of U. Drepper, `Futexes Are Tricky`, Red
  Hat, 2011.

* The grant and the push are the steps `grant` and `push` of `Integrated.lean`, as `embed_ticket_ring_pass_grant`
  and `embed_ticket_ring_pass_push` run them. This file runs no replay and no release, so each call runs once.
-/

namespace LocusCarcerum.EndToEnd

open LocusCarcerum.Linearizable
open EmbeddedTypes

variable {α : Type} {W : Nat}

/-- A call on the ring: the element that it claims and what it does. -/
structure Job (α : Type) where
  elem : Nat
  kind : Kind α

/-- The state end to end. The host, the run of the calls, the slot of the call that each worker runs, and the worker
that popped each slot. -/
structure E (α : Type) (W : Nat) where
  host : Integrated.State
  mem : State α W
  bound : Nat → Option Nat
  boundBy : Nat → Option Nat

/-- A step end to end. -/
inductive Op where
  | grant (worker slot : Nat)
  | push (worker slot : Nat) (intact : Bool)
  | enter (worker lane : Nat)
  | pop (worker slot : Nat)
  | work (worker : Nat)
  | finish (worker : Nat)
  | leave (worker : Nat)
  | kill (worker : Nat)
  | free (worker : Nat)

/-- One step, with `n` lanes. A step whose condition does not hold changes nothing. -/
def estep (n : Nat) (e : E α W) : Op → E α W
  | .grant w a =>
    match e.host.worker w with
    | .inPass _ =>
      if e.host.slot a = .empty then
        { e with host := { e.host.setSlot a (.granted e.host.next) with next := e.host.next + 1 } }
      else e
    | _ => e
  | .push w a intact =>
    match e.host.worker w, e.host.slot a with
    | .inPass _, .granted st => { e with host := e.host.setSlot a (.pushed st intact) }
    | _, _ => e
  | .enter w l =>
    if l < n ∧ e.host.worker w = .idle ∧ (e.host.lane l).busy = false ∧ (e.host.lane l).ended w = false then
      { e with host := (e.host.setLane l (Integrated.laneEntered (e.host.lane l) w)).setWorker w (.inPass l) }
    else e
  | .pop w a =>
    match e.host.worker w, e.host.slot a with
    | .inPass l, .pushed stamp intact =>
      if e.bound w = none then
        { e with
          host := e.host.setSlot a (.popped stamp l intact)
          bound := if intact then update e.bound w (some a) else e.bound
          boundBy := if intact then update e.boundBy a (some w) else e.boundBy }
      else e
    | _, _ => e
  | .work w =>
    match e.host.worker w, e.bound w with
    | .inPass _, some a => { e with mem := step e.mem a }
    | _, _ => e
  | .finish w =>
    match e.bound w with
    | some a => if (e.mem.threads a).phase.isDone then { e with bound := update e.bound w none } else e
    | none => e
  | .leave w =>
    match e.host.worker w with
    | .inPass l =>
      if e.bound w = none then
        { e with host := (e.host.setLane l (Integrated.laneLeft (e.host.lane l) w)).setWorker w .idle }
      else e
    | _ => e
  | .kill w =>
    match e.host.worker w with
    | .inPass l => { e with host := e.host.setWorker w (.dead l) }
    | _ => e
  | .free w =>
    match e.host.worker w with
    | .dead l =>
      { e with
        host := (e.host.setLane l (Integrated.laneLeft (e.host.lane l) w)).setWorker w .gone
        mem :=
          match e.bound w with
          | some a => finish e.mem a
          | none => e.mem }
    | _ => e

/-- A run end to end. -/
def erun (n : Nat) (e : E α W) (ops : List Op) : E α W := ops.foldl (estep n) e

/-- The start: the host at rest, each call before its read, no slot popped. -/
def estart (jobs : Nat → Job α) (cells : Nat → α) : E α W :=
  ⟨Integrated.rest, start cells (fun a => ⟨(jobs a).elem, (jobs a).kind, .idle⟩), fun _ => none, fun _ => none⟩

/-- The invariant end to end.

* The host keeps the invariant of `EmbeddedTypes.Integrated`, and the calls keep the invariant of
  `Linearizable.lean`.
* Each thread does the call of its slot.

* A worker that runs a call popped its slot, which names the lane of the worker. The worker is in its pass, or dead
  on that lane, or freed.
* A slot that a worker popped stays popped, and a call that no worker popped is before its read.
* A popped call whose worker does not run it now has its four steps behind it.

* A worker whose pass ended on a lane is not on that lane, and an idle worker runs no call. -/
structure EInv (jobs : Nat → Job α) (e : E α W) : Prop where
  host : Integrated.Inv e.host
  lin : Inv e.mem
  call : ∀ a, (e.mem.threads a).elem = (jobs a).elem ∧ (e.mem.threads a).kind = (jobs a).kind
  bind : ∀ w a, e.bound w = some a → e.boundBy a = some w ∧ ∃ stamp l, e.host.slot a = .popped stamp l true ∧
    (e.host.worker w = .inPass l ∨ e.host.worker w = .dead l ∨ e.host.worker w = .gone)
  once : ∀ a w, e.boundBy a = some w → ∃ stamp l, e.host.slot a = .popped stamp l true
  idle : ∀ a, e.boundBy a = none → (e.mem.threads a).phase = .idle
  done : ∀ a w, e.boundBy a = some w → e.bound w ≠ some a → (e.mem.threads a).phase.isDone = true
  ended : ∀ l w, (e.host.lane l).ended w = true → e.host.worker w ≠ .inPass l ∧ e.host.worker w ≠ .dead l
  rested : ∀ w, e.host.worker w = .idle → e.bound w = none

theorem estart_inv (jobs : Nat → Job α) (cells : Nat → α) : EInv jobs (estart jobs cells : E α W) := by
  refine ⟨Integrated.rest_inv, start_inv cells _ fun _ => rfl, fun _ => ⟨rfl, rfl⟩, ?_, ?_, fun _ _ => rfl, ?_, ?_,
    fun _ _ => rfl⟩
  · intro w a h; simp [estart] at h
  · intro a w h; simp [estart] at h
  · intro a w h; simp [estart] at h
  · intro l w h; simp [estart, Integrated.rest, Integrated.laneRest] at h

/-! ## Each step keeps the invariant -/

section Steps

variable (n : Nat) (jobs : Nat → Job α)

/-- A step of the host that changes one slot, which holds no popped call, keeps the invariant. The lanes and the
workers stay as they were, and the host keeps the invariant of `EmbeddedTypes.Integrated`. -/
theorem inv_slot (e : E α W) (hi : EInv jobs e) (a : Nat) (h' : Integrated.State) (hhost : Integrated.Inv h')
    (hlane : h'.lane = e.host.lane) (hworker : h'.worker = e.host.worker)
    (hslot : ∀ b, b ≠ a → h'.slot b = e.host.slot b) (hfree : ∀ st l r, e.host.slot a ≠ .popped st l r) :
    EInv jobs { e with host := h' } := by
  refine ⟨hhost, hi.lin, hi.call, ?_, ?_, hi.idle, hi.done, ?_, ?_⟩
  · intro w a' hb
    obtain ⟨hby, st, l, hp, hw⟩ := hi.bind w a' hb
    have hne : a' ≠ a := fun h => by subst h; exact hfree _ _ _ hp
    exact ⟨hby, st, l, by rw [hslot a' hne]; exact hp, by rw [hworker]; exact hw⟩
  · intro a' w hb
    obtain ⟨st, l, hp⟩ := hi.once a' w hb
    have hne : a' ≠ a := fun h => by subst h; exact hfree _ _ _ hp
    exact ⟨st, l, by rw [hslot a' hne]; exact hp⟩
  · intro l w he
    show h'.worker w ≠ .inPass l ∧ h'.worker w ≠ .dead l
    rw [hworker]
    exact hi.ended l w (by rw [← hlane]; exact he)
  · intro w hw
    exact hi.rested w (by rw [← hworker]; exact hw)

/-- The grant of a slot in a pass keeps the invariant: the step `grant` of `EmbeddedTypes.Integrated`. -/
theorem inv_grant (e : E α W) (hi : EInv jobs e) (w a : Nat) : EInv jobs (estep n e (.grant w a)) := by
  cases hw : e.host.worker w with
  | inPass l =>
    by_cases hs : e.host.slot a = .empty
    · have e1 : estep n e (.grant w a) =
          { e with host := { e.host.setSlot a (.granted e.host.next) with next := e.host.next + 1 } } := by
        simp [estep, hw, hs]
      rw [e1]
      exact inv_slot jobs e hi a _ (Integrated.step_inv hi.host (Integrated.Step.grant (n := n) w l a hw (Or.inl hs)))
        rfl rfl (fun b hb => by simp [Integrated.State.setSlot, hb]) (fun _ _ _ h => by rw [hs] at h; cases h)
    · have e1 : estep n e (.grant w a) = e := by simp [estep, hw, hs]
      rw [e1]; exact hi
  | idle => have e1 : estep n e (.grant w a) = e := by simp [estep, hw]
            rw [e1]; exact hi
  | dead l => have e1 : estep n e (.grant w a) = e := by simp [estep, hw]
              rw [e1]; exact hi
  | gone => have e1 : estep n e (.grant w a) = e := by simp [estep, hw]
            rw [e1]; exact hi

/-- The push of a job to a granted slot in a pass keeps the invariant: the step `push` of
`EmbeddedTypes.Integrated`. -/
theorem inv_push (e : E α W) (hi : EInv jobs e) (w a : Nat) (intact : Bool) :
    EInv jobs (estep n e (.push w a intact)) := by
  cases hw : e.host.worker w with
  | inPass l =>
    cases hs : e.host.slot a with
    | granted st =>
      have e1 : estep n e (.push w a intact) = { e with host := e.host.setSlot a (.pushed st intact) } := by
        simp [estep, hw, hs]
      rw [e1]
      exact inv_slot jobs e hi a _ (Integrated.step_inv hi.host (Integrated.Step.push (n := n) w l a st intact hw hs))
        rfl rfl (fun b hb => by simp [Integrated.State.setSlot, hb]) (fun _ _ _ h => by rw [hs] at h; cases h)
    | empty => have e1 : estep n e (.push w a intact) = e := by simp [estep, hw, hs]
               rw [e1]; exact hi
    | pushed _ _ => have e1 : estep n e (.push w a intact) = e := by simp [estep, hw, hs]
                    rw [e1]; exact hi
    | popped _ _ _ => have e1 : estep n e (.push w a intact) = e := by simp [estep, hw, hs]
                      rw [e1]; exact hi
    | replayed _ _ _ => have e1 : estep n e (.push w a intact) = e := by simp [estep, hw, hs]
                        rw [e1]; exact hi
    | released _ _ _ => have e1 : estep n e (.push w a intact) = e := by simp [estep, hw, hs]
                        rw [e1]; exact hi
  | idle => have e1 : estep n e (.push w a intact) = e := by simp [estep, hw]
            rw [e1]; exact hi
  | dead l => have e1 : estep n e (.push w a intact) = e := by simp [estep, hw]
              rw [e1]; exact hi
  | gone => have e1 : estep n e (.push w a intact) = e := by simp [estep, hw]
            rw [e1]; exact hi

theorem inv_enter (e : E α W) (hi : EInv jobs e) (w l : Nat) : EInv jobs (estep n e (.enter w l)) := by
  by_cases hc : l < n ∧ e.host.worker w = .idle ∧ (e.host.lane l).busy = false ∧ (e.host.lane l).ended w = false
  · have e1 : estep n e (.enter w l) =
        { e with host := (e.host.setLane l (Integrated.laneEntered (e.host.lane l) w)).setWorker w (.inPass l) } := by
      simp only [estep, hc, and_self, ↓reduceIte]
    obtain ⟨hn, hw, hl, hf⟩ := hc
    rw [e1]
    have hb := hi.rested w hw
    refine ⟨Integrated.step_inv hi.host (Integrated.Step.enter w l hn hw hl hf), hi.lin, hi.call, ?_, hi.once, hi.idle,
      hi.done, ?_, ?_⟩
    · intro w' a hb'
      have hne : w' ≠ w := fun h => by subst h; rw [hb] at hb'; cases hb'
      obtain ⟨hby, st, l', hp, hw'⟩ := hi.bind w' a hb'
      refine ⟨hby, st, l', by simpa [Integrated.State.setWorker, Integrated.State.setLane] using hp, ?_⟩
      simpa [Integrated.State.setWorker, Integrated.State.setLane, hne] using hw'
    · intro l' w' he
      by_cases hll : l' = l
      · subst hll
        simp only [Integrated.State.setWorker, Integrated.State.setLane, ↓reduceIte, Integrated.laneEntered] at he
        by_cases hww : w' = w
        · subst hww; rw [hf] at he; cases he
        · simpa [Integrated.State.setWorker, Integrated.State.setLane, hww] using hi.ended l' w' he
      · simp only [Integrated.State.setWorker, Integrated.State.setLane, hll, ↓reduceIte] at he
        by_cases hww : w' = w
        · subst hww
          simp [Integrated.State.setWorker, Integrated.State.setLane, Ne.symm hll]
        · simpa [Integrated.State.setWorker, Integrated.State.setLane, hww] using hi.ended l' w' he
    · intro w' hw'
      by_cases hww : w' = w
      · subst hww; simp [Integrated.State.setWorker] at hw'
      · exact hi.rested w' (by simpa [Integrated.State.setWorker, Integrated.State.setLane, hww] using hw')
  · have e1 : estep n e (.enter w l) = e := by simp only [estep, hc, ↓reduceIte]
    rw [e1]
    exact hi

theorem inv_pop (e : E α W) (hi : EInv jobs e) (w a : Nat) : EInv jobs (estep n e (.pop w a)) := by
  cases hw : e.host.worker w with
  | inPass l =>
    cases hs : e.host.slot a with
    | pushed stamp intact =>
      by_cases hb : e.bound w = none
      · have hnot : e.boundBy a = none := by
          cases hby : e.boundBy a with
          | none => rfl
          | some w' =>
            obtain ⟨_, _, hp⟩ := hi.once a w' hby
            rw [hs] at hp
            cases hp
        have hostInv := Integrated.step_inv hi.host (Integrated.Step.pop (n := n) w l a stamp intact hw hs)
        have hother : ∀ w' a', e.bound w' = some a' → a' ≠ a := fun w' a' hb' he => by
          subst he
          rw [(hi.bind w' a' hb').1] at hnot
          cases hnot
        cases intact with
        | false =>
          have e1 : estep n e (.pop w a) = { e with host := e.host.setSlot a (.popped stamp l false) } := by
            simp [estep, hw, hs, hb]
          rw [e1]
          refine ⟨hostInv, hi.lin, hi.call, ?_, ?_, hi.idle, hi.done, hi.ended, hi.rested⟩
          · intro w' a' hb'
            obtain ⟨hby, st, l', hp, hw'⟩ := hi.bind w' a' hb'
            exact ⟨hby, st, l', by simp [Integrated.State.setSlot, hother w' a' hb', hp], hw'⟩
          · intro a' w' hby
            have hne : a' ≠ a := fun h => by subst h; rw [hnot] at hby; cases hby
            obtain ⟨st, l', hp⟩ := hi.once a' w' hby
            exact ⟨st, l', by simp [Integrated.State.setSlot, hne, hp]⟩
        | true =>
          have e1 : estep n e (.pop w a) = { e with
              host := e.host.setSlot a (.popped stamp l true)
              bound := update e.bound w (some a)
              boundBy := update e.boundBy a (some w) } := by
            simp [estep, hw, hs, hb]
          rw [e1]
          refine ⟨hostInv, hi.lin, hi.call, ?_, ?_, ?_, ?_, hi.ended, ?_⟩
          · intro w' a' hb'
            by_cases hww : w' = w
            · subst hww
              simp at hb'
              subst hb'
              exact ⟨by simp, stamp, l, by simp [Integrated.State.setSlot], Or.inl (by simpa [Integrated.State.setSlot] using hw)⟩
            · simp [hww] at hb'
              obtain ⟨hby, st, l', hp, hw'⟩ := hi.bind w' a' hb'
              have hne := hother w' a' hb'
              exact ⟨by simp [hne, hby], st, l', by simp [Integrated.State.setSlot, hne, hp], hw'⟩
          · intro a' w' hby
            by_cases haa : a' = a
            · subst haa; exact ⟨stamp, l, by simp [Integrated.State.setSlot]⟩
            · simp [haa] at hby
              obtain ⟨st, l', hp⟩ := hi.once a' w' hby
              exact ⟨st, l', by simp [Integrated.State.setSlot, haa, hp]⟩
          · intro a' hby
            have haa : a' ≠ a := fun h => by subst h; simp at hby
            simp [haa] at hby
            exact hi.idle a' hby
          · intro a' w' hby hnb
            by_cases haa : a' = a
            · subst haa
              simp at hby
              subst hby
              simp at hnb
            · simp [haa] at hby
              apply hi.done a' w' hby
              by_cases hww : w' = w
              · subst hww; rw [hb]; simp
              · simpa [hww] using hnb
          · intro w' hw'
            by_cases hww : w' = w
            · subst hww
              have hw'' : e.host.worker w' = .idle := hw'
              rw [hw] at hw''
              cases hw''
            · simpa [hww] using hi.rested w' hw'
      · have e1 : estep n e (.pop w a) = e := by simp [estep, hw, hs, hb]
        rw [e1]
        exact hi
    | empty =>
      have e1 : estep n e (.pop w a) = e := by simp [estep, hw, hs]
      rw [e1]
      exact hi
    | popped st l' r =>
      have e1 : estep n e (.pop w a) = e := by simp [estep, hw, hs]
      rw [e1]
      exact hi
    | granted st =>
      have e1 : estep n e (.pop w a) = e := by simp [estep, hw, hs]
      rw [e1]
      exact hi
    | replayed st l' r =>
      have e1 : estep n e (.pop w a) = e := by simp [estep, hw, hs]
      rw [e1]
      exact hi
    | released st l' r =>
      have e1 : estep n e (.pop w a) = e := by simp [estep, hw, hs]
      rw [e1]
      exact hi
  | idle =>
    have e1 : estep n e (.pop w a) = e := by simp [estep, hw]
    rw [e1]
    exact hi
  | dead l =>
    have e1 : estep n e (.pop w a) = e := by simp [estep, hw]
    rw [e1]
    exact hi
  | gone =>
    have e1 : estep n e (.pop w a) = e := by simp [estep, hw]
    rw [e1]
    exact hi

theorem inv_work (hW : 0 < W) (e : E α W) (hi : EInv jobs e) (w : Nat) : EInv jobs (estep n e (.work w)) := by
  cases hw : e.host.worker w with
  | inPass l =>
    cases hb : e.bound w with
    | some a =>
      have e1 : estep n e (.work w) = { e with mem := step e.mem a } := by simp [estep, hw, hb]
      rw [e1]
      have hby := (hi.bind w a hb).1
      refine ⟨hi.host, step_inv hW _ hi.lin a, ?_, hi.bind, hi.once, ?_, ?_, hi.ended, hi.rested⟩
      · intro a'
        by_cases haa : a' = a
        · subst haa
          obtain ⟨he, hk⟩ := step_call e.mem a'
          rw [he, hk]
          exact hi.call a'
        · rw [step_other _ _ _ haa]
          exact hi.call a'
      · intro a' hn
        have haa : a' ≠ a := fun h => by subst h; rw [hby] at hn; cases hn
        rw [step_other _ _ _ haa]
        exact hi.idle a' hn
      · intro a' w' hby' hnb
        by_cases haa : a' = a
        · subst haa
          rw [hby] at hby'
          cases hby'
          exact absurd hb hnb
        · rw [step_other _ _ _ haa]
          exact hi.done a' w' hby' hnb
    | none =>
      have e1 : estep n e (.work w) = e := by simp [estep, hw, hb]
      rw [e1]
      exact hi
  | idle =>
    have e1 : estep n e (.work w) = e := by simp [estep, hw]
    rw [e1]
    exact hi
  | dead l =>
    have e1 : estep n e (.work w) = e := by simp [estep, hw]
    rw [e1]
    exact hi
  | gone =>
    have e1 : estep n e (.work w) = e := by simp [estep, hw]
    rw [e1]
    exact hi

theorem inv_finish (e : E α W) (hi : EInv jobs e) (w : Nat) : EInv jobs (estep n e (.finish w)) := by
  cases hb : e.bound w with
  | some a =>
    by_cases hd : (e.mem.threads a).phase.isDone = true
    · have e1 : estep n e (.finish w) = { e with bound := update e.bound w none } := by simp [estep, hb, hd]
      rw [e1]
      refine ⟨hi.host, hi.lin, hi.call, ?_, hi.once, hi.idle, ?_, hi.ended, ?_⟩
      · intro w' a' hb'
        have hww : w' ≠ w := fun h => by subst h; simp at hb'
        simp [hww] at hb'
        exact hi.bind w' a' hb'
      · intro a' w' hby hnb
        by_cases hww : w' = w
        · subst hww
          by_cases haa : a' = a
          · subst haa; exact hd
          · apply hi.done a' w' hby
            rw [hb]
            simpa using Ne.symm haa
        · simp [hww] at hnb
          exact hi.done a' w' hby hnb
      · intro w' hw'
        by_cases hww : w' = w
        · subst hww; simp
        · simpa [hww] using hi.rested w' hw'
    · have e1 : estep n e (.finish w) = e := by simp [estep, hb, hd]
      rw [e1]
      exact hi
  | none =>
    have e1 : estep n e (.finish w) = e := by simp [estep, hb]
    rw [e1]
    exact hi

theorem inv_leave (e : E α W) (hi : EInv jobs e) (w : Nat) : EInv jobs (estep n e (.leave w)) := by
  cases hw : e.host.worker w with
  | inPass l =>
    by_cases hb : e.bound w = none
    · have e1 : estep n e (.leave w) =
          { e with host := (e.host.setLane l (Integrated.laneLeft (e.host.lane l) w)).setWorker w .idle } := by
        simp [estep, hw, hb]
      rw [e1]
      refine ⟨Integrated.step_inv hi.host (Integrated.Step.leave (n := n) w l hw), hi.lin, hi.call, ?_, ?_, hi.idle,
        hi.done, ?_, ?_⟩
      · intro w' a hb'
        have hww : w' ≠ w := fun h => by subst h; rw [hb] at hb'; cases hb'
        obtain ⟨hby, st, l', hp, hw'⟩ := hi.bind w' a hb'
        refine ⟨hby, st, l', by simpa [Integrated.State.setWorker, Integrated.State.setLane] using hp, ?_⟩
        simpa [Integrated.State.setWorker, Integrated.State.setLane, hww] using hw'
      · intro a w' hby
        obtain ⟨st, l', hp⟩ := hi.once a w' hby
        exact ⟨st, l', by simpa [Integrated.State.setWorker, Integrated.State.setLane] using hp⟩
      · intro l' w' he
        by_cases hww : w' = w
        · subst hww; simp [Integrated.State.setWorker]
        · have he' : (e.host.lane l').ended w' = true := by
            by_cases hll : l' = l
            · subst hll
              simpa [Integrated.State.setWorker, Integrated.State.setLane, Integrated.laneLeft, hww] using he
            · simpa [Integrated.State.setWorker, Integrated.State.setLane, hll] using he
          simpa [Integrated.State.setWorker, Integrated.State.setLane, hww] using hi.ended l' w' he'
      · intro w' hw'
        by_cases hww : w' = w
        · subst hww; exact hb
        · exact hi.rested w' (by simpa [Integrated.State.setWorker, Integrated.State.setLane, hww] using hw')
    · have e1 : estep n e (.leave w) = e := by simp [estep, hw, hb]
      rw [e1]
      exact hi
  | idle =>
    have e1 : estep n e (.leave w) = e := by simp [estep, hw]
    rw [e1]
    exact hi
  | dead l =>
    have e1 : estep n e (.leave w) = e := by simp [estep, hw]
    rw [e1]
    exact hi
  | gone =>
    have e1 : estep n e (.leave w) = e := by simp [estep, hw]
    rw [e1]
    exact hi

theorem inv_kill (e : E α W) (hi : EInv jobs e) (w : Nat) : EInv jobs (estep n e (.kill w)) := by
  cases hw : e.host.worker w with
  | inPass l =>
    have e1 : estep n e (.kill w) = { e with host := e.host.setWorker w (.dead l) } := by simp [estep, hw]
    rw [e1]
    refine ⟨Integrated.step_inv hi.host (Integrated.Step.kill (n := n) w l hw), hi.lin, hi.call, ?_, hi.once, hi.idle,
      hi.done, ?_, ?_⟩
    · intro w' a hb'
      obtain ⟨hby, st, l', hp, hw'⟩ := hi.bind w' a hb'
      refine ⟨hby, st, l', hp, ?_⟩
      by_cases hww : w' = w
      · subst hww
        rw [hw] at hw'
        rcases hw' with h | h | h <;> cases h
        right; left; simp [Integrated.State.setWorker]
      · simpa [Integrated.State.setWorker, hww] using hw'
    · intro l' w' he
      by_cases hww : w' = w
      · subst hww
        have hll : l' ≠ l := fun h => by subst h; exact (hi.ended l' w' he).1 hw
        simp [Integrated.State.setWorker, Ne.symm hll]
      · simpa [Integrated.State.setWorker, hww] using hi.ended l' w' he
    · intro w' hw'
      by_cases hww : w' = w
      · subst hww; simp [Integrated.State.setWorker] at hw'
      · exact hi.rested w' (by simpa [Integrated.State.setWorker, hww] using hw')
  | idle =>
    have e1 : estep n e (.kill w) = e := by simp [estep, hw]
    rw [e1]
    exact hi
  | dead l =>
    have e1 : estep n e (.kill w) = e := by simp [estep, hw]
    rw [e1]
    exact hi
  | gone =>
    have e1 : estep n e (.kill w) = e := by simp [estep, hw]
    rw [e1]
    exact hi

/-- The calls after the free of a dead worker: the supervisor finishes the call that the worker ran. The invariant of
the calls holds, and each thread does the call of its slot. Each call but the finished one stays as it was. -/
theorem free_mem (hW : 0 < W) (e : E α W) (hi : EInv jobs e) (w : Nat) :
    Inv (match e.bound w with | some a => finish e.mem a | none => e.mem) ∧
      (∀ a, ((match e.bound w with | some a => finish e.mem a | none => e.mem).threads a).elem = (jobs a).elem ∧
        ((match e.bound w with | some a => finish e.mem a | none => e.mem).threads a).kind = (jobs a).kind) ∧
      (∀ a, (e.bound w ≠ some a) →
        (match e.bound w with | some a => finish e.mem a | none => e.mem).threads a = e.mem.threads a) := by
  cases hb : e.bound w with
  | none => exact ⟨hi.lin, hi.call, fun _ _ => rfl⟩
  | some a0 =>
    refine ⟨finish_inv hW _ hi.lin a0, fun a => ?_, fun a ha => ?_⟩
    · by_cases haa : a = a0
      · subst haa
        obtain ⟨he, hk⟩ := finish_call e.mem a
        rw [he, hk]
        exact hi.call a
      · rw [finish_other _ _ _ haa]
        exact hi.call a
    · have haa : a ≠ a0 := fun h => ha (by rw [h])
      exact finish_other _ _ _ haa

theorem inv_free (hW : 0 < W) (e : E α W) (hi : EInv jobs e) (w : Nat) : EInv jobs (estep n e (.free w)) := by
  cases hw : e.host.worker w with
  | dead l =>
    have e1 : estep n e (.free w) =
        { e with
          host := (e.host.setLane l (Integrated.laneLeft (e.host.lane l) w)).setWorker w .gone
          mem := match e.bound w with | some a => finish e.mem a | none => e.mem } := by
      simp [estep, hw]
    rw [e1]
    obtain ⟨hlin, hcall, hkeep⟩ := free_mem jobs hW e hi w
    refine ⟨Integrated.step_inv hi.host (Integrated.Step.free (n := n) w l hw), hlin, hcall, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro w' a hb'
      obtain ⟨hby, st, l', hp, hw'⟩ := hi.bind w' a hb'
      refine ⟨hby, st, l', by simpa [Integrated.State.setWorker, Integrated.State.setLane] using hp, ?_⟩
      by_cases hww : w' = w
      · subst hww; right; right; simp [Integrated.State.setWorker]
      · simpa [Integrated.State.setWorker, Integrated.State.setLane, hww] using hw'
    · intro a w' hby
      obtain ⟨st, l', hp⟩ := hi.once a w' hby
      exact ⟨st, l', by simpa [Integrated.State.setWorker, Integrated.State.setLane] using hp⟩
    · intro a hn
      have hne : e.bound w ≠ some a := fun h => by rw [(hi.bind w a h).1] at hn; cases hn
      rw [hkeep a hne]
      exact hi.idle a hn
    · intro a w' hby hnb
      have hne : e.bound w ≠ some a := by
        intro h
        have hw2 := (hi.bind w a h).1
        rw [hby] at hw2
        cases hw2
        exact hnb h
      rw [hkeep a hne]
      exact hi.done a w' hby hnb
    · intro l' w' he
      by_cases hww : w' = w
      · subst hww; simp [Integrated.State.setWorker]
      · have he' : (e.host.lane l').ended w' = true := by
          by_cases hll : l' = l
          · subst hll
            simpa [Integrated.State.setWorker, Integrated.State.setLane, Integrated.laneLeft, hww] using he
          · simpa [Integrated.State.setWorker, Integrated.State.setLane, hll] using he
        simpa [Integrated.State.setWorker, Integrated.State.setLane, hww] using hi.ended l' w' he'
    · intro w' hw'
      by_cases hww : w' = w
      · subst hww; simp [Integrated.State.setWorker] at hw'
      · exact hi.rested w' (by simpa [Integrated.State.setWorker, Integrated.State.setLane, hww] using hw')
  | idle =>
    have e1 : estep n e (.free w) = e := by simp [estep, hw]
    rw [e1]
    exact hi
  | inPass l =>
    have e1 : estep n e (.free w) = e := by simp [estep, hw]
    rw [e1]
    exact hi
  | gone =>
    have e1 : estep n e (.free w) = e := by simp [estep, hw]
    rw [e1]
    exact hi

/-- The free of a dead worker finishes its call. So the call holds no claim after the free, and its element is free
for each other call. -/
theorem freed_holds_nothing (n : Nat) (e : E α W) (w a l : Nat) (hw : e.host.worker w = .dead l)
    (hb : e.bound w = some a) : ((estep n e (.free w)).mem.threads a).phase.holds = false := by
  have e1 : (estep n e (.free w)).mem = finish e.mem a := by simp [estep, hw, hb]
  rw [e1]
  exact finish_releases e.mem a

/-- Each step keeps the invariant. -/
theorem estep_inv (hW : 0 < W) (e : E α W) (hi : EInv jobs e) (op : Op) : EInv jobs (estep n e op) := by
  cases op with
  | grant w a => exact inv_grant n jobs e hi w a
  | push w a intact => exact inv_push n jobs e hi w a intact
  | enter w l => exact inv_enter n jobs e hi w l
  | pop w a => exact inv_pop n jobs e hi w a
  | work w => exact inv_work n jobs hW e hi w
  | finish w => exact inv_finish n jobs e hi w
  | leave w => exact inv_leave n jobs e hi w
  | kill w => exact inv_kill n jobs e hi w
  | free w => exact inv_free n jobs hW e hi w

/-- Each run keeps the invariant, for each count of lanes, producers and workers, and each order of steps. -/
theorem erun_inv (hW : 0 < W) (e : E α W) (hi : EInv jobs e) (ops : List Op) : EInv jobs (erun n e ops) := by
  induction ops generalizing e with
  | nil => exact hi
  | cons op rest ih => exact ih _ (estep_inv n jobs hW e hi op)

end Steps

/-! ## The lanes -/

/-- Each step changes each lane word by one step of the drain model. -/
theorem estep_lane (n : Nat) (jobs : Nat → Job α) (e : E α W) (hi : EInv jobs e) (op : Op) (l0 : Nat) :
    Manifold.Drain.Step (e.host.lane l0) ((estep n e op).host.lane l0) := by
  cases op with
  | grant w a =>
    simp only [estep]
    split
    · split
      · exact .sleeper
      · exact .sleeper
    · exact .sleeper
  | push w a intact =>
    simp only [estep]
    split
    · exact .sleeper
    · exact .sleeper
  | enter w l =>
    by_cases hc : l < n ∧ e.host.worker w = .idle ∧ (e.host.lane l).busy = false ∧ (e.host.lane l).ended w = false
    · simp only [estep, hc, and_self, ↓reduceIte]
      obtain ⟨_, _, hl, hf⟩ := hc
      by_cases hll : l0 = l
      · subst hll
        simp only [Integrated.State.setWorker, Integrated.State.setLane, ↓reduceIte]
        exact Integrated.entered_is_step _ w hl hf
      · simp only [Integrated.State.setWorker, Integrated.State.setLane, hll, ↓reduceIte]
        exact .sleeper
    · simp only [estep, hc, ↓reduceIte]; exact .sleeper
  | pop w a =>
    simp only [estep]
    split
    · split
      · exact .sleeper
      · exact .sleeper
    · exact .sleeper
  | work w =>
    simp only [estep]
    split
    · exact .sleeper
    · exact .sleeper
  | finish w =>
    simp only [estep]
    split
    · split
      · exact .sleeper
      · exact .sleeper
    · exact .sleeper
  | leave w =>
    cases hw : e.host.worker w with
    | inPass l =>
      by_cases hb : e.bound w = none
      · simp only [estep, hw, hb, ↓reduceIte]
        by_cases hll : l0 = l
        · subst hll
          simp only [Integrated.State.setWorker, Integrated.State.setLane, ↓reduceIte]
          exact Integrated.left_is_step _ w (hi.host.holds w l0 (Or.inl hw)).2
        · simp only [Integrated.State.setWorker, Integrated.State.setLane, hll, ↓reduceIte]
          exact .sleeper
      · simp only [estep, hw, hb, ↓reduceIte]; exact .sleeper
    | idle => simp only [estep, hw]; exact .sleeper
    | dead l => simp only [estep, hw]; exact .sleeper
    | gone => simp only [estep, hw]; exact .sleeper
  | kill w =>
    cases hw : e.host.worker w with
    | inPass l => simp only [estep, hw]; exact .sleeper
    | idle => simp only [estep, hw]; exact .sleeper
    | dead l => simp only [estep, hw]; exact .sleeper
    | gone => simp only [estep, hw]; exact .sleeper
  | free w =>
    cases hw : e.host.worker w with
    | dead l =>
      simp only [estep, hw]
      by_cases hll : l0 = l
      · subst hll
        simp only [Integrated.State.setWorker, Integrated.State.setLane, ↓reduceIte]
        exact Integrated.left_is_step _ w (hi.host.holds w l0 (Or.inr hw)).2
      · simp only [Integrated.State.setWorker, Integrated.State.setLane, hll, ↓reduceIte]
        exact .sleeper
    | idle => simp only [estep, hw]; exact .sleeper
    | inPass l => simp only [estep, hw]; exact .sleeper
    | gone => simp only [estep, hw]; exact .sleeper

theorem follows_cons {s t u : Manifold.Drain.State} (st : Manifold.Drain.Step s t) (f : Manifold.Drain.Follows t u) :
    Manifold.Drain.Follows s u := by
  induction f with
  | refl => exact .step (.refl s) st
  | step _ st' ih => exact .step ih st'

/-- A run changes each lane word through steps of the drain model. So each theorem of the drain applies to each lane
end to end. -/
theorem erun_lane (hW : 0 < W) (n : Nat) (jobs : Nat → Job α) (e : E α W) (hi : EInv jobs e) (ops : List Op)
    (l : Nat) : Manifold.Drain.Follows (e.host.lane l) ((erun n e ops).host.lane l) := by
  induction ops generalizing e with
  | nil => exact .refl _
  | cons op rest ih =>
    exact follows_cons (estep_lane n jobs e hi op l) (ih _ (estep_inv n jobs hW e hi op))

/-- Each step of the drain model on a lane word is a run of the futex model on that word. The sleeper and its wake
stay as they were. -/
theorem lane_wakes {d d' : Manifold.Drain.State} (st : Manifold.Drain.Step d d') (ws : Manifold.Wake.State)
    (hr : Manifold.Wake.Reach ws) (hb : ws.word.busy = d.busy) (hg : ws.word.generation = d.generation) :
    ∃ ws', Manifold.Wake.Reach ws' ∧ ws'.word.busy = d'.busy ∧ ws'.word.generation = d'.generation ∧
      ws'.sleeper = ws.sleeper ∧ ws'.owed = ws.owed := by
  cases st with
  | enter p h _ =>
    exact ⟨_, .step hr (.enter (by rw [hb]; exact h)), rfl, hg, rfl, rfl⟩
  | refused h => exact ⟨ws, hr, hb, hg, rfl, rfl⟩
  | leave p h => exact ⟨_, .step hr (.change false), rfl, by simp [hg], rfl, rfl⟩
  | claim h => exact ⟨_, .step hr (.enter (by rw [hb]; exact h)), rfl, hg, rfl, rfl⟩
  | shut _ _ => exact ⟨_, .step hr (.change d.busy), rfl, by simp [hg], rfl, rfl⟩
  | free _ _ => exact ⟨_, .step hr (.change false), rfl, by simp [hg], rfl, rfl⟩
  | takeover _ _ => exact ⟨_, .step hr (.change d.busy), rfl, by simp [hg], rfl, rfl⟩
  | adopt _ _ _ _ _ => exact ⟨ws, hr, hb, hg, rfl, rfl⟩
  | sleeper => exact ⟨ws, hr, hb, hg, rfl, rfl⟩

/-- Each run end to end is a run of the futex model on each lane word, with the sleeper and its wake as they were. So
`EmbeddedTypes.Manifold.Wake.no_lost_wake` holds for a drain that sleeps on the word of the lane. -/
theorem erun_wakes (hW : 0 < W) (n : Nat) (jobs : Nat → Job α) (e : E α W) (hi : EInv jobs e) (ops : List Op)
    (l : Nat) (ws : Manifold.Wake.State) (hr : Manifold.Wake.Reach ws) (hb : ws.word.busy = (e.host.lane l).busy)
    (hg : ws.word.generation = (e.host.lane l).generation) :
    ∃ ws', Manifold.Wake.Reach ws' ∧ ws'.word.busy = ((erun n e ops).host.lane l).busy ∧
      ws'.word.generation = ((erun n e ops).host.lane l).generation ∧ ws'.sleeper = ws.sleeper ∧
      ws'.owed = ws.owed := by
  induction ops generalizing e ws with
  | nil => exact ⟨ws, hr, hb, hg, rfl, rfl⟩
  | cons op rest ih =>
    obtain ⟨ws1, hr1, hb1, hg1, hs1, ho1⟩ := lane_wakes (estep_lane n jobs e hi op l) ws hr hb hg
    obtain ⟨ws2, hr2, hb2, hg2, hs2, ho2⟩ := ih _ (estep_inv n jobs hW e hi op) ws1 hr1 hb1 hg1
    exact ⟨ws2, hr2, hb2, hg2, hs2.trans hs1, ho2.trans ho1⟩

/-! ## What the run gives -/

/-- A popped slot stays popped. -/
theorem estep_slot (n : Nat) (e : E α W) (op : Op) (a stamp l : Nat) (r : Bool)
    (h : e.host.slot a = .popped stamp l r) : (estep n e op).host.slot a = .popped stamp l r := by
  cases op with
  | grant w a' =>
    simp only [estep]
    split
    · split
      · next hs =>
        have hne : a ≠ a' := fun he => by subst he; rw [h] at hs; cases hs
        simp [Integrated.State.setSlot, hne, h]
      · exact h
    · exact h
  | push w a' intact =>
    simp only [estep]
    split
    · next st _ hs =>
      have hne : a ≠ a' := fun he => by subst he; rw [h] at hs; cases hs
      simp [Integrated.State.setSlot, hne, h]
    · exact h
  | enter w l' =>
    simp only [estep]
    split
    · simpa [Integrated.State.setWorker, Integrated.State.setLane] using h
    · exact h
  | pop w a' =>
    simp only [estep]
    split
    · next l' stamp' intact hw hs =>
      split
      · have hne : a ≠ a' := fun he => by subst he; rw [h] at hs; cases hs
        simp [Integrated.State.setSlot, hne, h]
      · exact h
    · exact h
  | work w =>
    simp only [estep]
    split
    · exact h
    · exact h
  | finish w =>
    simp only [estep]
    split
    · split
      · exact h
      · exact h
    · exact h
  | leave w =>
    simp only [estep]
    split
    · split
      · simpa [Integrated.State.setWorker, Integrated.State.setLane] using h
      · exact h
    · exact h
  | kill w =>
    simp only [estep]
    split
    · simpa [Integrated.State.setWorker] using h
    · exact h
  | free w =>
    simp only [estep]
    split
    · simpa [Integrated.State.setWorker, Integrated.State.setLane] using h
    · exact h

/-- The worker that popped a slot stays the one that popped it. -/
theorem estep_boundBy (n : Nat) (jobs : Nat → Job α) (e : E α W) (hi : EInv jobs e) (op : Op) (a w : Nat)
    (h : e.boundBy a = some w) : (estep n e op).boundBy a = some w := by
  cases op with
  | pop w' a' =>
    simp only [estep]
    split
    · next l' stamp' intact hw hs =>
      split
      · have hne : a ≠ a' := fun he => by
          subst he
          obtain ⟨_, _, hp⟩ := hi.once a w h
          rw [hp] at hs
          cases hs
        cases intact <;> simp [hne, h]
      · exact h
    · exact h
  | grant _ _ => simp only [estep]; split <;> (try split) <;> exact h
  | push _ _ _ => simp only [estep]; split <;> exact h
  | enter _ _ => simp only [estep]; split <;> exact h
  | work _ => simp only [estep]; split <;> exact h
  | finish _ => simp only [estep]; split <;> (try split) <;> exact h
  | leave _ => simp only [estep]; split <;> (try split) <;> exact h
  | kill _ => simp only [estep]; split <;> exact h
  | free _ => simp only [estep]; split <;> exact h

theorem erun_boundBy (hW : 0 < W) (n : Nat) (jobs : Nat → Job α) (e : E α W) (hi : EInv jobs e) (ops : List Op)
    (a w : Nat) (h : e.boundBy a = some w) : (erun n e ops).boundBy a = some w := by
  induction ops generalizing e with
  | nil => exact h
  | cons op rest ih => exact ih _ (estep_inv n jobs hW e hi op) (estep_boundBy n jobs e hi op a w h)

theorem erun_slot (n : Nat) (e : E α W) (ops : List Op) (a stamp l : Nat) (r : Bool)
    (h : e.host.slot a = .popped stamp l r) : (erun n e ops).host.slot a = .popped stamp l r := by
  induction ops generalizing e with
  | nil => exact h
  | cons op rest ih => exact ih _ (estep_slot n e op a stamp l r h)

/-- A call whose job pointer failed its check never takes a step: its thread stays before its read. -/
theorem refused_never_runs (jobs : Nat → Job α) (e : E α W) (hi : EInv jobs e) (a stamp l : Nat)
    (h : e.host.slot a = .popped stamp l false) : (e.mem.threads a).phase = .idle := by
  cases hby : e.boundBy a with
  | none => exact hi.idle a hby
  | some w =>
    obtain ⟨_, _, hp⟩ := hi.once a w hby
    rw [hp] at h
    cases h

/-- One worker at max runs the call of a slot: the worker that popped it stays the one for each later state. -/
theorem call_once (hW : 0 < W) (n : Nat) (jobs : Nat → Job α) (cells : Nat → α) (ops more : List Op) (a w : Nat)
    (h : (erun n (estart jobs cells : E α W) ops).boundBy a = some w) :
    (erun n (erun n (estart jobs cells : E α W) ops) more).boundBy a = some w :=
  erun_boundBy hW n jobs _ (erun_inv n jobs hW _ (estart_inv jobs cells) ops) more a w h

/-- A worker that runs a call in its pass holds its lane, busy, and the slot names that lane. -/
theorem call_in_pass (jobs : Nat → Job α) (e : E α W) (hi : EInv jobs e) (w a l : Nat) (hb : e.bound w = some a)
    (hw : e.host.worker w = .inPass l) :
    (e.host.lane l).busy = true ∧ (e.host.lane l).holder = some w ∧ ∃ stamp, e.host.slot a = .popped stamp l true := by
  obtain ⟨hbusy, hhold⟩ := hi.host.holds w l (Or.inl hw)
  obtain ⟨_, stamp, l', hp, hw'⟩ := hi.bind w a hb
  have hll : l' = l := by
    rw [hw] at hw'
    rcases hw' with h | h | h <;> cases h
    rfl
  subst hll
  exact ⟨hbusy, hhold, stamp, hp⟩

/-- A drain reads the lane of a call in progress, and reads the lane over after a run. It then finds the call done,
or its worker freed by the supervisor. So a writer that drains does not overlap a call. -/
theorem drain_waits_for_call (hW : 0 < W) (n : Nat) (jobs : Nat → Job α) (e : E α W) (hi : EInv jobs e)
    (ops : List Op) (w a l : Nat) (hb : e.bound w = some a) (hw : e.host.worker w = .inPass l)
    (ho : Manifold.Drain.over (e.host.lane l) ((erun n e ops).host.lane l) = true) :
    ((erun n e ops).mem.threads a).phase.isDone = true ∨ (erun n e ops).host.worker w = .gone := by
  obtain ⟨_, hhold, stamp, hp⟩ := call_in_pass jobs e hi w a l hb hw
  have hend := Manifold.Drain.over_means_ended w (hi.host.lane l) hhold (erun_lane hW n jobs e hi ops l) ho
  have ht := erun_inv n jobs hW e hi ops
  have hby := erun_boundBy hW n jobs e hi ops a w (hi.bind w a hb).1
  have hslot := erun_slot n e ops a stamp l true hp
  by_cases hbt : (erun n e ops).bound w = some a
  · obtain ⟨_, stamp', l', hp', hw'⟩ := ht.bind w a hbt
    rw [hslot] at hp'
    cases hp'
    obtain ⟨hni, hnd⟩ := ht.ended l w hend
    rcases hw' with h | h | h
    · exact absurd h hni
    · exact absurd h hnd
    · exact Or.inr h
  · exact Or.inl (ht.done a w hby hbt)

/-- No two calls hold one element at the same time, end to end. -/
theorem exclusive (hW : 0 < W) (n : Nat) (jobs : Nat → Job α) (cells : Nat → α) (ops : List Op) (u v : Nat)
    (huv : u ≠ v) (hu : ((erun n (estart jobs cells : E α W) ops).mem.threads u).phase.holds = true)
    (hv : ((erun n (estart jobs cells : E α W) ops).mem.threads v).phase.holds = true) :
    ((erun n (estart jobs cells : E α W) ops).mem.threads u).elem ≠
      ((erun n (estart jobs cells : E α W) ops).mem.threads v).elem :=
  fun he => huv ((erun_inv n jobs hW _ (estart_inv jobs cells) ops).lin.apart u v hu hv he)

/-! ## The sequential order end to end -/

/-- Each step end to end leaves the calls as they were, or takes one step of one call. -/
theorem estep_mem (n : Nat) (e : E α W) (op : Op) : ∃ sched, (estep n e op).mem = run e.mem sched := by
  cases op with
  | work w =>
    simp only [estep]
    split
    · next a _ _ => exact ⟨[a], rfl⟩
    · exact ⟨[], rfl⟩
  | free w =>
    simp only [estep]
    split
    · split
      · next a _ =>
        rcases finish_steps e.mem a with h | h | h
        · exact ⟨[], h⟩
        · exact ⟨[a], h⟩
        · exact ⟨[a, a], h⟩
      · exact ⟨[], rfl⟩
    · exact ⟨[], rfl⟩
  | grant _ _ => simp only [estep]; split <;> (try split) <;> exact ⟨[], rfl⟩
  | push _ _ _ => simp only [estep]; split <;> exact ⟨[], rfl⟩
  | enter _ _ => simp only [estep]; split <;> exact ⟨[], rfl⟩
  | pop _ _ => simp only [estep]; split <;> (try split) <;> exact ⟨[], rfl⟩
  | finish _ => simp only [estep]; split <;> (try split) <;> exact ⟨[], rfl⟩
  | leave _ => simp only [estep]; split <;> (try split) <;> exact ⟨[], rfl⟩
  | kill _ => simp only [estep]; split <;> exact ⟨[], rfl⟩

/-- Each run end to end keeps the invariant of the order of `Linearizable.lean`. -/
theorem erun_loginv (n : Nat) (calls : Nat → Thread α W) (init : Nat → α) (e : E α W)
    (hl : LogInv calls init e.mem) (ops : List Op) : LogInv calls init (erun n e ops).mem := by
  induction ops generalizing e with
  | nil => exact hl
  | cons op rest ih =>
    apply ih
    obtain ⟨sched, h⟩ := estep_mem n e op
    rw [h]
    exact run_loginv calls init e.mem hl sched

/-- The calls of the start, each before its read. -/
def callsOf (jobs : Nat → Job α) : Nat → Thread α W := fun a => ⟨(jobs a).elem, (jobs a).kind, .idle⟩

/-- End to end, the run is linearizable. The model is the calls of the log replayed one at a time, and each call is
in the log once at max. A reader after its work gives the replay of the calls before it. A call that ended before
another call made its claim comes first in the log. -/
theorem linearizable (hW : 0 < W) (n : Nat) (jobs : Nat → Job α) (cells : Nat → α) (ops more : List Op) :
    (erun n (estart jobs cells : E α W) ops).mem.model =
        replay (callsOf (W := W) jobs) cells (erun n (estart jobs cells : E α W) ops).mem.log ∧
      (erun n (estart jobs cells : E α W) ops).mem.log.Nodup ∧
      (∀ k r, (jobs k).kind = .get → ((erun n (estart jobs cells : E α W) ops).mem.threads k).phase.result = some r →
        r = some (replay (callsOf (W := W) jobs) cells (before (erun n (estart jobs cells : E α W) ops).mem.log k)
          (jobs k).elem)) ∧
      (∀ a b, ((erun n (estart jobs cells : E α W) ops).mem.threads a).phase.isDone = true →
        ((erun n (estart jobs cells : E α W) ops).mem.threads b).phase.before = true →
        ∃ pre post, (erun n (erun n (estart jobs cells : E α W) ops) more).mem.log = pre ++ post ∧ a ∈ pre ∧
          b ∉ pre) := by
  have h0 : LogInv (callsOf jobs) cells (estart jobs cells : E α W).mem := start_log cells _ fun _ => rfl
  have hl := erun_loginv n (callsOf jobs) cells _ h0 ops
  have hi := erun_inv n jobs hW _ (estart_inv jobs cells) ops
  refine ⟨hl.model, hl.nodup, ?_, ?_⟩
  · intro k r hk hr
    have hk' : ((erun n (estart jobs cells : E α W) ops).mem.threads k).kind = .get := by
      rw [(hl.call k).2]; exact hk
    have hm : k ∈ (erun n (estart jobs cells : E α W) ops).mem.log := by
      rw [hl.mem k]
      cases hp : ((erun n (estart jobs cells : E α W) ops).mem.threads k).phase <;>
        simp_all [Phase.result, Phase.before]
    rw [hi.lin.result k r hk' hr, hl.reads k hk' hm, (hl.call k).1]
    rfl
  · intro a b ha hb
    obtain ⟨more', hm⟩ : ∃ more', (erun n (erun n (estart jobs cells : E α W) ops) more).mem.log =
        (erun n (estart jobs cells : E α W) ops).mem.log ++ more' := by
      have hlog : ∀ (e : E α W) (ops : List Op), ∃ m, (erun n e ops).mem.log = e.mem.log ++ m := by
        intro e ops
        induction ops generalizing e with
        | nil => exact ⟨[], by simp [erun]⟩
        | cons op rest ih =>
          obtain ⟨m, hm⟩ := ih (estep n e op)
          obtain ⟨sched, h⟩ := estep_mem n e op
          obtain ⟨m', hm'⟩ := run_log_prefix e.mem sched
          refine ⟨m' ++ m, ?_⟩
          show (erun n (estep n e op) rest).mem.log = _
          rw [hm, h, hm', List.append_assoc]
      exact hlog _ more
    refine ⟨_, more', hm, (hl.mem a).2 (done_not_before _ ha), fun h => ?_⟩
    rw [(hl.mem b).1 h] at hb
    cases hb

/-! ## Two lanes -/

/-- Two writes of element `0`: slot `0` writes `1` and slot `1` writes `2`. -/
def twoWrites : Nat → Job Nat := fun a => if a = 0 then ⟨0, .put 1⟩ else ⟨0, .put 2⟩

/-- Worker `0` on lane `0` grants and pushes slots `0` and `1`. Worker `1` on lane `1` pops slot `1` and does its
call to the end. Then worker `0` pops slot `0` and does its call. -/
def lateFirst : List Op :=
  [.enter 0 0, .enter 1 1, .grant 0 0, .push 0 0 true, .grant 0 1, .push 0 1 true, .pop 1 1, .work 1, .work 1,
    .work 1, .work 1, .finish 1, .leave 1, .pop 0 0, .work 0, .work 0, .work 0, .work 0, .finish 0, .leave 0]

/-- The turnstile gives the passes no order. With two lanes, the call of slot `1` lands before the call of slot `0`,
and element `0` ends at `1`, the write of slot `0`. -/
theorem two_lanes_reorder : (erun 2 (estart twoWrites (fun _ => 0) : E Nat 8) lateFirst).mem.cells 0 = 1 := by
  decide

end LocusCarcerum.EndToEnd
