/-!
# LocusCarcerum, the sleeper bit on the closed word

A pass that finds the cellblock closed backs off: it reads the closed word, sets the sleeper bit with one fetch-or,
and waits on the word for the value it read with that bit set (`embed_turnstile_backed_off`). The wait is the futex
contract of `Futex.lean`: the kernel compares and sleeps in one step. A writer opens with one fetch-and that takes
the trim bits, then a second fetch-and that clears the closed bit and the sleeper bit, and wakes the word where
the value the second fetch-and returned had the sleeper bit (`carcer_opened`).

One writer and one sleeper on the word's two bits. The writer shuts, takes the word, opens, and wakes. The sleeper
reads, sets the bit, and either sleeps or returns at the futex's compare. The `gated` sleeper sleeps only where the
value it read has the closed bit: a value read open has no writer to wake it.

The theorems:

- `gated_woken`: with the gate, a cellblock open with the writer at rest has no sleeper asleep. Every sleep ends at
  the open that ends the closed period it began in.
- `ungated_sleeps`: without the gate, the sleeper reads the word open, sets the bit and sleeps, with the writer at
  rest and no wake to come. The sleep lasts its whole timeout.

The model is sequentially consistent, as `Trim.lean` is.
-/

namespace LocusCarcerum.Sleep

/-- The closed word's two bits. -/
structure Word where
  closed : Bool
  sleepers : Bool
  deriving DecidableEq, Repr

/-- The writer: at rest, holding the closed bit, holding with the trim bits taken, or opened with a wake to make. -/
inductive Writer where
  | rest
  | holding
  | took
  | waking (wake : Bool)
  deriving DecidableEq, Repr

/-- The sleeper: before its read, with the word read, with the bit set, asleep, or returned. -/
inductive Sleeper where
  | idle
  | read (r : Word)
  | flagged (r : Word)
  | asleep
  | done
  deriving DecidableEq, Repr

structure State where
  word : Word
  writer : Writer
  sleeper : Sleeper
  deriving DecidableEq, Repr

section

variable (gated : Bool)

inductive Step : State → State → Prop where
  /-- The writer's shut: the closed bit taken where it was clear. -/
  | shut {s : State} (hw : s.writer = .rest) (hc : s.word.closed = false) :
      Step s { s with word := { s.word with closed := true }, writer := .holding }
  /-- The open's first fetch-and: the trim bits taken, which these two bits do not hold. -/
  | take {s : State} (hw : s.writer = .holding) : Step s { s with writer := .took }
  /-- The open's second fetch-and: both bits cleared, and a wake owed where the value it returned had the sleeper
  bit. -/
  | open_ {s : State} (hw : s.writer = .took) :
      Step s { s with word := ⟨false, false⟩, writer := .waking s.word.sleepers }
  /-- The wake: every sleep on the word ends where one is owed. -/
  | wake {s : State} {w : Bool} (hw : s.writer = .waking w) :
      Step s { s with writer := .rest,
                      sleeper := if w && s.sleeper = .asleep then .done else s.sleeper }
  /-- The sleeper reads the word; with the gate, a value read open ends the backoff with no sleep. -/
  | read {s : State} (hs : s.sleeper = .idle) :
      Step s { s with sleeper := if gated && !s.word.closed then .done else .read s.word }
  /-- The sleeper's fetch-or of the sleeper bit. -/
  | flag {s : State} {r : Word} (hs : s.sleeper = .read r) :
      Step s { s with word := { s.word with sleepers := true }, sleeper := .flagged r }
  /-- The futex's compare and sleep, one step: asleep where the word holds the value read with the bit set. -/
  | wait {s : State} {r : Word} (hs : s.sleeper = .flagged r) :
      Step s { s with sleeper := if s.word = { r with sleepers := true } then .asleep else .done }

def start : State := ⟨⟨false, false⟩, .rest, .idle⟩

inductive Reach : State → Prop where
  | start : Reach start
  | step {s s' : State} : Reach s → Step gated s s' → Reach s'

end

/-- The writer holds the closed bit: shut, or with the word taken. -/
def Holds (w : Writer) : Prop := w = .holding ∨ w = .took

/-- The invariant with the gate. A sleeper with the word read holds a closed value. The closed bit is set exactly
while the writer holds it. An asleep sleeper has both bits set on the word, or a wake owed to it. -/
structure Inv (s : State) : Prop where
  readClosed : ∀ r, (s.sleeper = .read r ∨ s.sleeper = .flagged r) → r.closed = true
  closedHeld : s.word.closed = true → Holds s.writer
  heldClosed : Holds s.writer → s.word.closed = true
  asleepCovered : s.sleeper = .asleep → (s.word.closed = true ∧ s.word.sleepers = true) ∨ s.writer = .waking true

theorem inv_start : Inv start where
  readClosed := by intro r h; simp [start] at h
  closedHeld := by intro h; simp [start] at h
  heldClosed := by intro h; simp [start, Holds] at h
  asleepCovered := by intro h; simp [start] at h

theorem inv_step {s s' : State} (hs : Inv s) (st : Step true s s') : Inv s' := by
  cases st with
  | shut hw hc =>
    refine ⟨hs.readClosed, fun _ => Or.inl rfl, fun _ => rfl, ?_⟩
    intro ha
    rcases hs.asleepCovered ha with ⟨hcl, _⟩ | hwk
    · rw [hc] at hcl; cases hcl
    · rw [hw] at hwk; cases hwk
  | take hw =>
    refine ⟨hs.readClosed, fun _ => Or.inr rfl, fun _ => hs.heldClosed (Or.inl hw), ?_⟩
    intro ha
    rcases hs.asleepCovered ha with h | hwk
    · exact Or.inl h
    · rw [hw] at hwk; cases hwk
  | open_ hw =>
    refine ⟨hs.readClosed, fun h => by simp at h, fun h => by simp [Holds] at h, ?_⟩
    intro ha
    rcases hs.asleepCovered ha with ⟨_, hsl⟩ | hwk
    · right; simp only [hsl]
    · rw [hw] at hwk; cases hwk
  | @wake w hw =>
    have hopen : s.word.closed = false := by
      cases hc : s.word.closed with
      | false => rfl
      | true =>
        rcases hs.closedHeld hc with h | h <;> rw [hw] at h <;> cases h
    refine ⟨?_, (fun h => by rw [hopen] at h; cases h), (fun h => by simp [Holds] at h), ?_⟩
    · intro r hr
      by_cases hsa : s.sleeper = .asleep
      · cases w <;> simp [hsa] at hr
      · have : s.sleeper = .read r ∨ s.sleeper = .flagged r := by cases w <;> simpa [hsa] using hr
        exact hs.readClosed r this
    · intro ha
      by_cases hsa : s.sleeper = .asleep
      · rcases hs.asleepCovered hsa with ⟨hcl, _⟩ | hwk
        · rw [hopen] at hcl; cases hcl
        · rw [hw] at hwk; cases hwk; simp [hsa] at ha
      · simp [hsa] at ha
  | read hsl =>
    refine ⟨?_, hs.closedHeld, hs.heldClosed, ?_⟩
    · intro r hr
      cases hc : s.word.closed with
      | true => simp [hc] at hr; subst hr; exact hc
      | false => simp [hc] at hr
    · intro ha
      cases hc : s.word.closed <;> simp [hc] at ha
  | @flag r hsl =>
    refine ⟨?_, hs.closedHeld, hs.heldClosed, ?_⟩
    · intro r' hr'
      simp at hr'
      subst hr'
      exact hs.readClosed r (Or.inl hsl)
    · intro ha; simp at ha
  | @wait r hsl =>
    have hrc := hs.readClosed r (Or.inr hsl)
    refine ⟨?_, hs.closedHeld, hs.heldClosed, ?_⟩
    · intro r' hr'
      by_cases he : s.word = { r with sleepers := true } <;> simp [he] at hr'
    · intro ha
      by_cases he : s.word = { r with sleepers := true }
      · left; rw [he]; exact ⟨hrc, rfl⟩
      · simp [he] at ha

theorem inv_reach {s : State} (h : Reach true s) : Inv s := by
  induction h with
  | start => exact inv_start
  | step _ st ih => exact inv_step ih st

/-- With the gate, a cellblock open with the writer at rest has no sleeper asleep. -/
theorem gated_woken {s : State} (h : Reach true s) (hw : s.writer = .rest) (hc : s.word.closed = false) :
    s.sleeper ≠ .asleep := by
  intro ha
  rcases (inv_reach h).asleepCovered ha with ⟨hcl, _⟩ | hwk
  · rw [hc] at hcl; cases hcl
  · rw [hw] at hwk; cases hwk

/-- Without the gate: the sleeper reads the word open, sets the bit and sleeps, the writer at rest. -/
theorem ungated_sleeps : Reach false ⟨⟨false, true⟩, .rest, .asleep⟩ := by
  have h1 : Reach false ⟨⟨false, false⟩, .rest, .read ⟨false, false⟩⟩ :=
    Reach.step Reach.start (Step.read (gated := false) (s := start) rfl)
  have h2 : Reach false ⟨⟨false, true⟩, .rest, .flagged ⟨false, false⟩⟩ :=
    Reach.step h1 (Step.flag (gated := false) (r := ⟨false, false⟩) rfl)
  have h3 := Reach.step h2 (Step.wait (gated := false) (r := ⟨false, false⟩) rfl)
  simpa using h3

end LocusCarcerum.Sleep
