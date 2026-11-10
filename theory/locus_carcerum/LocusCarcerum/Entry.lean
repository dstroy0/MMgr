/-!
# LocusCarcerum, the movable option's entries and the zeroing

A movable take claims an entry (`carcer_entry_claimed`) by setting its bit of `claimed` with one fetch-or, and the take
whose fetch-or found the bit clear names the entry; a release (`carcer_entry_given`) clears the bit of the entry it
named. The theorem: no two contexts name one entry.

The zeroing. A give zeroes the payload, `size & zeroing & proceeds` bytes, and so the whole payload where both
masks are set and nothing otherwise.
-/

namespace LocusCarcerum.Entry

/-- A context: at rest, or naming an entry. -/
inductive At where
  | idle
  | names (entry : Nat)
  deriving DecidableEq, Repr

structure State where
  claimed : Nat → Bool
  ctx : Nat → At

def setCtx (f : Nat → At) (i : Nat) (a : At) : Nat → At := fun j => if j = i then a else f j
def setBit (f : Nat → Bool) (k : Nat) (b : Bool) : Nat → Bool := fun j => if j = k then b else f j

inductive Step : State → State → Prop where
  /-- A claim's fetch-or: the entry named where the bit was clear, nothing where it was set. -/
  | claim {s : State} (i k : Nat) (h : s.ctx i = .idle) :
      Step s { claimed := setBit s.claimed k true,
               ctx := setCtx s.ctx i (if s.claimed k then .idle else .names k) }
  /-- A release: the bit of the entry named cleared. -/
  | release {s : State} (i k : Nat) (h : s.ctx i = .names k) :
      Step s { claimed := setBit s.claimed k false, ctx := setCtx s.ctx i .idle }

def start : State := ⟨fun _ => false, fun _ => .idle⟩

inductive Reach : State → Prop where
  | start : Reach start
  | step {s s' : State} : Reach s → Step s s' → Reach s'

structure Inv (s : State) : Prop where
  namedClaimed : ∀ i k, s.ctx i = .names k → s.claimed k = true
  oneName : ∀ i j k, s.ctx i = .names k → s.ctx j = .names k → i = j

theorem inv_step {s s' : State} (hs : Inv s) (st : Step s s') : Inv s' := by
  cases st with
  | claim i k h =>
    constructor
    · intro j m hj
      by_cases hji : j = i
      · subst hji
        cases hk : s.claimed k <;> simp [setCtx, hk] at hj
        subst hj; simp [setBit]
      · have := hs.namedClaimed j m (by simpa [setCtx, hji] using hj)
        by_cases hmk : m = k
        · subst hmk; simp [setBit]
        · simp [setBit, hmk, this]
    · intro a b m ha hb
      have old : ∀ c, c ≠ i → setCtx s.ctx i (if s.claimed k then .idle else .names k) c = .names m →
          s.ctx c = .names m := by
        intro c hci hc; simpa [setCtx, hci] using hc
      by_cases hai : a = i
      · by_cases hbi : b = i
        · rw [hai, hbi]
        · subst hai
          have hb' := old b hbi hb
          cases hck : s.claimed k
          · simp [setCtx, hck] at ha
            subst ha
            have := hs.namedClaimed b _ hb'
            rw [hck] at this; cases this
          · simp [setCtx, hck] at ha
      · by_cases hbi : b = i
        · subst hbi
          have ha' := old a hai ha
          cases hck : s.claimed k
          · simp [setCtx, hck] at hb
            subst hb
            have := hs.namedClaimed a _ ha'
            rw [hck] at this; cases this
          · simp [setCtx, hck] at hb
        · exact hs.oneName a b m (old a hai ha) (old b hbi hb)
  | release i k h =>
    constructor
    · intro j m hj
      have hji : j ≠ i := by intro e; subst e; simp [setCtx] at hj
      have hj' : s.ctx j = .names m := by simpa [setCtx, hji] using hj
      have hmk : m ≠ k := by
        intro e; subst e; exact hji (hs.oneName j i m hj' h)
      simp [setBit, hmk, hs.namedClaimed j m hj']
    · intro a b m ha hb
      have hai : a ≠ i := by intro e; subst e; simp [setCtx] at ha
      have hbi : b ≠ i := by intro e; subst e; simp [setCtx] at hb
      exact hs.oneName a b m (by simpa [setCtx, hai] using ha) (by simpa [setCtx, hbi] using hb)

theorem inv_reach {s : State} (h : Reach s) : Inv s := by
  induction h with
  | start => exact ⟨by intro i k h; simp [start] at h, by intro i j k h; simp [start] at h⟩
  | step _ st ih => exact inv_step ih st

/-- No two contexts name one entry. -/
theorem one_name {s : State} (h : Reach s) (i j k : Nat) (hi : s.ctx i = .names k) (hj : s.ctx j = .names k) :
    i = j := (inv_reach h).oneName i j k hi hj

/-- The bytes a give zeroes: the payload where the give zeroes and proceeds, nothing otherwise. -/
def zeroed (size : Nat) (zeroing proceeds : Bool) : Nat := if zeroing && proceeds then size else 0

theorem zeroed_whole (size : Nat) : zeroed size true true = size := rfl
theorem zeroed_none (size : Nat) (z p : Bool) (h : z = false ∨ p = false) : zeroed size z p = 0 := by
  rcases h with h | h <;> simp [zeroed, h]

end LocusCarcerum.Entry
