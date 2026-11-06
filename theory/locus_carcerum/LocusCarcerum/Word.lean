/-!
# LocusCarcerum, the word that carries both boundaries

The persistent end in the low half of one word and the temporary top in the high half, `M` the size of a half.
A bump up adds its need to the word; a bump down subtracts its need shifted into the high half, modulo the
word. Under the two caps the C states, a bump up never carries into the top's half, and a bump down's borrow
leaves the word without touching the end's half. The in-flight bound: every bump up reads the end at or below
the most bytes a cellblock may hold before its read-modify-write, and the bumps that move the end between that
read and the read-modify-write are at most the lanes in flight, so the end stays within its half.
-/

namespace LocusCarcerum.Word

/-- The word of an end and a top, `M` the size of a half. -/
def encode (M E T : Nat) : Nat := T * M + E

/-- The end: the low half. -/
def low (M w : Nat) : Nat := w % M

/-- The top: the high half. -/
def high (M w : Nat) : Nat := w / M

theorem low_encode (M E T : Nat) (hE : E < M) : low M (encode M E T) = E := by
  unfold low encode
  rw [Nat.mul_comm, Nat.mul_add_mod, Nat.mod_eq_of_lt hE]

theorem high_encode (M E T : Nat) (hE : E < M) : high M (encode M E T) = T := by
  unfold high encode
  have hM : 0 < M := Nat.lt_of_le_of_lt (Nat.zero_le E) hE
  rw [Nat.mul_comm, Nat.mul_add_div hM, Nat.div_eq_of_lt hE, Nat.add_zero]

/-- A bump up of need `n` with `E + n` within the half: the end moves by `n` and the top is untouched. -/
theorem bumpUp_no_carry (M E T n : Nat) (h : E + n < M) :
    low M (encode M E T + n) = E + n ∧ high M (encode M E T + n) = T := by
  have hw : encode M E T + n = encode M (E + n) T := by
    unfold encode
    omega
  rw [hw]
  exact ⟨low_encode M (E + n) T h, high_encode M (E + n) T h⟩

/-- A word reduced modulo the whole word `M * M`: the high half folds, the low half stands. -/
theorem encode_mod_square (M E T : Nat) (hE : E < M) :
    encode M E T % (M * M) = encode M E (T % M) := by
  have hM : 0 < M := Nat.lt_of_le_of_lt (Nat.zero_le E) hE
  have hT : T = M * (T / M) + T % M := (Nat.div_add_mod T M).symm
  have hsplit : encode M E T = (M * M) * (T / M) + encode M E (T % M) := by
    unfold encode
    calc T * M + E = (M * (T / M) + T % M) * M + E := by rw [← hT]
      _ = (M * M) * (T / M) + (T % M * M + E) := by
        rw [Nat.add_mul, Nat.mul_assoc, Nat.mul_comm (T / M) M, ← Nat.mul_assoc]
        omega
  have hsmall : encode M E (T % M) < M * M := by
    unfold encode
    have h1 : T % M < M := Nat.mod_lt T hM
    have h2 : T % M * M + E < (T % M + 1) * M := by
      rw [Nat.add_mul, Nat.one_mul]
      omega
    have h3 : (T % M + 1) * M ≤ M * M := Nat.mul_le_mul_right M (Nat.succ_le_of_lt h1)
    omega
  rw [hsplit, Nat.mul_add_mod, Nat.mod_eq_of_lt hsmall]

/-- A bump down of need `n ≤ M`: the word takes `n` shifted into the high half away, modulo the word; the
borrow leaves the word, the end stands, and the top is `T - n` within its half. -/
theorem bumpDown_borrow_leaves (M E T n : Nat) (hE : E < M) (hn : n ≤ M) :
    low M ((encode M E T + (M * M - n * M)) % (M * M)) = E ∧
    high M ((encode M E T + (M * M - n * M)) % (M * M)) = (T + M - n) % M := by
  have hw : encode M E T + (M * M - n * M) = encode M E (T + M - n) := by
    unfold encode
    have hnM : n * M ≤ M * M := Nat.mul_le_mul_right M hn
    have hsub : (T + M - n) * M = T * M + M * M - n * M := by
      rw [Nat.sub_mul, Nat.add_mul]
    omega
  rw [hw, encode_mod_square M E (T + M - n) hE]
  exact ⟨low_encode M E ((T + M - n) % M) hE, high_encode M E ((T + M - n) % M) hE⟩

/-- A bump down that is taken, `n ≤ T`: the top is `T - n` exactly. -/
theorem bumpDown_within (M T n : Nat) (hT : T < M) (hn : n ≤ T) : (T + M - n) % M = T - n := by
  have h : T + M - n = (T - n) + M := by omega
  rw [h, Nat.add_mod_right, Nat.mod_eq_of_lt (by omega)]

/-- The in-flight bound. A bump up reads the end at `pre ≤ most` before its read-modify-write; the bumps that
add to the end between that read and the read-modify-write are at most `lanes` in flight, each of at most
`most_need`; the bump's own need is at most `most_need`. With `most + (lanes + 1) * most_need < M` the end after
the read-modify-write is within the half. -/
theorem in_flight_within (M most most_need lanes pre between n : Nat) (hpre : pre ≤ most)
    (hbetween : between ≤ lanes * most_need) (hn : n ≤ most_need)
    (hcap : most + (lanes + 1) * most_need < M) : pre + between + n < M := by
  rw [Nat.succ_mul] at hcap
  omega

/-- The C's caps on a 64-bit word: a half of 32 bits, the most bytes `2 ^ 31`, the most need `16 + 2 ^ 24`, and
up to 126 bumps in flight beside the one deciding. -/
theorem caps_64 : 2 ^ 31 + (126 + 1) * (16 + 2 ^ 24) < 2 ^ 32 := by decide

/-- The C's caps on a 32-bit word: a half of 16 bits, the most bytes `2 ^ 15`, the most need `16 + 2 ^ 8`, and
up to 119 bumps in flight beside the one deciding. -/
theorem caps_32 : 2 ^ 15 + (119 + 1) * (16 + 2 ^ 8) < 2 ^ 16 := by decide

end LocusCarcerum.Word
