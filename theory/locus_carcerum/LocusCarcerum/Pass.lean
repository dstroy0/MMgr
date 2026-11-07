/-!
# LocusCarcerum, passes and the writer's drain

A pass (`carcer_passed`) badges in through the turnstiles, then reads the closed bit: clear, it works on the
cellblock; set, it works on the sink and touches nothing. It badges out when its work is done. A writer
(`carcer_shut`) takes the closed bit, then drains the turnstiles: it waits until no pass is badged in. It then
works alone, and opens.

The theorem: while a writer works alone, no pass works on the cellblock. A pass badged in before the shut is
waited out by the drain; a pass badged in after reads the closed bit set.
-/

namespace LocusCarcerum.Pass

/-- A pass: at rest, badged in before its read, working on the cellblock, or working on the sink. -/
inductive PassAt where
  | idle
  | badged
  | working
  | sink
  deriving DecidableEq, Repr

/-- A writer: at rest, holding the bit and draining, or working alone. -/
inductive WriterAt where
  | idle
  | draining
  | alone
  deriving DecidableEq, Repr

structure State where
  closed : Bool
  pass : Nat → PassAt
  writer : Nat → WriterAt

def setPass (f : Nat → PassAt) (i : Nat) (p : PassAt) : Nat → PassAt := fun j => if j = i then p else f j
def setWriter (f : Nat → WriterAt) (i : Nat) (w : WriterAt) : Nat → WriterAt := fun j => if j = i then w else f j

inductive Step : State → State → Prop where
  | badge {s : State} (i : Nat) (h : s.pass i = .idle) : Step s { s with pass := setPass s.pass i .badged }
  | read {s : State} (i : Nat) (h : s.pass i = .badged) :
      Step s { s with pass := setPass s.pass i (if s.closed then .sink else .working) }
  | leave {s : State} (i : Nat) (h : s.pass i = .working ∨ s.pass i = .sink) :
      Step s { s with pass := setPass s.pass i .idle }
  | shut {s : State} (w : Nat) (h : s.writer w = .idle) (hc : s.closed = false) :
      Step s { s with closed := true, writer := setWriter s.writer w .draining }
  | drained {s : State} (w : Nat) (h : s.writer w = .draining) (hd : ∀ i, s.pass i = .idle) :
      Step s { s with writer := setWriter s.writer w .alone }
  | opened {s : State} (w : Nat) (h : s.writer w = .alone) :
      Step s { s with closed := false, writer := setWriter s.writer w .idle }

def start : State := ⟨false, fun _ => .idle, fun _ => .idle⟩

inductive Reach : State → Prop where
  | start : Reach start
  | step {s s' : State} : Reach s → Step s s' → Reach s'

structure Inv (s : State) : Prop where
  writerClosed : ∀ w, s.writer w ≠ .idle → s.closed = true
  oneWriter : ∀ v w, s.writer v ≠ .idle → s.writer w ≠ .idle → v = w
  aloneQuiet : ∀ w i, s.writer w = .alone → s.pass i ≠ .working

theorem inv_start : Inv start := ⟨by intro w h; simp [start] at h, by intro v w h; simp [start] at h,
  by intro w i h; simp [start] at h⟩

theorem inv_step {s s' : State} (hs : Inv s) (st : Step s s') : Inv s' := by
  cases st with
  | badge i h =>
    refine ⟨hs.writerClosed, hs.oneWriter, ?_⟩
    intro w j hw hj
    by_cases hji : j = i
    · subst hji; simp [setPass] at hj
    · exact hs.aloneQuiet w j hw (by simpa [setPass, hji] using hj)
  | read i h =>
    refine ⟨hs.writerClosed, hs.oneWriter, ?_⟩
    intro w j hw hj
    by_cases hji : j = i
    · subst hji
      have := hs.writerClosed w (by rw [hw]; simp)
      simp [setPass, this] at hj
    · exact hs.aloneQuiet w j hw (by simpa [setPass, hji] using hj)
  | leave i h =>
    refine ⟨hs.writerClosed, hs.oneWriter, ?_⟩
    intro w j hw hj
    by_cases hji : j = i
    · subst hji; simp [setPass] at hj
    · exact hs.aloneQuiet w j hw (by simpa [setPass, hji] using hj)
  | shut v h hc =>
    refine ⟨?_, ?_, ?_⟩
    · intro _ _; rfl
    · intro a b ha hb
      have none : ∀ k, k ≠ v → setWriter s.writer v .draining k ≠ .idle → False := by
        intro k hkv hk
        have := hs.writerClosed k (by simpa [setWriter, hkv] using hk)
        rw [hc] at this; cases this
      by_cases hav : a = v
      · by_cases hbv : b = v
        · rw [hav, hbv]
        · exact (none b hbv hb).elim
      · exact (none a hav ha).elim
    · intro w j hw _
      by_cases hwv : w = v
      · subst hwv; simp [setWriter] at hw
      · have := hs.writerClosed w (by simp [setWriter, hwv] at hw; rw [hw]; simp)
        rw [hc] at this; cases this
  | drained v h hd =>
    refine ⟨?_, ?_, ?_⟩
    · intro w hw
      by_cases hwv : w = v
      · exact hs.writerClosed v (by rw [h]; simp)
      · exact hs.writerClosed w (by simpa [setWriter, hwv] using hw)
    · intro a b ha hb
      have ha' : s.writer a ≠ .idle := by
        by_cases hav : a = v
        · rw [hav, h]; simp
        · simpa [setWriter, hav] using ha
      have hb' : s.writer b ≠ .idle := by
        by_cases hbv : b = v
        · rw [hbv, h]; simp
        · simpa [setWriter, hbv] using hb
      exact hs.oneWriter a b ha' hb'
    · intro w j _ hj
      rw [hd j] at hj; cases hj
  | opened v h =>
    refine ⟨?_, ?_, ?_⟩
    · intro w hw
      by_cases hwv : w = v
      · subst hwv; simp [setWriter] at hw
      · have := hs.oneWriter v w (by rw [h]; simp) (by simpa [setWriter, hwv] using hw)
        exact (hwv this.symm).elim
    · intro a b ha hb
      have hav : a ≠ v := by intro e; subst e; simp [setWriter] at ha
      have hbv : b ≠ v := by intro e; subst e; simp [setWriter] at hb
      exact hs.oneWriter a b (by simpa [setWriter, hav] using ha) (by simpa [setWriter, hbv] using hb)
    · intro w j hw
      have hwv : w ≠ v := by intro e; subst e; simp [setWriter] at hw
      exact hs.aloneQuiet w j (by simpa [setWriter, hwv] using hw)

theorem inv_reach {s : State} (h : Reach s) : Inv s := by
  induction h with
  | start => exact inv_start
  | step _ st ih => exact inv_step ih st

/-- While a writer works alone, no pass works on the cellblock. -/
theorem alone_exclusive {s : State} (h : Reach s) (w i : Nat) (hw : s.writer w = .alone) : s.pass i ≠ .working :=
  (inv_reach h).aloneQuiet w i hw

end LocusCarcerum.Pass
