import LocusCarcerum.Guard
import LocusCarcerum.Take

/-!
# LocusCarcerum, a vector over the two kinds of cell

A vector that does what `std::vector` does: it appends, it removes the last element, it reads and writes an
element by its index, and it grows. The caller holds a guard and a const identity token, and nothing more.

* The guard takes the token and passes it to the record behind it (`LocusCarcerum.Guard`). The record holds the
  internal state: the identity of the cell, the count, the capacity and the ceiling. The caller never sees the record
  or an internal pointer.

* The record decides if a call is legal, and one CAS commits the call, as the movable option does. Here a call
  commits in one step. `LocusCarcerum.Linearizable` runs the four steps of the C for each count of threads, and
  shows that they leave the memory of this one step commit. `LocusCarcerum.ConcurrentVector` runs `at`, `set`,
  `push_back` and `pop_back` on many threads at once, and proves them linearizable. `vec[n]` decays to
  the base of the cell and the offset `n * stride`. The span test of the movable option passes the span only in the
  live elements, and so it stops before the neighbor cell.

* The elements are one movable cell (`mmgr_movable_buf_alloc`). A read copies a span of the cell out to a buffer of
  the caller (`mmgr_movable_buf_read`). A write copies a buffer of the caller into the cell
  (`mmgr_movable_buf_write`). The mover (`mmgr_movable_buf_move`) changes where a cell lies and not what it holds.

The movable option has no copy from one cell to another. So a change of one element is a read into an element on
the stack of the caller, the change, and a write back. A growth copies the elements a chunk at a time through a
buffer on the stack of the caller.

The vector has a bounded pool, and so it has a hard ceiling on its capacity. A growth stops at the ceiling, and a
`push_back` at the ceiling refuses its element and changes nothing. The alignment of the pool, which `alignas` sets,
bounds the alignment of an element.

The theorems of the vector.

* The store: an operation changes one payload only through that payload and its own arguments. So no operation copies
  one cell to another.

* The vector: each operation leaves the elements of `std::vector`, and a growth through the buffer keeps them for each
  chunk size. The mover changes no element.

* The guard: a call gives back an element, a count, a commit or a refusal, and never an identity or an address. Two
  sound guards with the same view through each token give the same answers to each run of calls. The mover and a
  relocation to a fresh cell change no view. So the compaction behind the guard is no concern of the caller.

* The pool: the ceiling holds the old and the new cell of a growth together in the pool, and it is the largest that
  does. Each element lies at an offset of its alignment.

The theorems of the holes.

* The tiers that give out pointers: a change of the layout that keeps each pointer keeps the bytes of the holes.

* Odd slabs: a persistent take that no hole holds bumps the end, and the holes stay. A slab of the size of a hole
  fills it with no rest.

* Temporary cells held past their call: a give in stack order leaves no hole, and a give under a held cell leaves
  one.

* The movable option: the mover can put the cells end to end with no hole, and each vector keeps its elements.

* The persistent and temporary tiers behind the guard: the fill puts the busy cells end to end, with no hole. It
  changes no view through a token. The hole of a held temporary cell closes. A caller that holds pointers pins each
  cell, and a tier with a hole keeps it.
-/

namespace LocusCarcerum.Vector

open LocusCarcerum.Layout

/-! ## The movable option -/

/-- The payload of one movable cell: its count of elements and the element at each index below the count. -/
structure Payload (α : Type) where
  len : Nat
  elem : Nat → α

/-- The movable option: the payload of each identity, and the offset that the mover gave it. An identity that names
no cell has no payload. `next` is the identity that the next take gives out. -/
structure Store (α : Type) where
  payload : Nat → Option (Payload α)
  place : Nat → Nat
  next : Nat

/-- A function with the value at one point changed. -/
def update {β : Type} (f : Nat → β) (k : Nat) (v : β) : Nat → β := fun j => if j = k then v else f j

variable {α : Type} [Inhabited α]

/-- The elements from `off` of a payload, `n` of them, as a list. -/
def slice (p : Payload α) (off n : Nat) : List α := (List.range n).map fun j => p.elem (off + j)

/-- A read out to a buffer of the caller: the `n` elements from `off`, none where the span passes the cell or the
identity names no cell. -/
def read (s : Store α) (id off n : Nat) : Option (List α) :=
  match s.payload id with
  | some p => if off + n ≤ p.len then some (slice p off n) else none
  | none => none

/-- The payload with a buffer put over the span from `off`. -/
def spliced (p : Payload α) (off : Nat) (buf : List α) : Payload α :=
  ⟨p.len, fun j => if off ≤ j ∧ j < off + buf.length then buf.getD (j - off) default else p.elem j⟩

/-- A write from a buffer of the caller: the span from `off` replaced. The store is as it was where the span passes
the cell or the identity names no cell. -/
def write (s : Store α) (id off : Nat) (buf : List α) : Store α :=
  match s.payload id with
  | some p =>
    if off + buf.length ≤ p.len then { s with payload := update s.payload id (some (spliced p off buf)) } else s
  | none => s

/-- A take of the movable option: a new identity, and a payload of `n` copies of `d`. -/
def alloc (s : Store α) (n : Nat) (d : α) : Store α × Nat :=
  ({ s with payload := update s.payload s.next (some ⟨n, fun _ => d⟩), next := s.next + 1 }, s.next)

/-- A give of the movable option: the identity names no cell after it. -/
def release (s : Store α) (id : Nat) : Store α := { s with payload := update s.payload id none }

/-- The mover: each identity at the offset that `to` gives it. -/
def moved (s : Store α) (to : Nat → Nat) : Store α := { s with place := to }

/-- The operations of the movable option that change the store. A read changes nothing, and so it is not one. -/
inductive Op (α : Type) where
  | take (n : Nat) (d : α)
  | give (id : Nat)
  | put (id off : Nat) (buf : List α)
  | move (to : Nat → Nat)

/-- An operation applied to the store. -/
def apply (s : Store α) : Op α → Store α
  | .take n d => (alloc s n d).1
  | .give id => release s id
  | .put id off buf => write s id off buf
  | .move to => moved s to

/-- No copy: after an operation, the payload of an identity comes from its payload before and from the operation
alone. Two stores that agree on that payload and on the next identity agree on it after the same operation. So no
operation carries the elements of one cell into another. -/
theorem no_copy (s t : Store α) (x : Nat) (op : Op α) (hx : s.payload x = t.payload x) (hn : s.next = t.next) :
    (apply s op).payload x = (apply t op).payload x := by
  cases op with
  | take n d => simp [apply, alloc, update, hx, hn]
  | give id => simp [apply, release, update, hx]
  | put id off buf =>
    simp only [apply, write]
    by_cases hid : x = id
    · subst hid
      cases hp : s.payload x with
      | none =>
        have ht : t.payload x = none := by rw [← hx, hp]
        simp [ht, hx]
      | some p =>
        have ht : t.payload x = some p := by rw [← hx, hp]
        simp only [ht]
        split <;> simp [update, hx]
    · cases hs : s.payload id <;> cases ht : t.payload id <;> simp <;>
        (try split) <;> (try split) <;> simp [update, hid, hx]
  | move to => simp [apply, moved, hx]

omit [Inhabited α] in
/-- The mover changes no payload. -/
theorem move_payload (s : Store α) (to : Nat → Nat) : (moved s to).payload = s.payload := rfl

/-! ### Reads and writes -/

omit [Inhabited α] in
theorem slice_length (p : Payload α) (off n : Nat) : (slice p off n).length = n := by simp [slice]

theorem slice_getD (p : Payload α) (off n i : Nat) (h : i < n) : (slice p off n).getD i default = p.elem (off + i) := by
  simp [slice, List.getD_eq_getElem?_getD, h]

omit [Inhabited α] in
theorem read_fits (s : Store α) (id off n : Nat) (p : Payload α) (hp : s.payload id = some p) (h : off + n ≤ p.len) :
    read s id off n = some (slice p off n) := by
  simp [read, hp, h]

theorem write_here (s : Store α) (id off : Nat) (buf : List α) (p : Payload α) (hp : s.payload id = some p)
    (h : off + buf.length ≤ p.len) : (write s id off buf).payload id = some (spliced p off buf) := by
  simp [write, hp, h, update]

theorem write_other (s : Store α) (id off : Nat) (buf : List α) (y : Nat) (hy : y ≠ id) :
    (write s id off buf).payload y = s.payload y := by
  unfold write
  split
  · split <;> simp [update, hy]
  · rfl

theorem write_next (s : Store α) (id off : Nat) (buf : List α) : (write s id off buf).next = s.next := by
  unfold write
  split
  · split <;> rfl
  · rfl

/-- Each identity at or past the next one names no cell. -/
def Fresh (s : Store α) : Prop := ∀ j, s.next ≤ j → s.payload j = none

/-- A write names no new identity, and so it keeps the store fresh. -/
theorem write_fresh (s : Store α) (id off : Nat) (buf : List α) (hf : Fresh s) : Fresh (write s id off buf) := by
  intro j hj
  rw [write_next] at hj
  by_cases hjc : j = id
  · subst hjc
    simp only [write, hf j hj]
  · rw [write_other s id off buf j hjc]; exact hf j hj

/-! ## The vector -/

/-- The internal state of a vector. The record behind its token holds it, and the caller never sees it. It holds the
identity of its movable cell, its count, its capacity, and the ceiling that its bounded pool sets (`poolLimit`). -/
structure Vec where
  cell : Nat
  size : Nat
  cap : Nat
  limit : Nat

/-- A vector sound in a store: its identity names a payload of its capacity. Its count is at max its capacity, and its
capacity is at max its ceiling. -/
def Sound (s : Store α) (v : Vec) : Prop :=
  ∃ p, s.payload v.cell = some p ∧ p.len = v.cap ∧ v.size ≤ v.cap ∧ v.cap ≤ v.limit

/-- The elements of a vector, in their order, as `std::vector` holds them. -/
def elems (s : Store α) (v : Vec) : List α :=
  match s.payload v.cell with
  | some p => slice p 0 v.size
  | none => []

omit [Inhabited α] in
theorem elems_of (s : Store α) (v : Vec) (p : Payload α) (hp : s.payload v.cell = some p) :
    elems s v = slice p 0 v.size := by
  simp [elems, hp]

omit [Inhabited α] in
/-- The mover changes no element of a vector. -/
theorem move_elems (s : Store α) (to : Nat → Nat) (v : Vec) : elems (moved s to) v = elems s v := rfl

/-- `at`: the element at index `i`, read out to an element of the caller, none past the count. -/
def get (s : Store α) (v : Vec) (i : Nat) : Option α :=
  if i < v.size then (read s v.cell i 1).bind List.head? else none

omit [Inhabited α] in
theorem get_elems (s : Store α) (v : Vec) (i : Nat) (hs : Sound s v) : get s v i = (elems s v)[i]? := by
  obtain ⟨p, hp, hlen, hsize, hlim⟩ := hs
  rw [elems_of s v p hp]
  unfold get
  split
  · next hi =>
    rw [read_fits s v.cell i 1 p hp (by omega)]
    simp [slice, hi]
  · next hi =>
    symm
    rw [List.getElem?_eq_none_iff, slice_length]
    omega

/-- A write of one element from the caller: the element at index `i` replaced, nothing changed past the count. -/
def set (s : Store α) (v : Vec) (i : Nat) (x : α) : Store α := if i < v.size then write s v.cell i [x] else s

theorem set_elems (s : Store α) (v : Vec) (i : Nat) (x : α) (hs : Sound s v) :
    elems (set s v i x) v = (elems s v).set i x := by
  obtain ⟨p, hp, hlen, hsize, hlim⟩ := hs
  unfold set
  split
  · next hi =>
    rw [elems_of _ v _ (write_here s v.cell i [x] p hp (by simp; omega)), elems_of s v p hp]
    apply List.ext_getElem
    · simp [slice]
    · intro j h1 h2
      simp only [slice, List.getElem_map, List.getElem_range, List.getElem_set, spliced] at *
      by_cases hj : i = j
      · subst hj; simp
      · simp [hj]; intro h; omega
  · next hi =>
    rw [elems_of s v p hp]
    simp only [slice]
    rw [List.set_eq_of_length_le]
    simp; omega

/-- The change of one element through the stack. The element goes out to a local of the caller, the caller changes
it, and the changed element goes back. The movable option has no copy from one cell to another and no change in
place. So the local on the stack is the one place where the change happens. -/
def modify (s : Store α) (v : Vec) (i : Nat) (f : α → α) : Store α :=
  match get s v i with
  | some y => set s v i (f y)
  | none => s

theorem modify_elems (s : Store α) (v : Vec) (i : Nat) (f : α → α) (hs : Sound s v) (hi : i < v.size) :
    elems (modify s v i f) v = (elems s v).set i (f ((elems s v)[i]!)) := by
  unfold modify
  rw [get_elems s v i hs]
  have hl : i < (elems s v).length := by
    obtain ⟨p, hp, _, _, _⟩ := hs
    rw [elems_of s v p hp, slice_length]; exact hi
  rw [List.getElem?_eq_getElem hl]
  simp only
  rw [set_elems s v i _ hs, List.getElem!_eq_getElem?_getD, List.getElem?_eq_getElem hl]
  rfl

/-- `pop_back`: the last element removed. The cell keeps its bytes, and a later write puts a new element there. -/
def pop (v : Vec) : Vec := { v with size := v.size - 1 }

omit [Inhabited α] in
theorem pop_elems (s : Store α) (v : Vec) (hs : Sound s v) : elems s (pop v) = (elems s v).dropLast := by
  obtain ⟨p, hp, _, _, _⟩ := hs
  rw [elems_of s (pop v) p hp, elems_of s v p hp]
  simp only [pop, slice]
  cases h : v.size with
  | zero => simp
  | succ n => simp [List.range_succ]

/-! ### The growth through a buffer on the stack -/

/-- A copy of `rem` elements from `off` of the cell `src` to the same place of the cell `dst`. Each step reads a
chunk of at max `k` elements out to a buffer on the stack and writes the buffer to `dst`. -/
def copyRun (k : Nat) (hk : 0 < k) (s : Store α) (src dst off rem : Nat) : Store α :=
  if _h : rem = 0 then s
  else
    match read s src off (min k rem) with
    | some buf => copyRun k hk (write s dst off buf) src dst (off + min k rem) (rem - min k rem)
    | none => s
termination_by rem
decreasing_by omega

/-- What a copy leaves. Each cell but `dst` is as it was, and `dst` keeps its length. The span of the copy holds the
elements of `ps`, and the rest of `dst` is as it was. -/
def Copied (s t : Store α) (dst off rem : Nat) (ps pd : Payload α) : Prop :=
  t.next = s.next ∧ (∀ y, y ≠ dst → t.payload y = s.payload y) ∧
    ∃ q, t.payload dst = some q ∧ q.len = pd.len ∧
      (∀ j, off ≤ j → j < off + rem → q.elem j = ps.elem j) ∧
      (∀ j, (j < off ∨ off + rem ≤ j) → q.elem j = pd.elem j)

/-- A copy through the stack copies each element of the span, for each chunk size. -/
theorem copyRun_copies (k : Nat) (hk : 0 < k) (src dst : Nat) (hne : src ≠ dst) :
    ∀ rem (s : Store α) off ps pd, s.payload src = some ps → s.payload dst = some pd →
      off + rem ≤ ps.len → off + rem ≤ pd.len → Copied s (copyRun k hk s src dst off rem) dst off rem ps pd := by
  intro rem
  induction rem using Nat.strongRecOn with
  | _ rem ih =>
    intro s off ps pd hps hpd hsrc hdst
    rw [copyRun]
    split
    · next h0 =>
      subst h0
      exact ⟨rfl, fun _ _ => rfl, pd, hpd, rfl, fun j h1 h2 => by omega, fun _ _ => rfl⟩
    · next h0 =>
      have hc : 0 < min k rem := by omega
      rw [read_fits s src off (min k rem) ps hps (by omega)]
      simp only
      let buf := slice ps off (min k rem)
      have hbuf : buf.length = min k rem := slice_length ps off _
      let s1 := write s dst off buf
      have h1d : s1.payload dst = some (spliced pd off buf) := write_here s dst off buf pd hpd (by omega)
      have h1s : s1.payload src = some ps := by rw [write_other s dst off buf src hne]; exact hps
      have hstep := ih (rem - min k rem) (by omega) s1 (off + min k rem) ps (spliced pd off buf) h1s h1d
        (by omega) (by simp [spliced]; omega)
      obtain ⟨hn, hothers, q, hq, hqlen, hin, hout⟩ := hstep
      refine ⟨by rw [hn]; exact write_next s dst off buf, fun y hy => by rw [hothers y hy]; exact write_other s dst off buf y hy,
        q, hq, by rw [hqlen]; rfl, ?_, ?_⟩
      · intro j hj1 hj2
        by_cases hjc : j < off + min k rem
        · rw [hout j (Or.inl hjc)]
          simp only [spliced, hbuf]
          rw [ite_eq_left ⟨hj1, hjc⟩, slice_getD ps off _ (j - off) (by omega)]
          congr 1; omega
        · exact hin j (by omega) (by omega)
      · intro j hj
        rw [hout j (by omega)]
        simp only [spliced, hbuf]
        rw [ite_eq_right (by omega)]

/-- The capacity after a growth: twice the capacity, 1 for a capacity of 0, and never past the ceiling. -/
def grown (cap limit : Nat) : Nat := min (if cap = 0 then 1 else 2 * cap) limit

theorem grown_gt (cap limit : Nat) (h : cap < limit) : cap < grown cap limit := by
  unfold grown; split <;> omega

theorem grown_le (cap limit : Nat) : grown cap limit ≤ limit := Nat.min_le_right _ _

/-- A relocation of a vector to a new cell of `newCap` elements. It takes the new cell, copies the elements to it
through the stack, and gives the old cell back. The record of the vector names the new cell. A growth is a relocation
to the grown capacity, and an internal compaction is a relocation to the same capacity. -/
def regrow (k : Nat) (hk : 0 < k) (s : Store α) (v : Vec) (d : α) (newCap : Nat) : Store α × Vec :=
  let taken := alloc s newCap d
  (release (copyRun k hk taken.1 v.cell taken.2 0 v.size) v.cell, { v with cell := taken.2, cap := newCap })

/-- A relocation keeps every element, in its order, and leaves a sound vector of the new capacity. It changes the
payload of no identity but the old cell, which it gives back, and the new cell, which was fresh. -/
theorem regrow_elems (k : Nat) (hk : 0 < k) (s : Store α) (v : Vec) (d : α) (newCap : Nat) (hs : Sound s v)
    (hf : Fresh s) (hsz : v.size ≤ newCap) (hlim : newCap ≤ v.limit) :
    elems (regrow k hk s v d newCap).1 (regrow k hk s v d newCap).2 = elems s v ∧
      Sound (regrow k hk s v d newCap).1 (regrow k hk s v d newCap).2 ∧ Fresh (regrow k hk s v d newCap).1 ∧
      (∀ j, j ≠ v.cell → j < s.next → (regrow k hk s v d newCap).1.payload j = s.payload j) := by
  obtain ⟨p, hp, hlen, hsize, _⟩ := hs
  have hold : v.cell < s.next := by
    apply Nat.lt_of_not_le
    intro h
    rw [hf v.cell h] at hp
    cases hp
  have hne : v.cell ≠ s.next := by omega
  let s1 := (alloc s newCap d).1
  have h1src : s1.payload v.cell = some p := by simp [s1, alloc, update, hne, hp]
  have h1dst : s1.payload s.next = some ⟨newCap, fun _ => d⟩ := by simp [s1, alloc, update]
  have hc := copyRun_copies k hk v.cell s.next hne v.size s1 0 p ⟨newCap, fun _ => d⟩ h1src h1dst
    (by omega) (by simp; omega)
  obtain ⟨hn, hothers, q, hq, hqlen, hin, _⟩ := hc
  have hq' : (regrow k hk s v d newCap).1.payload s.next = some q := by
    simp only [regrow, release, update]
    rw [ite_eq_right (Ne.symm hne)]
    exact hq
  refine ⟨?_, ⟨q, hq', by simp [regrow]; exact hqlen, by simp [regrow]; omega, by simp [regrow]; omega⟩, ?_, ?_⟩
  · rw [elems_of _ _ q hq', elems_of s v p hp]
    simp only [regrow, slice]
    apply List.map_congr_left
    intro j hj
    simp at hj
    simp only [Nat.zero_add]
    exact hin j (Nat.zero_le _) (by omega)
  · intro j hj
    have hj' : (copyRun k hk s1 v.cell s.next 0 v.size).next ≤ j := hj
    rw [hn] at hj'
    simp only [regrow, release, update]
    split
    · rfl
    · next hjc =>
      have hjn : j ≠ s.next := by simp [s1, alloc] at hj'; omega
      change (copyRun k hk s1 v.cell s.next 0 v.size).payload j = none
      rw [hothers j hjn]
      simp only [s1, alloc, update, hjn, ↓reduceIte]
      exact hf j (by simp [s1, alloc] at hj'; omega)
  · intro j hjc hjl
    have hjn : j ≠ s.next := by omega
    simp only [regrow, release, update, hjc, ↓reduceIte]
    change (copyRun k hk s1 v.cell s.next 0 v.size).payload j = s.payload j
    rw [hothers j hjn]
    simp only [s1, alloc, update, hjn, ↓reduceIte]

/-- A growth: a relocation to the grown capacity. -/
def grow (k : Nat) (hk : 0 < k) (s : Store α) (v : Vec) (d : α) : Store α × Vec :=
  regrow k hk s v d (grown v.cap v.limit)

/-- A growth below the ceiling keeps every element, in its order. It leaves a sound vector of more capacity, at max
the ceiling. -/
theorem grow_elems (k : Nat) (hk : 0 < k) (s : Store α) (v : Vec) (d : α) (hs : Sound s v) (hf : Fresh s)
    (hroom : v.cap < v.limit) :
    elems (grow k hk s v d).1 (grow k hk s v d).2 = elems s v ∧ Sound (grow k hk s v d).1 (grow k hk s v d).2 ∧
      v.cap < (grow k hk s v d).2.cap ∧ Fresh (grow k hk s v d).1 := by
  have hgt := grown_gt v.cap v.limit hroom
  have hle := grown_le v.cap v.limit
  have hsz : v.size ≤ grown v.cap v.limit := by obtain ⟨_, _, _, h, _⟩ := hs; omega
  obtain ⟨he, hsd, hfr, _⟩ := regrow_elems k hk s v d _ hs hf hsz hle
  exact ⟨he, hsd, by simp [grow, regrow]; exact hgt, hfr⟩

/-- `push_back`: the element written past the last one, after a growth where the cell is full. A vector at its
ceiling refuses the element, and the store is as it was. -/
def push (k : Nat) (hk : 0 < k) (s : Store α) (v : Vec) (x d : α) : Option (Store α × Vec) :=
  if v.size < v.cap then some (write s v.cell v.size [x], { v with size := v.size + 1 })
  else if v.cap < v.limit then
    let g := grow k hk s v d
    some (write g.1 g.2.cell g.2.size [x], { g.2 with size := g.2.size + 1 })
  else none

/-- An element written past the last one of a vector with room for it. -/
theorem append_elems (s : Store α) (v : Vec) (x : α) (hs : Sound s v) (hroom : v.size < v.cap) :
    elems (write s v.cell v.size [x]) { v with size := v.size + 1 } = elems s v ++ [x] ∧
      Sound (write s v.cell v.size [x]) { v with size := v.size + 1 } := by
  obtain ⟨p, hp, hlen, hsize, hlim⟩ := hs
  have hw := write_here s v.cell v.size [x] p hp (by simp; omega)
  refine ⟨?_, ⟨_, hw, by simp [spliced]; exact hlen, by simp; omega, hlim⟩⟩
  rw [elems_of _ { v with size := v.size + 1 } _ hw, elems_of s v p hp]
  simp only [slice, List.range_succ, List.map_append, List.map_cons, List.map_nil, spliced]
  congr 1
  · apply List.map_congr_left
    intro j hj
    simp at hj
    simp; intro h; omega
  · simp

/-- `push_back` below the ceiling leaves the elements and the new one after them, and a sound vector. Its count rises
by one, its ceiling stays, and the store keeps its identities fresh. -/
theorem push_elems (k : Nat) (hk : 0 < k) (s : Store α) (v : Vec) (x d : α) (hs : Sound s v) (hf : Fresh s)
    (s' : Store α) (v' : Vec) (h : push k hk s v x d = some (s', v')) :
    elems s' v' = elems s v ++ [x] ∧ Sound s' v' ∧ v'.size = v.size + 1 ∧ v'.limit = v.limit ∧ Fresh s' := by
  unfold push at h
  split at h
  · next hroom =>
    simp only [Option.some.injEq, Prod.mk.injEq] at h
    obtain ⟨rfl, rfl⟩ := h
    exact ⟨(append_elems s v x hs hroom).1, (append_elems s v x hs hroom).2, rfl, rfl, write_fresh s _ _ _ hf⟩
  · next hroom =>
    split at h
    · next hceil =>
      simp only [Option.some.injEq, Prod.mk.injEq] at h
      obtain ⟨rfl, rfl⟩ := h
      obtain ⟨he, hg, hcap, hgf⟩ := grow_elems k hk s v d hs hf hceil
      have hfull : (grow k hk s v d).2.size < (grow k hk s v d).2.cap := by
        obtain ⟨_, _, _, hsize, _⟩ := hs
        have : (grow k hk s v d).2.size = v.size := rfl
        omega
      have ha := append_elems (grow k hk s v d).1 (grow k hk s v d).2 x hg hfull
      exact ⟨by rw [ha.1, he], ha.2, rfl, rfl, write_fresh _ _ _ _ hgf⟩
    · cases h

/-- `push_back` refuses an element only at the ceiling: a vector whose count is its capacity and whose capacity is its
ceiling. A refused element changes no state. -/
theorem push_refused (k : Nat) (hk : 0 < k) (s : Store α) (v : Vec) (x d : α) (hs : Sound s v) :
    push k hk s v x d = none ↔ v.size = v.limit := by
  obtain ⟨_, _, _, hsize, hlim⟩ := hs
  unfold push
  split
  · simp; omega
  · split
    · simp; omega
    · simp; omega

omit [Inhabited α] in
/-- `size`: the count of elements, which is the length of the elements. -/
theorem size_elems (s : Store α) (v : Vec) (hs : Sound s v) : (elems s v).length = v.size := by
  obtain ⟨p, hp, _, _, _⟩ := hs
  simp [elems, hp, slice]

/-! ## The tiers that give out pointers -/

variable (H : Nat)

/-- The pointers of a tier from offset `base`: the offset of each busy cell, with the cell. A take of the persistent
or the temporary tier gives the program the address of its payload, and the program retains it. So each of these is
an address that a change of the layout must keep. -/
def pointers : Nat → List Cell → List (Nat × Cell)
  | _, [] => []
  | base, c :: rest => (if c.busy then [(base, c)] else []) ++ pointers (base + foot H c) rest

/-- The bytes that the busy cells of a tier position. -/
def busyBytes (l : List Cell) : Nat := ((busyCells l).map (foot H)).sum

/-- The bytes of the holes: the free bytes below the end of the last busy cell. -/
def holes (l : List Cell) : Nat := len H (trimEnd l) - busyBytes H l

/-- The end of the last pointer, `base` where there is none. -/
def reach (base : Nat) (ps : List (Nat × Cell)) : Nat :=
  match ps.getLast? with
  | some p => p.1 + foot H p.2
  | none => base

theorem pointers_nil (base : Nat) (l : List Cell) : pointers H base l = [] ↔ busyCells l = [] := by
  induction l generalizing base with
  | nil => simp [pointers, busyCells]
  | cons c rest ih => cases hc : c.busy <;> simp [pointers, busyCells, hc, ih]

theorem trimEnd_nil (l : List Cell) : trimEnd l = [] ↔ busyCells l = [] := by
  induction l with
  | nil => simp [trimEnd, busyCells]
  | cons c rest ih =>
    simp only [trimEnd]
    split
    · next h => rw [h] at ih; cases hc : c.busy <;> simp [busyCells, hc] <;> exact ih.1 rfl
    · next d tail h =>
      rw [h] at ih
      have : busyCells rest ≠ [] := fun hb => by simp at ih; exact ih hb
      cases hc : c.busy <;> simp [busyCells, hc, this]

theorem busyBytes_cons (c : Cell) (rest : List Cell) :
    busyBytes H (c :: rest) = (if c.busy then foot H c else 0) + busyBytes H rest := by
  cases hc : c.busy <;> simp [busyBytes, busyCells, hc]

/-- The busy bytes come from the pointers alone. -/
theorem busyBytes_pointers (base : Nat) (l : List Cell) :
    busyBytes H l = ((pointers H base l).map fun p => foot H p.2).sum := by
  induction l generalizing base with
  | nil => simp [busyBytes, busyCells, pointers]
  | cons c rest ih =>
    rw [busyBytes_cons, ih (base + foot H c)]
    cases hc : c.busy <;> simp [pointers, hc]

/-- The end of the last busy cell comes from the pointers alone. -/
theorem reach_pointers (base : Nat) (l : List Cell) :
    base + len H (trimEnd l) = reach H base (pointers H base l) := by
  induction l generalizing base with
  | nil => simp [trimEnd, len, pointers, reach]
  | cons c rest ih =>
    have hih := ih (base + foot H c)
    simp only [trimEnd]
    split
    · next h =>
      have hb : busyCells rest = [] := (trimEnd_nil rest).1 h
      have hp : pointers H (base + foot H c) rest = [] := (pointers_nil H _ rest).2 hb
      cases hc : c.busy <;> simp [pointers, hc, hp, len, reach]
    · next d tail h =>
      have hb : busyCells rest ≠ [] := fun hb => by rw [(trimEnd_nil rest).2 hb] at h; cases h
      have hp : pointers H (base + foot H c) rest ≠ [] := fun hp => hb ((pointers_nil H _ rest).1 hp)
      rw [h] at hih
      have hlast : ((if c.busy then [(base, c)] else []) ++ pointers H (base + foot H c) rest).getLast? =
          (pointers H (base + foot H c) rest).getLast? := by
        rw [List.getLast?_append]
        cases hq : (pointers H (base + foot H c) rest).getLast? with
        | none => rw [List.getLast?_eq_none_iff] at hq; exact absurd hq hp
        | some q => simp
      obtain ⟨q, hq⟩ : ∃ q, (pointers H (base + foot H c) rest).getLast? = some q := by
        cases hq : (pointers H (base + foot H c) rest).getLast? with
        | none => rw [List.getLast?_eq_none_iff] at hq; exact absurd hq hp
        | some q => exact ⟨q, rfl⟩
      simp only [pointers, reach, hlast, hq]
      simp only [reach, hq] at hih
      simp only [len] at hih ⊢
      omega

/-- The fragmentation stays: two layouts of a tier that give the same pointers have the same bytes of holes. A change
of the layout that keeps each pointer the program retains can move no busy cell, and so it closes no hole. -/
theorem fragmentation_stays (l l' : List Cell) (h : pointers H 0 l = pointers H 0 l') : holes H l = holes H l' := by
  have hr := reach_pointers H 0 l
  have hr' := reach_pointers H 0 l'
  unfold holes
  rw [busyBytes_pointers H 0 l, busyBytes_pointers H 0 l', h]
  rw [h] at hr
  omega

/-- A tier with a hole: a free cell between two busy ones. Its bytes stay as long as both pointers do. -/
theorem fragmentation_happens : holes H [⟨8, true, 1⟩, ⟨8, false, 0⟩, ⟨8, true, 1⟩] = H + 8 := by
  simp [holes, trimEnd, len, busyBytes, busyCells, foot] <;> omega

/-! ## Where the holes come from -/

/-- A free run of the size of a request takes it whole, with no free rest. The words of a cell or a rest take a byte
or more. So a hole of the size of the slabs of a tier fills again at the next take. -/
theorem exact_refill (A want owner z : Nat) (h : 0 < H + A) :
    LocusCarcerum.Take.positioned H A want owner ⟨want, false, z⟩ = [⟨want, true, owner⟩] := by
  simp [LocusCarcerum.Take.positioned]; omega

theorem busyBytes_append (l : List Cell) (c : Cell) (hc : c.busy = true) :
    busyBytes H (l ++ [c]) = busyBytes H l + foot H c := by
  induction l with
  | nil => simp [busyBytes, busyCells, hc]
  | cons d rest ih => rw [List.cons_append, busyBytes_cons, busyBytes_cons, ih]; omega

theorem trimEnd_append_busy (l : List Cell) (c : Cell) (hc : c.busy = true) : trimEnd (l ++ [c]) = l ++ [c] := by
  induction l with
  | nil => simp [trimEnd, hc]
  | cons d rest ih =>
    rw [List.cons_append]
    simp only [trimEnd, ih]
    cases rest with
    | nil => rfl
    | cons e tail => rfl

theorem len_append (l m : List Cell) : len H (l ++ m) = len H l + len H m := by
  induction l with
  | nil => simp [len]
  | cons c rest ih => simp [len, ih]; omega

/-- Odd slabs leave holes that stay. Where no free cell of the persistent tier holds a request, the take bumps the end,
and each hole lies under the new cell. The bytes of the holes do not fall. -/
theorem odd_hole_stays (l : List Cell) (want owner : Nat) :
    holes H l ≤ holes H (l ++ [⟨want, true, owner⟩]) := by
  unfold holes
  rw [trimEnd_append_busy l _ rfl, busyBytes_append H l _ rfl, len_append]
  have := trimEnd_len H l
  simp [len]
  omega

/-- A tier whose cells are all busy. -/
def AllBusy (l : List Cell) : Prop := ∀ c ∈ l, c.busy = true

theorem trimEnd_allBusy (l : List Cell) (h : AllBusy l) : trimEnd l = l := by
  induction l with
  | nil => rfl
  | cons c rest ih =>
    have hc : c.busy = true := h c (by simp)
    have hr : AllBusy rest := fun d hd => h d (by simp [hd])
    simp only [trimEnd, ih hr]
    cases rest with
    | nil => simp [hc]
    | cons e tail => rfl

/-- A tier whose cells are all busy has no hole. -/
theorem allBusy_no_holes (l : List Cell) (h : AllBusy l) : holes H l = 0 := by
  unfold holes
  rw [trimEnd_allBusy l h]
  have : len H l = busyBytes H l := by
    induction l with
    | nil => rfl
    | cons c rest ih =>
      have hc : c.busy = true := h c (by simp)
      rw [busyBytes_cons, ite_eq_left hc]
      simp only [len]
      rw [ih (fun d hd => h d (by simp [hd]))]
  omega

/-- A step of the temporary tier: a take at the top, or a give of the cell at the top. -/
inductive TempStep where
  | take (n owner : Nat)
  | giveTop

/-- The temporary tier after a step, from its base. -/
def tempStep (l : List Cell) : TempStep → List Cell
  | .take n owner => l ++ [⟨n, true, owner⟩]
  | .giveTop => l.dropLast

/-- Temporary cells given back within the call that took them leave no hole. Each give is of the cell at the top, as a
stack gives, and so the tier stays a run of busy cells. -/
theorem lifo_no_holes (steps : List TempStep) : holes H (steps.foldl tempStep []) = 0 := by
  apply allBusy_no_holes
  suffices h : ∀ l, AllBusy l → AllBusy (steps.foldl tempStep l) from h [] (fun _ hc => by cases hc)
  induction steps with
  | nil => intro l hl; exact hl
  | cons step rest ih =>
    intro l hl
    apply ih
    cases step with
    | take n owner =>
      intro c hc
      simp only [tempStep, List.mem_append, List.mem_singleton] at hc
      rcases hc with hc | hc
      · exact hl c hc
      · rw [hc]
    | giveTop => intro c hc; exact hl c ((List.dropLast_sublist l).subset hc)

/-- A temporary cell held past a later take puts a hole in the tier. The give of the cell under it leaves its bytes
free below a busy cell. They stay a hole until the held cell goes back. -/
theorem held_cell_hole (low high owner : Nat) : holes H [⟨low, false, 0⟩, ⟨high, true, owner⟩] = H + low := by
  simp [holes, trimEnd, len, busyBytes, busyCells, foot] <;> omega

/-! ## The bounded pool -/

/-- The bytes that a movable cell of `cap` elements takes in the pool: its header and owner word, and the elements. -/
def cellBytes (stride cap : Nat) : Nat := H + cap * stride

/-- The ceiling that a pool of `bytes` gives a vector. A growth holds two cells at once, the old one and the new one,
for its copy. So the ceiling is the largest capacity whose cell fits in half of the pool. -/
def poolLimit (stride bytes : Nat) : Nat := (bytes / 2 - H) / stride

/-- Each growth fits the pool. Where both capacities are at max the ceiling, the two cells of the growth fit in the
pool together. -/
theorem poolLimit_peak (stride bytes cap cap' : Nat) (hH : H ≤ bytes / 2)
    (h1 : cap ≤ poolLimit H stride bytes) (h2 : cap' ≤ poolLimit H stride bytes) :
    cellBytes H stride cap + cellBytes H stride cap' ≤ bytes := by
  unfold cellBytes poolLimit at *
  have hm := Nat.div_mul_le_self (bytes / 2 - H) stride
  have hc1 : cap * stride ≤ (bytes / 2 - H) / stride * stride := Nat.mul_le_mul_right stride h1
  have hc2 : cap' * stride ≤ (bytes / 2 - H) / stride * stride := Nat.mul_le_mul_right stride h2
  have hhalf := Nat.div_mul_le_self bytes 2
  omega

/-- The ceiling is the largest: a cell of one element more does not fit in half of the pool. -/
theorem poolLimit_max (stride bytes : Nat) (hst : 0 < stride) (hH : H ≤ bytes / 2) :
    bytes / 2 < cellBytes H stride (poolLimit H stride bytes + 1) := by
  unfold cellBytes poolLimit
  have hd := Nat.div_add_mod (bytes / 2 - H) stride
  have hmod := Nat.mod_lt (bytes / 2 - H) hst
  rw [Nat.add_mul, Nat.one_mul, Nat.mul_comm ((bytes / 2 - H) / stride) stride]
  omega

omit [Inhabited α] in
/-- The cell of a sound vector over a pool fits the pool, and so does each growth it makes. -/
theorem sound_fits (s : Store α) (v : Vec) (stride bytes : Nat) (hs : Sound s v) (hH : H ≤ bytes / 2)
    (hceil : v.limit = poolLimit H stride bytes) :
    cellBytes H stride v.cap + cellBytes H stride (grown v.cap v.limit) ≤ bytes := by
  obtain ⟨_, _, _, _, hlim⟩ := hs
  exact poolLimit_peak H stride bytes _ _ hH (by omega) (by have := grown_le v.cap v.limit; omega)

/-! ## Bounded alignment -/

/-- `n` rounded up to a multiple of `a`. -/
def roundUp (n a : Nat) : Nat := (n + a - 1) / a * a

theorem roundUp_ge (n a : Nat) (ha : 0 < a) : n ≤ roundUp n a := by
  unfold roundUp
  have hd := Nat.div_add_mod (n + a - 1) a
  have hmod := Nat.mod_lt (n + a - 1) ha
  rw [Nat.mul_comm]
  omega

/-- The padding of an element is less than its alignment. -/
theorem roundUp_lt (n a : Nat) (ha : 0 < a) : roundUp n a < n + a := by
  unfold roundUp
  have := Nat.div_mul_le_self (n + a - 1) a
  omega

theorem roundUp_dvd (n a : Nat) : a ∣ roundUp n a := Nat.dvd_mul_left a _

/-- Each element lies at an offset of its alignment. The stride of an element is its size rounded up to its alignment.
The alignment divides the alignment of the pool, and the payload starts at an offset of the alignment of the pool. -/
theorem elem_aligned (size align poolAlign base i : Nat) (hdiv : align ∣ poolAlign) (hbase : poolAlign ∣ base) :
    align ∣ base + i * roundUp size align := by
  apply Nat.dvd_add (Nat.dvd_trans hdiv hbase)
  exact Nat.dvd_trans (roundUp_dvd size align) (Nat.dvd_mul_left _ i)

/-- The alignment of the pool bounds the alignment of an element: that alignment is at max the alignment of the pool. -/
theorem align_bounded (align poolAlign : Nat) (hdiv : align ∣ poolAlign) (hpool : 0 < poolAlign) :
    align ≤ poolAlign := Nat.le_of_dvd hpool hdiv

/-! ## The movable option closes its holes -/

/-- The movable cells end to end: each a busy cell of its payload, with no free cell between two. -/
def packedTier (sizes : List Nat) : List Cell := sizes.map fun n => ⟨n, true, 0⟩

theorem trimEnd_busy_list (sizes : List Nat) : trimEnd (packedTier sizes) = packedTier sizes := by
  induction sizes with
  | nil => rfl
  | cons n rest ih =>
    show trimEnd (⟨n, true, 0⟩ :: packedTier rest) = ⟨n, true, 0⟩ :: packedTier rest
    simp only [trimEnd, ih]
    cases rest with
    | nil => rfl
    | cons m tail => rfl

/-- A packed tier has no hole. The mover can give the movable cells such a placement. -/
theorem packed_no_holes (sizes : List Nat) : holes H (packedTier sizes) = 0 := by
  unfold holes
  rw [trimEnd_busy_list]
  have : ∀ s : List Nat, len H (packedTier s) = busyBytes H (packedTier s) := by
    intro s
    induction s with
    | nil => rfl
    | cons n rest ih => simp [packedTier, len, busyBytes_cons] at ih ⊢; omega
  rw [this]; omega

omit [Inhabited α] in
/-- The mover can give each identity any offset, a packed placement among them, and each vector reads the elements it
read before. An identity is not an address, and so the move breaks no reference that the program retains. -/
theorem compaction_keeps (s : Store α) (to : Nat → Nat) (vs : List Vec) :
    vs.map (elems (moved s to)) = vs.map (elems s) := rfl

/-! ## The vector behind the guard -/

/-- The records behind the tokens. The guard holds them with the movable option, and the caller holds the tokens
alone. -/
structure Guarded (α : Type) where
  store : Store α
  records : Nat → Option Vec

/-- A call through the guard: the operation on the vector that the token names. -/
inductive Call (α : Type) where
  | get (n : Nat)
  | set (n : Nat) (x : α)
  | push (x : α)
  | pop
  | size

/-- What the caller gets back: an element, a count, a commit, or a refusal. An answer holds no identity and no
address. -/
inductive Answer (α : Type) where
  | elem (x : α)
  | count (n : Nat)
  | done
  | refused

/-- A commit: the store and the record of the token, as the CAS of one call leaves them. -/
def commit (g : Guarded α) (t : Nat) (s : Store α) (v : Vec) : Guarded α := ⟨s, update g.records t (some v)⟩

/-- One call through the guard. The guard passes the token to its record. The record decides if the call is legal:
the span test of `vec[n]` for a read or a write, the ceiling for `push_back`, and the count for `pop_back`. Then the
call commits, or it gives a refusal and changes nothing. -/
def step (k : Nat) (hk : 0 < k) (d : α) (Wd stride : Nat) (g : Guarded α) (t : Nat) (c : Call α) :
    Guarded α × Answer α :=
  match g.records t with
  | none => (g, .refused)
  | some v =>
    match c with
    | .get n =>
      if Guard.legal Wd stride v.size n then
        match get g.store v n with
        | some x => (g, .elem x)
        | none => (g, .refused)
      else (g, .refused)
    | .set n x => if Guard.legal Wd stride v.size n then (commit g t (set g.store v n x) v, .done) else (g, .refused)
    | .push x =>
      match push k hk g.store v x d with
      | some r => (commit g t r.1 r.2, .done)
      | none => (g, .refused)
    | .pop => if 0 < v.size then (commit g t g.store (pop v), .done) else (g, .refused)
    | .size => (g, .count v.size)

/-- What the caller can know of the vector behind a token: its elements and its ceiling. -/
def view (g : Guarded α) (t : Nat) : Option (List α × Nat) := (g.records t).map fun v => (elems g.store v, v.limit)

/-- A sound guard: the store fresh, each record sound, and no two records with one cell. -/
structure Inv (g : Guarded α) : Prop where
  fresh : Fresh g.store
  sound : ∀ t v, g.records t = some v → Sound g.store v
  apart : ∀ t u v w, t ≠ u → g.records t = some v → g.records u = some w → v.cell ≠ w.cell

/-- Two guards that the caller cannot tell apart: the same view through each token. -/
def Same (g h : Guarded α) : Prop := ∀ t, view g t = view h t

/-- The answer of a call from the view alone: the elements `l` and the ceiling `lim`. -/
def answerOf (Wd stride : Nat) (c : Call α) (l : List α) (lim : Nat) : Answer α :=
  match c with
  | .get n =>
    if Guard.legal Wd stride l.length n then
      match l[n]? with
      | some x => .elem x
      | none => .refused
    else .refused
  | .set n _ => if Guard.legal Wd stride l.length n then .done else .refused
  | .push _ => if l.length < lim then .done else .refused
  | .pop => if 0 < l.length then .done else .refused
  | .size => .count l.length

/-- The elements after a call, from the view alone. -/
def listOf (Wd stride : Nat) (c : Call α) (l : List α) (lim : Nat) : List α :=
  match c with
  | .get _ => l
  | .set n x => if Guard.legal Wd stride l.length n then l.set n x else l
  | .push x => if l.length < lim then l ++ [x] else l
  | .pop => l.dropLast
  | .size => l

omit [Inhabited α] in
theorem elems_congr (s s' : Store α) (v : Vec) (h : s'.payload v.cell = s.payload v.cell) :
    elems s' v = elems s v := by
  simp only [elems, h]

omit [Inhabited α] in
theorem sound_congr (s s' : Store α) (v : Vec) (h : s'.payload v.cell = s.payload v.cell) :
    Sound s' v ↔ Sound s v := by
  simp only [Sound, h]

omit [Inhabited α] in
theorem cell_lt (s : Store α) (v : Vec) (hs : Sound s v) (hf : Fresh s) : v.cell < s.next := by
  obtain ⟨p, hp, _⟩ := hs
  apply Nat.lt_of_not_le
  intro h
  rw [hf v.cell h] at hp
  cases hp

omit [Inhabited α] in
theorem elems_length (s : Store α) (v : Vec) (hs : Sound s v) : (elems s v).length = v.size := by
  obtain ⟨p, hp, _⟩ := hs
  rw [elems_of s v p hp, slice_length]

theorem set_payload (s : Store α) (v : Vec) (n : Nat) (x : α) (y : Nat) (hy : y ≠ v.cell) :
    (set s v n x).payload y = s.payload y := by
  unfold set
  split
  · exact write_other _ _ _ _ y hy
  · rfl

theorem set_fresh (s : Store α) (v : Vec) (n : Nat) (x : α) (hf : Fresh s) : Fresh (set s v n x) := by
  unfold set
  split
  · exact write_fresh _ _ _ _ hf
  · exact hf

theorem set_sound (s : Store α) (v : Vec) (n : Nat) (x : α) (hs : Sound s v) : Sound (set s v n x) v := by
  obtain ⟨p, hp, hlen, hsize, hlim⟩ := hs
  unfold set
  split
  · next hn => exact ⟨_, write_here s v.cell n [x] p hp (by simp; omega), hlen, hsize, hlim⟩
  · exact ⟨p, hp, hlen, hsize, hlim⟩

/-- `push_back` changes the payload of no cell but its own and a fresh one, and its vector names one of the two. -/
theorem push_keeps (k : Nat) (hk : 0 < k) (s : Store α) (v : Vec) (x d : α) (hs : Sound s v) (hf : Fresh s)
    (s' : Store α) (v' : Vec) (h : push k hk s v x d = some (s', v')) :
    (∀ y, y ≠ v.cell → y < s.next → s'.payload y = s.payload y) ∧ (v'.cell = v.cell ∨ v'.cell = s.next) := by
  unfold push at h
  split at h
  · simp only [Option.some.injEq, Prod.mk.injEq] at h
    obtain ⟨rfl, rfl⟩ := h
    exact ⟨fun y hy _ => write_other _ _ _ _ y hy, Or.inl rfl⟩
  · split at h
    · next hceil =>
      simp only [Option.some.injEq, Prod.mk.injEq] at h
      obtain ⟨rfl, rfl⟩ := h
      have hsz : v.size ≤ grown v.cap v.limit := by
        obtain ⟨_, _, _, h1, _⟩ := hs
        have := grown_gt v.cap v.limit hceil
        omega
      obtain ⟨_, _, _, hk4⟩ := regrow_elems k hk s v d _ hs hf hsz (grown_le _ _)
      refine ⟨fun y hy hyl => ?_, Or.inr rfl⟩
      have hne : y ≠ (grow k hk s v d).2.cell := by
        show y ≠ s.next
        omega
      rw [write_other _ _ _ _ y hne]
      exact hk4 y hy hyl
    · cases h

/-- `push_back` commits where the count is below the ceiling, and refuses at the ceiling. -/
theorem push_some (k : Nat) (hk : 0 < k) (s : Store α) (v : Vec) (x d : α) (hs : Sound s v) :
    (push k hk s v x d).isSome = decide (v.size < v.limit) := by
  have hr := push_refused k hk s v x d hs
  obtain ⟨_, _, _, hsize, hlim⟩ := hs
  cases hp : push k hk s v x d with
  | none => have := hr.1 hp; simp; omega
  | some r =>
    have : v.size ≠ v.limit := fun he => by rw [hr.2 he] at hp; cases hp
    simp; omega

omit [Inhabited α] in
theorem view_commit_here (g : Guarded α) (t : Nat) (s : Store α) (v : Vec) :
    view (commit g t s v) t = some (elems s v, v.limit) := by
  simp [view, commit, update]

omit [Inhabited α] in
/-- A commit keeps the guard sound and the view through each other token. The new store must change no cell of another
record, and the new record must name its old cell or a fresh one. -/
theorem commit_inv (g : Guarded α) (hg : Inv g) (t : Nat) (v : Vec) (hv : g.records t = some v) (s' : Store α)
    (v' : Vec) (hf : Fresh s') (hs : Sound s' v')
    (hkeep : ∀ y, y ≠ v.cell → y < g.store.next → s'.payload y = g.store.payload y)
    (hcell : v'.cell = v.cell ∨ v'.cell = g.store.next) :
    Inv (commit g t s' v') ∧ ∀ u, u ≠ t → view (commit g t s' v') u = view g u := by
  have hother : ∀ u w, u ≠ t → g.records u = some w → s'.payload w.cell = g.store.payload w.cell := fun u w hu hw =>
    hkeep w.cell (hg.apart u t w v hu hw hv) (cell_lt _ _ (hg.sound u w hw) hg.fresh)
  have hnew : ∀ u w, u ≠ t → g.records u = some w → v'.cell ≠ w.cell := by
    intro u w hu hw
    have h1 := hg.apart u t w v hu hw hv
    have h2 := cell_lt _ _ (hg.sound u w hw) hg.fresh
    rcases hcell with hc | hc <;> rw [hc] <;> omega
  refine ⟨⟨hf, ?_, ?_⟩, ?_⟩
  · intro u w hw
    simp only [commit, update] at hw
    split at hw
    · cases hw; exact hs
    · next hu => exact (sound_congr _ _ _ (hother u w hu hw)).2 (hg.sound u w hw)
  · intro u1 u2 w1 w2 hne h1 h2
    simp only [commit, update] at h1 h2
    split at h1 <;> split at h2
    · next ha hb => exact absurd (ha.trans hb.symm) hne
    · next ha hb => cases h1; exact hnew u2 w2 hb h2
    · next ha hb => cases h2; exact fun he => hnew u1 w1 ha h1 he.symm
    · next ha hb => exact hg.apart u1 u2 w1 w2 hne h1 h2
  · intro u hu
    simp only [view, commit, update, hu, ↓reduceIte]
    cases hw : g.records u with
    | none => rfl
    | some w => simp [elems_congr _ _ _ (hother u w hu hw)]

/-- One call through the guard. The answer and the elements after it come from the view of the token alone. The view
through each other token stays, and the guard stays sound. -/
theorem step_spec (k : Nat) (hk : 0 < k) (d : α) (Wd stride : Nat) (g : Guarded α) (hg : Inv g) (t : Nat) (v : Vec)
    (hv : g.records t = some v) (c : Call α) :
    (step k hk d Wd stride g t c).2 = answerOf Wd stride c (elems g.store v) v.limit ∧
      view (step k hk d Wd stride g t c).1 t = some (listOf Wd stride c (elems g.store v) v.limit, v.limit) ∧
      (∀ u, u ≠ t → view (step k hk d Wd stride g t c).1 u = view g u) ∧ Inv (step k hk d Wd stride g t c).1 := by
  have hs := hg.sound t v hv
  have hl := elems_length _ _ hs
  have hview : view g t = some (elems g.store v, v.limit) := by simp [view, hv]
  cases c with
  | get n =>
    have e : step k hk d Wd stride g t (.get n) = (g, answerOf Wd stride (.get n) (elems g.store v) v.limit) := by
      simp only [step, hv, answerOf, hl, ← get_elems _ _ n hs]
      split
      · split <;> rfl
      · rfl
    rw [e]
    exact ⟨rfl, hview, fun _ _ => rfl, hg⟩
  | set n x =>
    by_cases hn : Guard.legal Wd stride v.size n = true
    · have e : step k hk d Wd stride g t (.set n x) = (commit g t (set g.store v n x) v, .done) := by
        simp [step, hv, hn]
      rw [e]
      obtain ⟨hi, ho⟩ := commit_inv g hg t v hv _ v (set_fresh _ _ _ _ hg.fresh) (set_sound _ _ _ _ hs)
        (fun y hy _ => set_payload _ _ _ _ y hy) (Or.inl rfl)
      refine ⟨by simp [answerOf, hl, hn], ?_, ho, hi⟩
      rw [view_commit_here, set_elems _ _ _ _ hs]
      simp [listOf, hl, hn]
    · have e : step k hk d Wd stride g t (.set n x) = (g, .refused) := by simp [step, hv, hn]
      rw [e]
      exact ⟨by simp [answerOf, hl, hn], by simp [listOf, hl, hn, hview], fun _ _ => rfl, hg⟩
  | push x =>
    have hsome := push_some k hk g.store v x d hs
    cases hp : push k hk g.store v x d with
    | none =>
      have e : step k hk d Wd stride g t (.push x) = (g, .refused) := by simp [step, hv, hp]
      rw [hp] at hsome
      have hn : ¬ v.size < v.limit := by simpa using hsome
      rw [e]
      exact ⟨by simp [answerOf, hl, hn], by simp [listOf, hl, hn, hview], fun _ _ => rfl, hg⟩
    | some r =>
      obtain ⟨s', v'⟩ := r
      have e : step k hk d Wd stride g t (.push x) = (commit g t s' v', .done) := by simp [step, hv, hp]
      rw [hp] at hsome
      have hn : v.size < v.limit := by simpa using hsome
      obtain ⟨he, hsd, _, hlim, hfr⟩ := push_elems k hk g.store v x d hs hg.fresh s' v' hp
      obtain ⟨hkeep, hcell⟩ := push_keeps k hk g.store v x d hs hg.fresh s' v' hp
      obtain ⟨hi, ho⟩ := commit_inv g hg t v hv s' v' hfr hsd hkeep hcell
      rw [e]
      refine ⟨by simp [answerOf, hl, hn], ?_, ho, hi⟩
      rw [view_commit_here, he, hlim]
      simp [listOf, hl, hn]
  | pop =>
    by_cases hn : 0 < v.size
    · have e : step k hk d Wd stride g t .pop = (commit g t g.store (pop v), .done) := by simp [step, hv, hn]
      rw [e]
      have hps : Sound g.store (pop v) := by
        obtain ⟨p, hp, hlen, hsize, hlim⟩ := hs
        exact ⟨p, hp, hlen, by simp [pop]; omega, hlim⟩
      obtain ⟨hi, ho⟩ := commit_inv g hg t v hv g.store (pop v) hg.fresh hps (fun _ _ _ => rfl) (Or.inl rfl)
      refine ⟨by simp [answerOf, hl, hn], ?_, ho, hi⟩
      rw [view_commit_here, pop_elems _ _ hs]
      rfl
    · have e : step k hk d Wd stride g t .pop = (g, .refused) := by simp [step, hv, hn]
      have hnil : elems g.store v = [] := List.eq_nil_of_length_eq_zero (by omega)
      rw [e]
      exact ⟨by simp [answerOf, hl, hn], by simp [listOf, hnil] at hview ⊢; exact hview, fun _ _ => rfl, hg⟩
  | size =>
    have e : step k hk d Wd stride g t .size = (g, .count v.size) := by simp [step, hv]
    rw [e]
    exact ⟨by simp [answerOf, hl], hview, fun _ _ => rfl, hg⟩

/-- A call keeps the guard sound. -/
theorem step_inv (k : Nat) (hk : 0 < k) (d : α) (Wd stride : Nat) (g : Guarded α) (hg : Inv g) (t : Nat)
    (c : Call α) : Inv (step k hk d Wd stride g t c).1 := by
  cases hv : g.records t with
  | none =>
    have e : step k hk d Wd stride g t c = (g, .refused) := by simp [step, hv]
    rw [e]
    exact hg
  | some v => exact (step_spec k hk d Wd stride g hg t v hv c).2.2.2

/-- The caller sees none of the internal state. Two sound guards with the same view through each token give the
same answer to a call, and they have the same view after it. The cells, their identities and their places can all
differ between the two. -/
theorem decoupled (k : Nat) (hk : 0 < k) (d : α) (Wd stride : Nat) (g h : Guarded α) (hg : Inv g) (hh : Inv h)
    (hs : Same g h) (t : Nat) (c : Call α) :
    (step k hk d Wd stride g t c).2 = (step k hk d Wd stride h t c).2 ∧
      Same (step k hk d Wd stride g t c).1 (step k hk d Wd stride h t c).1 := by
  cases hv : g.records t with
  | none =>
    have h0 : view h t = none := by rw [← hs t]; simp [view, hv]
    have hn : h.records t = none := by simpa [view] using h0
    have e1 : step k hk d Wd stride g t c = (g, .refused) := by simp [step, hv]
    have e2 : step k hk d Wd stride h t c = (h, .refused) := by simp [step, hn]
    rw [e1, e2]
    exact ⟨rfl, hs⟩
  | some v =>
    have h1 : view h t = some (elems g.store v, v.limit) := by rw [← hs t]; simp [view, hv]
    obtain ⟨w, hw, he, hlim⟩ : ∃ w, h.records t = some w ∧ elems h.store w = elems g.store v ∧ w.limit = v.limit := by
      cases hw : h.records t with
      | none => simp [view, hw] at h1
      | some w =>
        simp [view, hw] at h1
        exact ⟨w, rfl, h1.1, h1.2⟩
    obtain ⟨a1, v1, o1, _⟩ := step_spec k hk d Wd stride g hg t v hv c
    obtain ⟨a2, v2, o2, _⟩ := step_spec k hk d Wd stride h hh t w hw c
    refine ⟨by rw [a1, a2, he, hlim], ?_⟩
    intro u
    by_cases hu : u = t
    · subst hu
      rw [v1, v2, he, hlim]
    · rw [o1 u hu, o2 u hu, hs u]

/-- The answers of a run of calls, each a token and its call. -/
def run (k : Nat) (hk : 0 < k) (d : α) (Wd stride : Nat) (g : Guarded α) : List (Nat × Call α) → List (Answer α)
  | [] => []
  | (t, c) :: rest => (step k hk d Wd stride g t c).2 :: run k hk d Wd stride (step k hk d Wd stride g t c).1 rest

/-- Two sound guards with the same view give the same answers to each run of calls. -/
theorem run_decoupled (k : Nat) (hk : 0 < k) (d : α) (Wd stride : Nat) (g h : Guarded α) (hg : Inv g) (hh : Inv h)
    (hs : Same g h) (calls : List (Nat × Call α)) :
    run k hk d Wd stride g calls = run k hk d Wd stride h calls := by
  induction calls generalizing g h with
  | nil => rfl
  | cons tc rest ih =>
    obtain ⟨t, c⟩ := tc
    obtain ⟨ha, hs'⟩ := decoupled k hk d Wd stride g h hg hh hs t c
    simp only [run]
    rw [ha, ih _ _ (step_inv k hk d Wd stride g hg t c) (step_inv k hk d Wd stride h hh t c) hs']

/-- The mover behind the guard: each identity at a new offset. -/
def moveAll (g : Guarded α) (to : Nat → Nat) : Guarded α := ⟨moved g.store to, g.records⟩

omit [Inhabited α] in
/-- The mover changes no view, and the guard stays sound. -/
theorem moveAll_same (g : Guarded α) (to : Nat → Nat) (hg : Inv g) : Same g (moveAll g to) ∧ Inv (moveAll g to) :=
  ⟨fun _ => rfl, ⟨hg.fresh, hg.sound, hg.apart⟩⟩

/-- A relocation behind the guard. It copies the elements of the vector of a token to a fresh cell of the same
capacity, and gives the old cell back. The record then names the fresh cell. -/
def relocate (k : Nat) (hk : 0 < k) (d : α) (g : Guarded α) (t : Nat) : Guarded α :=
  match g.records t with
  | some v => commit g t (regrow k hk g.store v d v.cap).1 (regrow k hk g.store v d v.cap).2
  | none => g

/-- A relocation changes no view, and the guard stays sound. So the guard can put each cell where it chooses. -/
theorem relocate_same (k : Nat) (hk : 0 < k) (d : α) (g : Guarded α) (hg : Inv g) (t : Nat) :
    Same g (relocate k hk d g t) ∧ Inv (relocate k hk d g t) := by
  cases hv : g.records t with
  | none =>
    have e : relocate k hk d g t = g := by simp [relocate, hv]
    rw [e]
    exact ⟨fun _ => rfl, hg⟩
  | some v =>
    have e : relocate k hk d g t = commit g t (regrow k hk g.store v d v.cap).1 (regrow k hk g.store v d v.cap).2 := by
      simp [relocate, hv]
    rw [e]
    have hs := hg.sound t v hv
    obtain ⟨_, _, _, hsize, hlim⟩ := hg.sound t v hv
    obtain ⟨he, hsd, hfr, hk4⟩ := regrow_elems k hk g.store v d v.cap hs hg.fresh hsize hlim
    obtain ⟨hi, ho⟩ := commit_inv g hg t v hv _ _ hfr hsd hk4 (Or.inr rfl)
    refine ⟨fun u => ?_, hi⟩
    by_cases hu : u = t
    · subst hu
      rw [view_commit_here, he]
      simp [view, hv, regrow]
    · exact (ho u hu).symm

/-- The guard at the start: no cell and no record. -/
def emptyGuard : Guarded α := ⟨⟨fun _ => none, fun _ => 0, 0⟩, fun _ => none⟩

omit [Inhabited α] in
theorem empty_inv : Inv (emptyGuard : Guarded α) :=
  ⟨fun _ _ => rfl, fun _ _ h => by simp [emptyGuard] at h, fun _ _ _ _ _ h => by simp [emptyGuard] at h⟩

/-- A create: a cell of `cap` elements from the movable option, and a record for the token with no element. It refuses
a token that names a record, and a capacity past the ceiling. -/
def create (g : Guarded α) (t cap limit : Nat) (d : α) : Guarded α :=
  if g.records t = none ∧ cap ≤ limit then
    ⟨(alloc g.store cap d).1, update g.records t (some ⟨(alloc g.store cap d).2, 0, cap, limit⟩)⟩
  else g

omit [Inhabited α] in
/-- A create keeps the guard sound, and the token then views a vector with no element. -/
theorem create_inv (g : Guarded α) (hg : Inv g) (t cap limit : Nat) (d : α) :
    Inv (create g t cap limit d) ∧
      (g.records t = none → cap ≤ limit → view (create g t cap limit d) t = some ([], limit)) := by
  unfold create
  by_cases hc : g.records t = none ∧ cap ≤ limit
  · simp only [hc, and_self, ↓reduceIte]
    obtain ⟨hn, hcl⟩ := hc
    have hold : ∀ u w, g.records u = some w → w.cell < g.store.next := fun u w hw =>
      cell_lt _ _ (hg.sound u w hw) hg.fresh
    have hkeep : ∀ u w, g.records u = some w → (alloc g.store cap d).1.payload w.cell = g.store.payload w.cell :=
      fun u w hw => by
        have := hold u w hw
        simp [alloc, update, Nat.ne_of_lt this]
    refine ⟨⟨?_, ?_, ?_⟩, fun _ _ => ?_⟩
    · intro j hj
      simp only [alloc] at hj
      simp only [alloc, update, show j ≠ g.store.next by omega, ↓reduceIte]
      exact hg.fresh j (by omega)
    · intro u w hw
      by_cases hu : u = t
      · subst hu
        simp [update] at hw
        subst hw
        exact ⟨⟨cap, fun _ => d⟩, by simp [alloc, update], rfl, Nat.zero_le _, hcl⟩
      · simp [update, hu] at hw
        exact (sound_congr _ _ _ (hkeep u w hw)).2 (hg.sound u w hw)
    · intro u1 u2 w1 w2 hne h1 h2
      by_cases hu1 : u1 = t
      · subst hu1
        have hu2 : u2 ≠ u1 := Ne.symm hne
        simp [update] at h1
        simp [update, hu2] at h2
        subst h1
        have := hold u2 w2 h2
        simp [alloc]
        omega
      · by_cases hu2 : u2 = t
        · subst hu2
          simp [update] at h2
          simp [update, hu1] at h1
          subst h2
          have := hold u1 w1 h1
          simp [alloc]
          omega
        · simp [update, hu1] at h1
          simp [update, hu2] at h2
          exact hg.apart u1 u2 w1 w2 hne h1 h2
    · simp [view, update, elems, alloc, slice]
  · simp only [hc, ↓reduceIte]
    exact ⟨hg, fun hn hcl => absurd ⟨hn, hcl⟩ hc⟩

/-- A destroy: the cell of the token given back, and the record gone. -/
def destroy (g : Guarded α) (t : Nat) : Guarded α :=
  match g.records t with
  | some v => ⟨release g.store v.cell, update g.records t none⟩
  | none => g

omit [Inhabited α] in
/-- A destroy keeps the guard sound, and changes the view through no other token. -/
theorem destroy_inv (g : Guarded α) (hg : Inv g) (t : Nat) :
    Inv (destroy g t) ∧ ∀ u, u ≠ t → view (destroy g t) u = view g u := by
  unfold destroy
  cases hv : g.records t with
  | none => exact ⟨hg, fun _ _ => rfl⟩
  | some v =>
    have hkeep : ∀ u w, u ≠ t → g.records u = some w → (release g.store v.cell).payload w.cell = g.store.payload w.cell :=
      fun u w hu hw => by simp [release, update, hg.apart u t w v hu hw hv]
    refine ⟨⟨?_, ?_, ?_⟩, ?_⟩
    · intro j hj
      simp only [release, update]
      split
      · rfl
      · exact hg.fresh j hj
    · intro u w hw
      by_cases hu : u = t
      · subst hu; simp [update] at hw
      · simp [update, hu] at hw
        exact (sound_congr _ _ _ (hkeep u w hu hw)).2 (hg.sound u w hw)
    · intro u1 u2 w1 w2 hne h1 h2
      by_cases hu1 : u1 = t
      · subst hu1; simp [update] at h1
      · by_cases hu2 : u2 = t
        · subst hu2; simp [update] at h2
        · simp [update, hu1] at h1
          simp [update, hu2] at h2
          exact hg.apart u1 u2 w1 w2 hne h1 h2
    · intro u hu
      simp only [view, update, hu, ↓reduceIte]
      cases hw : g.records u with
      | none => rfl
      | some w => simp [elems_congr _ _ _ (hkeep u w hu hw)]

/-- The record of the cell at address `base` of a tier: the payload after its header, and the neighbor at the end of
its footprint. -/
def recordOf (base : Nat) (c : Cell) (stride size cap : Nat) : Guard.Record :=
  ⟨base + H, stride, size, cap, base + foot H c⟩

theorem pointers_ge (base : Nat) (l : List Cell) : ∀ d ∈ pointers H base l, base ≤ d.1 := by
  induction l generalizing base with
  | nil => intro d hd; simp [pointers] at hd
  | cons c rest ih =>
    intro d hd
    simp only [pointers, List.mem_append] at hd
    rcases hd with hd | hd
    · split at hd
      · simp at hd; subst hd; exact Nat.le_refl _
      · simp at hd
    · have := ih _ d hd
      unfold foot at this
      omega

/-- A record that the layout gives meets `Guard.Placed` where its capacity fits the cell. The cell ends at its
neighbor, and each other cell of the tier starts at that neighbor or past it. -/
theorem placed_in_layout (base : Nat) (c : Cell) (rest : List Cell) (stride size cap : Nat) (hsc : size ≤ cap)
    (hfit : cap * stride ≤ c.size) :
    Guard.Placed (recordOf H base c stride size cap) ∧
      ∀ d ∈ pointers H (base + foot H c) rest, (recordOf H base c stride size cap).neighbor ≤ d.1 := by
  refine ⟨⟨hsc, ?_⟩, fun d hd => pointers_ge H _ rest d hd⟩
  simp only [recordOf, foot]
  omega

/-! ## The persistent and temporary tiers behind the guard -/

/-- A tier behind the guard. It holds its cells in the order of their addresses, each with the identity that the record
of its token names. It also holds the payload of each identity. -/
structure Tier (α : Type) where
  cells : List (Cell × Nat)
  payload : Nat → Option (Payload α)

/-- The cells of a tier as the layout holds them. -/
def Tier.layout (t : Tier α) : List Cell := t.cells.map Prod.fst

/-- The identities that a busy cell of the tier holds. -/
def Tier.held (t : Tier α) : List Nat := (t.cells.filter fun c => c.1.busy).map Prod.snd

/-- What the caller can know through a token: the payload of the identity that its record names, while a busy cell
holds it. -/
def Tier.view (t : Tier α) (id : Nat) : Option (Payload α) := if id ∈ t.held then t.payload id else none

/-- The fill: the busy cells end to end in their order, with no free cell between two. The guard changes the base in
each record to the new place of its cell. -/
def Tier.pack (t : Tier α) : Tier α := { t with cells := t.cells.filter fun c => c.1.busy }

omit [Inhabited α] in
/-- The fill changes no view through a token. -/
theorem pack_view (t : Tier α) (id : Nat) : t.pack.view id = t.view id := by
  have hidem : (t.cells.filter fun c => c.1.busy).filter (fun c => c.1.busy) = t.cells.filter fun c => c.1.busy :=
    List.filter_eq_self.2 fun a ha => (List.mem_filter.1 ha).2
  simp only [Tier.view, Tier.pack, Tier.held, hidem]
  congr

omit [Inhabited α] in
/-- After the fill, the tier has no hole. -/
theorem pack_no_holes (t : Tier α) : holes H t.pack.layout = 0 := by
  apply allBusy_no_holes
  intro c hc
  simp only [Tier.layout, Tier.pack, List.mem_map, List.mem_filter] at hc
  obtain ⟨a, ⟨_, hb⟩, rfl⟩ := hc
  exact hb

omit [Inhabited α] in
/-- A caller that holds pointers pins each cell at its address. So the fill keeps the pointers of a tier only where the
tier has no hole. -/
theorem pack_moves_pointers (t : Tier α) (h : 0 < holes H t.layout) :
    pointers H 0 t.pack.layout ≠ pointers H 0 t.layout := by
  intro he
  have := fragmentation_stays H _ _ he
  rw [pack_no_holes] at this
  omega

omit [Inhabited α] in
/-- A temporary cell held past its call leaves a hole under it. Behind the guard, the fill closes the hole, and the
caller reads the same payload through its token. -/
theorem held_hole_closes (low high owner id : Nat) (p : Nat → Option (Payload α)) :
    holes H (Tier.layout ⟨[(⟨low, false, 0⟩, 0), (⟨high, true, owner⟩, id)], p⟩) = H + low ∧
      holes H (Tier.pack ⟨[(⟨low, false, 0⟩, 0), (⟨high, true, owner⟩, id)], p⟩).layout = 0 ∧
      (Tier.pack ⟨[(⟨low, false, 0⟩, 0), (⟨high, true, owner⟩, id)], p⟩).view id =
        Tier.view ⟨[(⟨low, false, 0⟩, 0), (⟨high, true, owner⟩, id)], p⟩ id :=
  ⟨held_cell_hole H low high owner, pack_no_holes H _, pack_view _ _⟩

end LocusCarcerum.Vector
