/-!
# LocusCarcerum, the orderings the drain rests on

`Pass.lean` and `Trim.lean` interleave whole steps. Under C11 the steps are atomic operations with orderings, and
two facts carry those proofs over.

The closed word. Every write to it is a read-modify-write: the fetch-or of a shut and of a give, the fetch-and and
the compare-exchange of an open. C11 gives the writes to one object one modification order, and a read-modify-write
reads the last value before its own write in it ([atomics.order], the read-modify-write rule). The steps of
`Trim.lean` on the closed word are therefore one order of whole steps, as the model takes them.

The drain. A pass claims its lane with a fetch-or on `busy`, then a sequentially consistent fence, then loads the
closed bit (`embed_turnstile_enter`, `carcer_attempted`). A writer fetch-ors the closed bit, then a sequentially
consistent fence, then loads `busy` (`carcer_shut`, `embed_turnstiles_drain`). The fences stand in one total order
`S`. C11's fence rule ([atomics.order], the rule for two seq_cst fences): where write `A` is sequenced before fence
`X`, fence `Y` before read `B`, and `X` precedes `Y` in `S`, `B` reads `A` or a later write. An execution below
carries the rule as its two hypotheses, one for each order of the two fences, and the theorem is the disjunction
the drain needs: the pass reads the closed bit set, or the drain reads the pass's lane busy and waits it out. That is
the step `Pass.lean` takes as whole.
-/

namespace LocusCarcerum.Order

/-- One pass and one writer as the fences order them. -/
structure Exec where
  /-- The places of the pass's fence and the writer's fence in `S`. -/
  passFence : Nat
  writerFence : Nat
  distinct : passFence ≠ writerFence
  /-- The pass's load of the closed word reads the writer's fetch-or, or a later write. -/
  passReadsClosed : Bool
  /-- The drain's load of `busy` reads the pass's claim, or a later write. -/
  drainReadsLane : Bool
  /-- The fence rule, the pass's fence first: the claim before it, the drain's load after the writer's. -/
  passFirst : passFence < writerFence → drainReadsLane = true
  /-- The fence rule, the writer's fence first: the fetch-or before it, the pass's load after the pass's. -/
  writerFirst : writerFence < passFence → passReadsClosed = true

/-- The pass reads the closed bit set, or the drain waits the pass out. -/
theorem drain_or_closed (e : Exec) : e.passReadsClosed = true ∨ e.drainReadsLane = true := by
  rcases Nat.lt_or_gt_of_ne e.distinct with h | h
  · exact Or.inr (e.passFirst h)
  · exact Or.inl (e.writerFirst h)

/-- Without the fences the two loads may both read the stale values: an execution with neither hypothesis. -/
structure Unfenced where
  passReadsClosed : Bool
  drainReadsLane : Bool

/-- The store-buffering outcome the fences forbid, which nothing in an unfenced execution rules out: the pass works
on the cellblock while the writer, its drain having missed the pass, works alone. -/
def storeBuffered : Unfenced := ⟨false, false⟩

theorem unfenced_admits : storeBuffered.passReadsClosed = false ∧ storeBuffered.drainReadsLane = false := by decide

end LocusCarcerum.Order
