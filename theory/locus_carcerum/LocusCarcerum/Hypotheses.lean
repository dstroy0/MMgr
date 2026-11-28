import LocusCarcerum.Vector
import LocusCarcerum.EndToEnd

/-!
# LocusCarcerum, the tests of the hypotheses

Each theorem of `Guard.lean`, `Vector.lean`, `Linearizable.lean` and `EndToEnd.lean` holds under its hypotheses. This
file tests those hypotheses in two ways.

* The facts of the build. A hypothesis on a width, an alignment or a count of bytes is a fact that the C fixes at
  compile time. Each test here takes the formula of the C at each width that `embed_types.h` selects, and shows the
  hypothesis holds there. The C asserts the same facts on the build itself, with `EMBED_STATIC_ASSERT`.

* The witnesses. Each test here gives a state that the steps reach from the start, where all the hypotheses of a
  theorem hold at the same time. So no theorem holds only because no state meets its hypotheses.
-/

namespace LocusCarcerum.Hypotheses

open LocusCarcerum.Layout

/-! ## The facts of the build -/

/-- `MMGR_CARCER_ALIGN` as `locus_carcerum.h` writes it: the larger of the bytes of `embed_word` and the alignment of
`embed_atomic_usize`. -/
def carcerAlign (wordBytes atomicAlign : Nat) : Nat := if wordBytes > atomicAlign then wordBytes else atomicAlign

/-- The bytes of `embed_word` and the alignment of `embed_atomic_usize` at each width that `embed_types.h` selects: 16,
32 and 64 bits, on an ABI that aligns a word at its size. -/
def rows : List (Nat × Nat) := [(2, 2), (4, 4), (8, 8)]

/-- `MMGR_ALIGN_BYTES`, the default of `mmgr_config.h`. -/
def alignBytes : Nat := 16

/-- At each width, `MMGR_CARCER_ALIGN` is a power of two, and it divides `MMGR_ALIGN_BYTES`. These are the asserts of
`locus_carcerum.h` on the two. So the hypothesis `align ∣ poolAlign` of `elem_aligned` holds for the grain of a cell. -/
theorem align_rows : ∀ r ∈ rows,
    carcerAlign r.1 r.2 &&& (carcerAlign r.1 r.2 - 1) = 0 ∧ alignBytes % carcerAlign r.1 r.2 = 0 := by
  decide

/-- The widths of `embed_atomic_usize_value` in bits. -/
def widths : List Nat := [16, 32, 64]

/-- `MMGR_CARCER_BYTES_MAX`: one bit below the half of the atomic size word. -/
def bytesMax (bits : Nat) : Nat := 2 ^ (bits / 2 - 1)

/-- At each width, the max bytes of a cellblock are below the values of the word. -/
theorem bytes_below_word : ∀ b ∈ widths, bytesMax b < 2 ^ b := by
  decide

/-- `MMGR_CARCER_POOL_BYTES_MAX`: one less than the values of a field of the closed word, times the bytes of a
region. A field has a third of the bits above the four flags, and a region is a quarter of a half's reach. -/
def poolMax (bits : Nat) : Nat := (2 ^ ((bits - 4) / 3) - 1) * 2 ^ (bits / 2 - 2)

/-- At each width, the max bytes of a pool are below the values of the word. -/
theorem pool_below_word : ∀ b ∈ widths, poolMax b < 2 ^ b := by
  decide

/-- The storage of a pool declaration is at max `MMGR_CARCER_POOL_BYTES_MAX`, as `MMGR_CARCER_STORAGE_0` asserts. So
the hypothesis `size < W` of `Bounds.fits_inside` holds for each cell at each width. -/
theorem span_hypothesis (b size : Nat) (hb : b ∈ widths) (h : size ≤ poolMax b) : size < 2 ^ b :=
  Nat.lt_of_le_of_lt h (pool_below_word b hb)

/-- The span test of `vec[n]` at the word of 32 bits: a stride of 8 bytes and a count of 100. It passes index 99, and
refuses index 100. -/
theorem legal_at_word : Guard.legal (2 ^ 32) 8 100 99 = true ∧ Guard.legal (2 ^ 32) 8 100 100 = false := by
  decide

/-- An element aligned at 8 bytes in a pool aligned at `MMGR_ALIGN_BYTES`. The hypotheses of `elem_aligned` hold, and
each element lies on its alignment. -/
theorem element_on_alignment (base i : Nat) (hbase : alignBytes ∣ base) :
    8 ∣ base + i * Vector.roundUp 8 8 :=
  Vector.elem_aligned 8 8 alignBytes base i (by decide) hbase

/-- The ceiling of a vector of 8-byte elements in a pool of 4096 bytes, with a header of 16 bytes. The old and the new
cell of a growth at that ceiling fit the pool together. -/
theorem ceiling_at_pool : Vector.poolLimit 16 8 4096 = 254 ∧
    Vector.cellBytes 16 8 254 + Vector.cellBytes 16 8 254 ≤ 4096 := by
  decide

/-- A cell of 64 bytes with a header of 16 bytes holds a vector of 8 elements of 8 bytes. The record that the layout
gives it meets `Guard.Placed`. -/
theorem placed_witness (rest : List Cell) :
    Guard.Placed (Vector.recordOf 16 0 ⟨64, true, 1⟩ 8 3 8) :=
  (Vector.placed_in_layout 16 0 ⟨64, true, 1⟩ rest 8 3 8 (by decide) (by decide)).1

/-! ## The witnesses: the claims of `Linearizable.lean` -/

/-- Two threads: thread `0` writes `7` at element `0`, and thread `1` reads element `1`. -/
def twoCalls : Nat → Linearizable.Thread Nat 8 := fun t =>
  if t = 0 then ⟨0, .put 7, .idle⟩ else ⟨1, .get, .idle⟩

/-- After two steps of each thread, both hold their claims at the same time, on two elements. The hypotheses of
`Linearizable.exclusive` and `Linearizable.held_alone` hold there. -/
theorem two_claims_held :
    ((Linearizable.run (Linearizable.start (fun _ => 0) twoCalls) [0, 0, 1, 1]).threads 0).phase.holds = true ∧
      ((Linearizable.run (Linearizable.start (fun _ => 0) twoCalls) [0, 0, 1, 1]).threads 1).phase.holds = true := by
  decide

/-- A run to the end of both threads. The reader gives element `1` of the replay before it. The log holds both threads
in the order of their claims. The hypotheses of `Linearizable.linearizable` and of `quiescent` hold there. -/
theorem reader_result :
    ((Linearizable.run (Linearizable.start (fun _ => 0) twoCalls) [1, 1, 0, 0, 1, 1, 0, 0]).threads 1).phase.result =
        some (some 0) ∧
      (Linearizable.run (Linearizable.start (fun _ => 0) twoCalls) [1, 1, 0, 0, 1, 1, 0, 0]).log = [1, 0] := by
  decide

/-! ## The witnesses: the run end to end -/

/-- The entry of two workers, the grant and the push of two slots by worker `0`, and the pop of slot `1` by worker
`1` on lane `1`. -/
def popped : List EndToEnd.Op :=
  [.enter 0 0, .enter 1 1, .grant 0 0, .push 0 0 true, .grant 0 1, .push 0 1 true, .pop 1 1]

/-- Worker `1` does the call of slot `1` to the end, and leaves its lane. -/
def finished : List EndToEnd.Op := [.work 1, .work 1, .work 1, .work 1, .finish 1, .leave 1]

/-- After `popped`, worker `1` runs the call of slot `1` in its pass on lane `1`. The hypotheses of
`EndToEnd.call_in_pass` and `EndToEnd.drain_waits_for_call` hold there. After `finished`, a drain that read lane `1`
reads it over, and the thread of the call is at `done`. -/
theorem drain_witness :
    (EndToEnd.erun 2 (EndToEnd.estart EndToEnd.twoWrites (fun _ => 0) : EndToEnd.E Nat 8) popped).bound 1 = some 1 ∧
      (EndToEnd.erun 2 (EndToEnd.estart EndToEnd.twoWrites (fun _ => 0) : EndToEnd.E Nat 8) popped).host.worker 1 =
        .inPass 1 ∧
      EmbeddedTypes.Manifold.Drain.over
          ((EndToEnd.erun 2 (EndToEnd.estart EndToEnd.twoWrites (fun _ => 0) : EndToEnd.E Nat 8) popped).host.lane 1)
          ((EndToEnd.erun 2 (EndToEnd.estart EndToEnd.twoWrites (fun _ => 0) : EndToEnd.E Nat 8)
            (popped ++ finished)).host.lane 1) = true ∧
      ((EndToEnd.erun 2 (EndToEnd.estart EndToEnd.twoWrites (fun _ => 0) : EndToEnd.E Nat 8)
        (popped ++ finished)).mem.threads 1).phase.isDone = true := by
  decide

/-- A slot whose job pointer fails its check, popped by worker `0`: its call stays before its read. The hypothesis of
`EndToEnd.refused_never_runs` holds there. -/
theorem refused_witness :
    (EndToEnd.erun 1 (EndToEnd.estart EndToEnd.twoWrites (fun _ => 0) : EndToEnd.E Nat 8)
        [.enter 0 0, .grant 0 0, .push 0 0 false, .pop 0 0, .work 0, .work 0]).host.slot 0 = .popped 0 0 false ∧
      ((EndToEnd.erun 1 (EndToEnd.estart EndToEnd.twoWrites (fun _ => 0) : EndToEnd.E Nat 8)
        [.enter 0 0, .grant 0 0, .push 0 0 false, .pop 0 0, .work 0, .work 0]).mem.threads 0).phase.before = true := by
  decide

/-- The futex model starts on a lane word at rest, with no sleeper. That state meets the hypotheses of
`EndToEnd.erun_wakes` for the lane of the start. -/
theorem wake_witness (l : Nat) :
    ∃ ws : EmbeddedTypes.Manifold.Wake.State, EmbeddedTypes.Manifold.Wake.Reach ws ∧
      ws.word.busy = ((EndToEnd.estart EndToEnd.twoWrites (fun _ => 0) : EndToEnd.E Nat 8).host.lane l).busy ∧
      ws.word.generation = ((EndToEnd.estart EndToEnd.twoWrites (fun _ => 0) : EndToEnd.E Nat 8).host.lane l).generation :=
  ⟨⟨⟨false, 0, false⟩, .idle, false, false⟩, .rest _, rfl, rfl⟩

/-! ## The witnesses: the vector behind the guard -/

/-- A vector of capacity 2 and ceiling 2 behind token `0`, made by a create from the empty guard. -/
def twoSlots : Vector.Guarded Nat := Vector.create Vector.emptyGuard 0 2 2 0

/-- The guard is sound from the start, and after the create. The hypotheses `Inv g` of `Vector.step_spec`,
`Vector.decoupled` and `Vector.run_decoupled` hold there. -/
theorem guard_sound : Vector.Inv twoSlots := (Vector.create_inv _ Vector.empty_inv 0 2 2 0).1

/-- Two pushes, a read of index 1, and a push at the ceiling, through the guard. The read gives the second element, and
the guard refuses the push at the ceiling. -/
theorem guard_calls :
    (Vector.run 1 Nat.one_pos 0 (2 ^ 8) 1 twoSlots [(0, .push 5), (0, .push 6), (0, .get 1), (0, .push 7)])[2]?
        matches some (Vector.Answer.elem 6) ∧
      (Vector.run 1 Nat.one_pos 0 (2 ^ 8) 1 twoSlots [(0, .push 5), (0, .push 6), (0, .get 1), (0, .push 7)])[3]?
        matches some Vector.Answer.refused := by
  decide

end LocusCarcerum.Hypotheses
