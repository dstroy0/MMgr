# Numbers, and how far they can be trusted {#qa_numeric}

`to_double` returns the correctly rounded double for every input, and `verba.g` does not. Both of
those are measured rather than believed, and this page says how, because a numeric routine that
nobody checked is a routine that is wrong in a way nobody has noticed yet.

## What correct means here

A double printed to seventeen significant digits names exactly one double. That is not a
convention, it is a property of binary64: seventeen digits is enough to tell any two neighbours
apart, so there is exactly one right answer for such a string and any other answer is a defect.

That gives a test with no judgement in it. Take a bit pattern, read it as a double, print it to
seventeen digits, read it back, and compare the patterns. Same bits or not.

The unit is the ulp - one representable step at that magnitude - because relative error hides the
thing that matters. Being wrong by 1e-300 is nothing next to 1.0 and everything next to a
subnormal, and a percentage cannot tell those apart.

## What the parser was doing

Measured over 999,480 random bit patterns:

|                        |          before |  after |
| ---------------------- | --------------: | -----: |
| exactly right          |           16.4% | 100.0% |
| off by 1 ulp           |           27.2% |      0 |
| off by 2 ulp           |           21.0% |      0 |
| off by 3 to 4          |           26.7% |      0 |
| off by 5 to 8          |            8.7% |      0 |
| off by more than 1024  |      225 values |      0 |
| worst                  | 282,109,089 ulp |      0 |
| silently returned zero |      225 values |      0 |

Two faults, and only one of them looked like a rounding problem.

**The fraction compounded its own divisor.** Each digit after the point did
`val += digit / scale` with `scale` built by repeated multiplication - three roundings per digit,
and an error in `scale` that divided into every digit after it. Seventeen digits was about fifty
rounding steps.

**The exponent saturated.** The power of ten was built by multiplying: `for k < ex: m *= 10.0`.
Ten to the three hundred and ninth is larger than any double, so it became an infinity, and
dividing by an infinity is zero. Every value with a decimal exponent past 308 came back as nothing
at all. That is where the 282 million ulp came from - it is not a rounding error, it is the whole
value.

## The path

The route mattered as much as the destination, because two of the four things tried made it worse
and one of them looked obviously right.

**A. Leave it.** 16.4% exact, 225 silent zeros.

**B. Accumulate the fraction as an integer and scale once.** The obvious fix for the compounding
divisor. It made things worse: 25,930 silent zeros against 225. A larger mantissa needs a more
negative exponent to compensate, which reaches the overflow sooner. A sensible-looking change that
moves the failure rate by two orders of magnitude in the wrong direction.

**C. Split the power of ten.** Ten to the k is five to the k times two to the k, and a power of two
is the exponent field - applying one is an add to that field. Exact, and nothing to overflow on the
way, because there is no product. Five to the three hundred and ninth is about 1.4e216, an ordinary
number, where ten to the same power is not a number at all.

**D. C without B.** Kills the silent zeros but leaves 590 values off by more than 8 ulp.

Both halves were needed. Neither alone was enough, and one alone was harmful.

That got to 20.9% exact with a worst case of 8 ulp - correct in the sense of never catastrophic,
and still wrong most of the time. Getting the rest needed the exactness that was there all along.

## Where the exactness comes from

A finite double is `mant * 2^e` with the mantissa at most 53 bits and the scale between -1074 and 971. That is not an approximation of the value, it **is** the value. The old code threw it away on
the first line by putting it into a double and doing arithmetic.

The temptation is to conclude that an exact decimal conversion needs arbitrary precision. It does
not, and the difference is worth being precise about. An exact decimal _expansion_ would: a
subnormal needs 5^1074, which is a 2,494 bit integer, 39 words at 64 bits. But nobody needs the
expansion. What is needed is enough bits to decide the rounding, and that is 53 for the mantissa,
one below it, and one bit saying whether anything at all is set under that.

128 bits carries all three with 74 to spare, and it never grows, because normalising after each
step keeps the fraction in place and pushes the growth into an `int` exponent.

## What is in the module

`src/pow5/pow5.h` is 360 bytes: nine powers of five and nine reciprocals, as 128 bit fractions with
their own binary exponents. Any decimal exponent below 512 is the product of at most nine of them,
so a loop that ran four hundred times runs nine. The reciprocals are what keep a negative exponent a
multiply rather than a division.

They are truncated rather than rounded, deliberately. A truncated entry is never above the true
power, so a product is never above the true product, and the bit that decides a tie is never
wrongly clear. Rounded entries would put error on both sides and the tie would stop being decidable
from the bits present.

The multiply is four partial products of the halves, reassembled with the carries written out - the
same shape at any word width.

**The rounding reads the same three places every time.** The 53 that become the mantissa, the one
below them, and whether anything is set under that. Clear is down. Set with something below is up.
Set with nothing below is the tie, and the tie goes to even. Nothing in it looks at what the value
was.

## How it is tested

Six ways, because each one catches something the others do not.

**Against a reference with a right answer.** 999,480 random bit patterns through print and parse,
comparing bit patterns. No tolerance, no judgement.

**Random bit patterns, not chosen values.** Values a person picks cluster around the ordinary. A bit
pattern read as a double reaches subnormals, the exponent boundaries and the awkward mantissas that
nobody would think to write down.

**Strobed bits.** The other half of the same idea: start from a plain value and flip a few bits,
which lands on the exponents either side of the ordinary range rather than uniformly across it.

**Every entry in the table.** The exponent selects entries by its set bits, so exponents with each
bit in turn reach each entry in turn, and one with several bits set makes the multiplies compound.

**The edges, named.** The largest finite double, the smallest normal, the smallest subnormal, the
step between the largest subnormal and the smallest normal, past the top, past the bottom, more
digits than the mantissa holds.

**The insides, where the outside cannot reach.** `test_cellularum_internals` compiles the
translation unit in rather than linking it, so the file-local entries are callable. Three things
live there:

- A carry out of the middle column of the 128 by 128 multiply. It happens about once in 2^63
  multiplies; forty thousand random and strobed values did not produce one and never would. The
  operands were solved for. A carry that is written and never executed is a carry nobody knows
  works.
- An exact tie handed to the rounding directly, rather than hunting for a decimal that lands
  exactly halfway between two doubles.
- The normalise path for a fraction with an empty high word, which the conversion never produces
  because it guards the mantissa first.

## What is still not exact

`verba.g` is not correctly rounded. Rendering and parsing are different problems and only the parse
has been done properly.

It runs its conversion in a 58 bit working word - a little over seventeen decimal digits - and
`g_mul10` and `g_div10` renormalise on every step, which shifts bits off the bottom. Measured on
500,000 random doubles, its output fails to name the value back 87.1% of the time even when parsed
by a correctly rounded reader, worst 6 ulp.

That is a real limit and it is written down here rather than left to be discovered. It is fine for
a log line and wrong for anything that has to persist a value and get it back. `MMGR_G_MAX_SIG`
clamps the digit count at 18 because past that the working word has run out of digits entirely and
the answer stops being merely imprecise - swept over 130,944 doubles, one to eighteen digits all
read back, nineteen was wrong on 321 of them, worst at 2^64.

The same approach that fixed the parse would fix this one. It has not been done yet.
