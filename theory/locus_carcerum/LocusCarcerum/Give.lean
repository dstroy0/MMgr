/-!
# LocusCarcerum, a cell counted off once

A cell's owner word carries the completion bit. A give that names the cell's owner, and a rewind that passes the cell,
each claim the word with one fetch-or (`carcer_foot_claimed`). The context whose fetch-or found the bit clear counts
the cell's bytes off its owner, and every other claim counts nothing. A give that names another owner claims with a
permit of none: its fetch-or sets nothing and it counts nothing.

The theorem, over any sequence of claims on one cell. The bytes counted off are the cell's size where at least one
claim had a permit. They are none where none had one.
-/

namespace LocusCarcerum.Give

/-- One claim: the bytes it counts off and the completion bit after it. -/
def claim (size : Nat) (permit complete : Bool) : Nat × Bool :=
  if permit && !complete then (size, true) else (0, complete)

/-- Claims in turn on one cell: the bytes counted off, the completion bit carried from one to the next. -/
def claims (size : Nat) : Bool → List Bool → Nat
  | _, [] => 0
  | complete, p :: rest => (claim size p complete).1 + claims size (claim size p complete).2 rest

theorem claims_done (size : Nat) (rest : List Bool) : claims size true rest = 0 := by
  induction rest with
  | nil => rfl
  | cons p rest ih => cases p <;> simp [claims, claim, ih]

theorem claims_none (size : Nat) (rest : List Bool) (h : ∀ p ∈ rest, p = false) : claims size false rest = 0 := by
  induction rest with
  | nil => rfl
  | cons p rest ih =>
    have hp : p = false := h p (by simp)
    subst hp
    simp [claims, claim, ih (fun q hq => h q (by simp [hq]))]

/-- Counted off once: the size where any claim had a permit, nothing where none had one. -/
theorem claims_once (size : Nat) (permits : List Bool) :
    claims size false permits = if permits.any id then size else 0 := by
  induction permits with
  | nil => rfl
  | cons p rest ih =>
    cases p
    · simp [claims, claim, ih]
    · simp [claims, claim, claims_done]

end LocusCarcerum.Give
