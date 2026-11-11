/-!
# LocusCarcerum, the address wait the model checker runs

The futex contract, on one word and one waiter, against the wait `genmc_host.h` gives GenMC.

The real wait. The kernel reads the bits of the word it compares and puts the waiter to sleep in one step. That
holds where those bits hold the bits of the value the waiter saw. Where they do not, the wait returns. A wake ends every sleep.
A sleep may also return with no wake: a timeout, a signal, a spurious wake.

The bits compared are `bits`. Windows' WaitOnAddress and macOS's 64-bit compare-and-wait compare the whole word; the
Linux futex compares 32 bits, the low half of the word on a little-endian host. The host's wakes end every sleep:
WakeByAddressAll, a futex wake of INT_MAX, a compare-and-wait wake with the all flag.

The model. The waiter reads the count of wakes, then the word, and returns where the compared bits moved. Otherwise
it either returns at once, a spurious return, or waits until the count moves past the one it read. A timed wait
returns at once.

Every other thread is the environment: a store to the word, or a wake. A wake adds one to the count of wakes.

The theorems:

- `real_in_model` and `model_in_real`: for any compared bits, every state the real wait reaches the model reaches
  with the same word, count and phase of the waiter. Every state the model reaches the real wait reaches. The
  phase is `running` before the wait returns or sleeps, `sleeping` asleep with no wake since, and `done` once the
  wait returned or a wake ended it. The model has every behavior of the contract and no other. A model
  execution blocked asleep is one a real waiter can sleep forever in.
- `whole_in_low`: every state the whole-word wait reaches, the wait that compares fewer bits reaches too. A model
  run on the 32-bit compare holds every host.
- `low_sleeps`: on the 32-bit compare a waiter sleeps on a word whose high half moved, which the whole-word compare
  never does. A model on the whole word would miss that sleep on Linux.
- `one_wake_loses`: a wake that ends one sleep leaves the second of two sleepers asleep, the store it waited for
  made and no wake to come. The host's wakes end every sleep for this reason.

The model is sequentially consistent, as `Trim.lean` and `Boundary.lean` are.
-/

namespace LocusCarcerum.Futex

/-- The phase of the waiter, the part of its state the rest of the program sees. -/
inductive Phase where
  | running
  | sleeping
  | done
  deriving DecidableEq, Repr

/-- The real waiter: before its wait, asleep, or returned. -/
inductive RealWaiter where
  | ready
  | asleep
  | done
  deriving DecidableEq, Repr

/-- The real state: the word, the count of wakes and the waiter. -/
structure Real where
  word : Nat
  wakes : Nat
  waiter : RealWaiter
  deriving DecidableEq, Repr

/-- The model's waiter: before its wait, with the count read, in wait on the count it read, or returned. -/
inductive ModelWaiter where
  | ready
  | read (epoch : Nat)
  | waiting (epoch : Nat)
  | done
  deriving DecidableEq, Repr

/-- The model's state: the word, the count of wakes and the waiter. -/
structure Model where
  word : Nat
  wakes : Nat
  waiter : ModelWaiter
  deriving DecidableEq, Repr

section

variable (bits : Nat → Nat) (seen : Nat)

/-- The real steps. -/
inductive RealStep : Real → Real → Prop where
  /-- Another thread stores to the word. -/
  | store {s : Real} (v : Nat) : RealStep s { s with word := v }
  /-- Another thread wakes: every sleep on the word ends. -/
  | wake {s : Real} :
      RealStep s { s with wakes := s.wakes + 1,
                          waiter := if s.waiter = .asleep then .done else s.waiter }
  /-- The kernel's check and sleep, one step: asleep where the compared bits hold the value seen's. -/
  | check {s : Real} (h : s.waiter = .ready) :
      RealStep s { s with waiter := if bits s.word = bits seen then .asleep else .done }
  /-- A sleep returns with no wake: a timeout, a signal, a spurious wake. -/
  | early {s : Real} (h : s.waiter = .asleep) : RealStep s { s with waiter := .done }

/-- The model's steps. -/
inductive ModelStep : Model → Model → Prop where
  /-- Another thread stores to the word. -/
  | store {s : Model} (v : Nat) : ModelStep s { s with word := v }
  /-- Another thread wakes: the count moves. -/
  | wake {s : Model} : ModelStep s { s with wakes := s.wakes + 1 }
  /-- The waiter reads the count of wakes. -/
  | readCount {s : Model} (h : s.waiter = .ready) : ModelStep s { s with waiter := .read s.wakes }
  /-- The waiter reads the word: it waits where the compared bits hold the value seen's, and returns where not. -/
  | check {s : Model} {e : Nat} (h : s.waiter = .read e) :
      ModelStep s { s with waiter := if bits s.word = bits seen then .waiting e else .done }
  /-- The wait ends once the count moved past the one read: GenMC's assume on the count. -/
  | woken {s : Model} {e : Nat} (h : s.waiter = .waiting e) (hw : s.wakes ≠ e) :
      ModelStep s { s with waiter := .done }
  /-- The spurious return, which the futex contract allows a wait with no end too. -/
  | early {s : Model} {e : Nat} (h : s.waiter = .waiting e) : ModelStep s { s with waiter := .done }
  /-- The timed wait, which returns at once. -/
  | atOnce {s : Model} (h : s.waiter = .ready) : ModelStep s { s with waiter := .done }

/-- The states the real steps reach from a word's first value. -/
inductive RealReach (first : Nat) : Real → Prop where
  | start : RealReach first ⟨first, 0, .ready⟩
  | step {s s' : Real} : RealReach first s → RealStep bits seen s s' → RealReach first s'

/-- The states the model's steps reach from a word's first value. -/
inductive ModelReach (first : Nat) : Model → Prop where
  | start : ModelReach first ⟨first, 0, .ready⟩
  | step {s s' : Model} : ModelReach first s → ModelStep bits seen s s' → ModelReach first s'

end

/-- The real waiter's phase. -/
def Real.phase (s : Real) : Phase :=
  match s.waiter with
  | .ready => .running
  | .asleep => .sleeping
  | .done => .done

/-- The model waiter's phase: a wait whose count moved already is over, though its last step is still to run. -/
def Model.phase (s : Model) : Phase :=
  match s.waiter with
  | .ready => .running
  | .read _ => .running
  | .waiting e => if s.wakes = e then .sleeping else .done
  | .done => .done

/-- The model state a real state maps to: asleep is in wait on the count as it stands. -/
def toModel (s : Real) : Model :=
  ⟨s.word, s.wakes, match s.waiter with
    | .ready => .ready
    | .asleep => .waiting s.wakes
    | .done => .done⟩

/-- The real state a model state maps to. A wait with the count read is still before the kernel's check, and a
wait whose count moved is over. -/
def toReal (s : Model) : Real :=
  ⟨s.word, s.wakes, match s.waiter with
    | .ready => .ready
    | .read _ => .ready
    | .waiting e => if s.wakes = e then .asleep else .done
    | .done => .done⟩

theorem toModel_phase (s : Real) : (toModel s).phase = s.phase := by
  cases s with
  | mk word wakes waiter => cases waiter <;> simp [toModel, Model.phase, Real.phase]

theorem toReal_phase (s : Model) : (toReal s).phase = s.phase := by
  cases s with
  | mk word wakes waiter =>
    cases waiter with
    | waiting e => by_cases h : wakes = e <;> simp [toReal, Model.phase, Real.phase, h]
    | _ => simp [toReal, Model.phase, Real.phase]

/-- The model's invariant: a count a waiter read is never past the count as it stands, as the count only grows. -/
theorem epoch_le {bits : Nat → Nat} {seen first : Nat} {s : Model} (h : ModelReach bits seen first s) :
    ∀ e, (s.waiter = .read e ∨ s.waiter = .waiting e) → e ≤ s.wakes := by
  induction h with
  | start => intro e he; simp at he
  | @step m _ hr st ih =>
    cases st with
    | store v => exact ih
    | wake => intro e he; exact Nat.le_succ_of_le (ih e he)
    | readCount hm => intro e he; simp at he ⊢; omega
    | @check e' hm =>
      intro e he
      have hle := ih e' (Or.inl hm)
      by_cases hb : bits m.word = bits seen <;> simp [hb] at he ⊢ <;> omega
    | woken _ _ => intro e he; simp at he
    | early _ => intro e he; simp at he
    | atOnce _ => intro e he; simp at he

/-- The model reaches every real state, through the map. -/
theorem real_in_model {bits : Nat → Nat} {seen first : Nat} {s : Real} (h : RealReach bits seen first s) :
    ModelReach bits seen first (toModel s) := by
  induction h with
  | start => exact ModelReach.start
  | @step r _ _ st ih =>
    cases st with
    | store v =>
      have := ModelReach.step ih (ModelStep.store (bits := bits) (seen := seen) v)
      cases hw : r.waiter <;> simpa [toModel, hw] using this
    | wake =>
      cases hw : r.waiter with
      | ready =>
        have := ModelReach.step ih (ModelStep.wake (bits := bits) (seen := seen))
        simpa [toModel, hw] using this
      | done =>
        have := ModelReach.step ih (ModelStep.wake (bits := bits) (seen := seen))
        simpa [toModel, hw] using this
      | asleep =>
        have h1 := ModelReach.step ih (ModelStep.wake (bits := bits) (seen := seen))
        have h2 := ModelReach.step h1
          (ModelStep.woken (bits := bits) (seen := seen) (e := r.wakes) (by simp [toModel, hw]) (by simp [toModel]))
        simpa [toModel, hw] using h2
    | check hr =>
      have h1 := ModelReach.step ih (ModelStep.readCount (bits := bits) (seen := seen) (by simp [toModel, hr]))
      have h2 := ModelReach.step h1 (ModelStep.check (bits := bits) (seen := seen) (e := r.wakes) (by simp [toModel]))
      by_cases hs : bits r.word = bits seen <;> simpa [toModel, hs] using h2
    | early hr =>
      have := ModelReach.step ih
        (ModelStep.early (bits := bits) (seen := seen) (e := r.wakes) (by simp [toModel, hr]))
      simpa [toModel] using this

/-- The real wait reaches every model state, through the map. -/
theorem model_in_real {bits : Nat → Nat} {seen first : Nat} {s : Model} (h : ModelReach bits seen first s) :
    RealReach bits seen first (toReal s) := by
  induction h with
  | start => exact RealReach.start
  | @step m _ hr st ih =>
    cases st with
    | store v =>
      have := RealReach.step ih (RealStep.store (bits := bits) (seen := seen) v)
      cases hw : m.waiter <;> simpa [toReal, hw] using this
    | wake =>
      have hstep := RealReach.step ih (RealStep.wake (bits := bits) (seen := seen))
      cases hw : m.waiter with
      | ready => simpa [toReal, hw] using hstep
      | read e => simpa [toReal, hw] using hstep
      | done => simpa [toReal, hw] using hstep
      | waiting e =>
        have hle := epoch_le hr e (Or.inr hw)
        have hne : m.wakes + 1 ≠ e := by omega
        by_cases he : m.wakes = e
        · simpa [toReal, hw, he, hne] using hstep
        · simpa [toReal, hw, he, hne] using hstep
    | readCount hm =>
      simpa [toReal, hm] using ih
    | @check e hm =>
      have h1 := RealReach.step ih (RealStep.check (bits := bits) (seen := seen) (by simp [toReal, hm]))
      by_cases hs : bits m.word = bits seen
      · by_cases he : m.wakes = e
        · simpa [toReal, hm, hs, he] using h1
        · have h2 := RealReach.step h1 (RealStep.early (bits := bits) (seen := seen) (by simp [toReal, hs]))
          simpa [toReal, hs, he] using h2
      · simpa [toReal, hs] using h1
    | @woken e hm hw =>
      simpa [toReal, hm, hw] using ih
    | @early e hm =>
      by_cases he : m.wakes = e
      · have := RealReach.step ih (RealStep.early (bits := bits) (seen := seen) (by simp [toReal, hm, he]))
        simpa [toReal] using this
      · simpa [toReal, hm, he] using ih
    | atOnce hm =>
      have h1 := RealReach.step ih (RealStep.check (bits := bits) (seen := seen) (by simp [toReal, hm]))
      by_cases hs : bits m.word = bits seen
      · have h2 := RealReach.step h1 (RealStep.early (bits := bits) (seen := seen) (by simp [toReal, hs]))
        simpa [toReal] using h2
      · simpa [toReal, hs] using h1

/-- Where the compared bits of two words match, the whole words may not: the compare that reads fewer bits. -/
def Coarser (fewer whole : Nat → Nat) : Prop := ∀ a b, whole a = whole b → fewer a = fewer b

/-- Every state the wait on `whole` reaches, the wait on fewer bits reaches. It sleeps where the other sleeps, and
where it sleeps alone an early return brings it to where the other returned. -/
theorem whole_in_low {fewer whole : Nat → Nat} (hc : Coarser fewer whole) {seen first : Nat} {s : Real}
    (h : RealReach whole seen first s) : RealReach fewer seen first s := by
  induction h with
  | start => exact RealReach.start
  | @step r _ _ st ih =>
    cases st with
    | store v => exact RealReach.step ih (RealStep.store v)
    | wake => exact RealReach.step ih RealStep.wake
    | check hr =>
      have h1 := RealReach.step ih (RealStep.check (bits := fewer) (seen := seen) hr)
      by_cases hw : whole r.word = whole seen
      · simpa [hw, hc _ _ hw] using h1
      · by_cases hf : fewer r.word = fewer seen
        · have h2 := RealReach.step h1 (RealStep.early (bits := fewer) (seen := seen) (by simp [hf]))
          simpa [hw] using h2
        · simpa [hw, hf] using h1
    | early hr => exact RealReach.step ih (RealStep.early hr)

/-- The bits the Linux futex compares: the low 32. -/
def low32 (n : Nat) : Nat := n % 2 ^ 32

/-- The whole word's bits are finer than the low 32. -/
theorem low32_coarser : Coarser low32 id := by
  intro a b h; simp at h; simp [h]

/-- A word whose high half moved: the 32-bit compare puts the waiter to sleep, the whole-word compare returns. -/
theorem low_sleeps :
    RealStep low32 0 ⟨2 ^ 32, 0, .ready⟩ ⟨2 ^ 32, 0, .asleep⟩ ∧
    ¬ RealStep id 0 ⟨2 ^ 32, 0, .ready⟩ ⟨2 ^ 32, 0, .asleep⟩ := by
  constructor
  · have := RealStep.check (bits := low32) (seen := 0) (s := ⟨2 ^ 32, 0, .ready⟩) rfl
    simpa [low32] using this
  · intro h
    cases h

/-- Two sleepers and a wake that ends one sleep: the second sleeper's state after the waker's store and wake. -/
structure TwoSleepers where
  word : Nat
  first : RealWaiter
  second : RealWaiter
  deriving DecidableEq, Repr

/-- A wake that ends one sleep, the first asleep: what WakeByAddressSingle, a futex wake of 1, or a compare-and-wait
wake with no all flag does. -/
def wakeOne (s : TwoSleepers) : TwoSleepers :=
  if s.first = .asleep then { s with first := .done }
  else if s.second = .asleep then { s with second := .done } else s

/-- Both asleep on word 0; the waker stores 1 and wakes once. The second sleeper is still asleep, the word moved, and
no wake remains to come. -/
theorem one_wake_loses :
    wakeOne { word := 1, first := .asleep, second := .asleep } = { word := 1, first := .done, second := .asleep } := by
  decide

end LocusCarcerum.Futex
