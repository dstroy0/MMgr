# Numbers, and how far they can be trusted {#qa_numeric}

`to_double` returns the correctly rounded double for every input, and `verba.g` and `verba.fixed`
render one exactly. The suites measure all three, and this page says how. A numeric routine that
nobody checks is a routine that is wrong in a way nobody sees yet.

## What correct means here

A double printed to seventeen significant digits names exactly one double. That is not a
convention. It is a property of binary64: seventeen digits are enough to tell any two neighbors
apart. So there is exactly one right answer for such a string, and any other answer is a defect.

That gives a test with no judgement in it. Take a bit pattern, read it as a double, print it to
seventeen digits, read it back, and compare the patterns. Same bits or not.

The unit is the ulp - one representable step at that magnitude - because relative error hides the
thing that matters. An error of 1e-300 is nothing next to 1.0 and everything next to a subnormal,
and a percentage cannot tell those apart.

Both directions have solutions with names, and this page does not restate their proofs. Correctly rounded
print is Guy L. Steele Jr. and Jon L. White, _How to print floating-point numbers accurately_, PLDI
'90, 112-126, doi:10.1145/93542.93559. Correctly rounded read is William D. Clinger, _How to read
floating point numbers accurately_, PLDI '90, 92-101, doi:10.1145/93542.93557.

## What a parser must not do

Two faults make a parser wrong, and only one of them looks like a round problem. Over 999,480 random
bit patterns, a parser with both is exactly right for 16.4% of them. It returns zero for 225 of them,
and its worst error is 282,109,089 ulp.

**A fraction must not compound its own divisor.** A loop that does `val += digit / scale`, with
`scale` built by repeated multiplication, rounds three times per digit. An error in `scale` divides
into every digit after it. Seventeen digits are about fifty steps that each round.

**An exponent must not saturate.** A power of ten built by multiplication, `for k < ex: m *= 10.0`,
overflows: ten to the three hundred and ninth is larger than any double. It becomes an infinity, and
a division by an infinity is zero. So every value with a decimal exponent past 308 comes back as
nothing at all. That is the 282 million ulp: not a round error, but the whole value.

## Two halves, and neither works alone

**Split the power of ten.** Ten to the k is five to the k times two to the k. A power of two is the
exponent field, and to apply one is an add to that field. That is exact, and nothing overflows on the
way, because there is no product. Five to the three hundred and ninth is about 1.4e216, an ordinary
number, where ten to the same power is not a number at all.

**Hold the fraction as an integer and scale once.** That alone is worse than nothing: it returns zero
for 25,930 values against 225. A larger mantissa needs a more negative exponent to compensate, which
reaches the overflow sooner. A change that looks sensible moves the failure rate by two orders of
magnitude in the wrong direction.

The split power without the integer fraction stops the silent zeros but leaves 590 values off by more
than 8 ulp. The two together are exact for 20.9% of values, with a worst case of 8 ulp: never
catastrophic, and still wrong for the majority. The rest needs the exactness that the double already
holds.

## Where the exactness comes from

A finite double is `mant * 2^e` with the mantissa at max 53 bits and the scale between -1074 and 971.
That is not an approximation of the value, it **is** the value. A conversion that puts it into a
double and does arithmetic on its first line throws that away.

The temptation is to conclude that an exact decimal conversion needs arbitrary precision. It does
not, and the difference deserves a precise statement. An exact decimal _expansion_ would: a
subnormal needs 5^1074, which is a 2,494 bit integer, 39 words at 64 bits. But nobody needs the
expansion. The requirement is enough bits to decide the round. That is 53 for the mantissa, one
below it, and one bit that says whether any bit under that is 1.

128 bits carry all three with 74 to spare, and the width never grows. A normalize after each step
retains the fraction in place and pushes the growth into an `int` exponent.

## What is in the module

The engine is `src/transformo`, and it is a module and not something the parser retains to itself.
To turn a mantissa and an exponent into the value they name is not a parse problem. The render side
has the same problem in the other direction and should not solve it twice. Its entries are
`mmgr_muto_*`.

`src/pow5/pow5.h` is 360 bytes: nine powers of five and nine reciprocals, as 128 bit fractions with
their own binary exponents. Any decimal exponent below 512 is the product of at max nine of them.
So the loop runs nine times, not four hundred. The reciprocals make a negative exponent a multiply
and not a division.

The entries truncate and do not round, deliberately. A truncated entry is never above the true
power. A product is never above the true product, and the bit that decides a tie is never wrongly
clear. Rounded entries would put error on both sides, and the bits present could no longer decide
the tie.

The multiply is four partial products of the halves, reassembled with the carries written out - the
same shape at any word width.

**The round reads the same three places every time.** The 53 bits that become the mantissa, the one
below them, and whether any bit under that is 1. A 0 there rounds down. A 1 with something below
rounds up. A 1 with nothing below is the tie, and the tie goes to even. Nothing in it looks at what
the value was.

`mmgr_muto_scale_to_u64` is the same round. It stops at an integer and does not go on to assemble a
double: the end a digit string needs. The caller tells it the parity of whatever sits above it,
because ties go to even. Even means even _in the number that gets written_. Past the point, that
is the last digit of the fraction. With no decimals, the integer part decides the tie.

## How it is tested

Six ways, because each one catches something the others do not.

**Against a reference with a right answer.** 999,480 random bit patterns go through print and parse,
and the test compares bit patterns. No tolerance, no judgement.

**Random bit patterns and not chosen values.** Values a person picks cluster around the ordinary. A
bit pattern read as a double reaches subnormals, the exponent boundaries and the awkward mantissas
that nobody would think to write down.

**Strobed bits.** The other half of the same idea: start from a plain value and flip a few bits.
That lands on the exponents on each side of the ordinary range and not uniformly across it.

**Every entry in the table.** The exponent selects entries by its set bits. So exponents with each
bit in turn reach each entry in turn, and one with several bits set makes the multiplies compound.

**The edges, named.** The largest finite double, the smallest normal and the smallest subnormal. The
step between the largest subnormal and the smallest normal, past the top, past the bottom, and more
digits than the mantissa holds.

**The insides, where the outside cannot reach.** `test_cellularum_internals` compiles the
translation unit in and does not link it, and so the file-local entries are callable. Three things
live there:

- A carry out of the middle column of the 128 by 128 multiply. It occurs about once in 2^63
  multiplies; forty thousand random and strobed values do not produce one and never would. The case
  solves for the operands. Nobody knows that a carry works if the code has it and no test runs it.
- An exact tie passed to the round directly, so no test has to hunt for a decimal that lands exactly
  halfway between two doubles.
- The normalize path for a fraction with an empty high word. The conversion never produces one,
  because it guards the mantissa first.

## What the render side gets from it

`verba.fixed` goes through the engine. Two faults in a scale of its own must not come back:

- A scale that comes off the mantissa only while the shift is under 64 leaves the whole mantissa
  unscaled past that.
- A correction then shifts a 64 bit word by more than 64. C does not define that shift, and x86
  turns it into no shift at all.

Together they put the answer out by 2^128: two times ten to the minus forty one prints as
`0.006958041`. With them, about one value in six below roughly 1e-41 is wrong. Against a correctly
rounded reference over 66,000 cases, the faults give **15.87% wrong** and the engine gives
**0.0000%**.

The tie rule is one rule: `fixed` and `g` both round an exact tie to even, and the test asserts it. A
`fixed` that truncates a tie toward zero while `g` rounds it renders one number two ways.

The engine inlines into `verba_scribo` and costs it 1264 bytes at -O2. See @ref qa_optimization.

## And the same for verba.g

A 58 bit intermediate word must not come back. That word holds a little over seventeen decimal digits, and
a renormalize on every `g_mul10` and `g_div10` shifts bits off the bottom. The digits a render
produces consist of exactly those bits. With it, a seventeen digit render fails to name its own value
back **87.1%** of the time, even through a correctly rounded reader, worst 6 ulp.

The scale is one call:

```c
mant = mmgr_muto_scale_to_u64(n, s, p, 0u);
```

`n` and `s` are the mantissa and binary exponent the extraction already has. `p` is how many powers of
ten the requested digit count is away from where the value sits. That is the same entry `verba.fixed`
takes, and the same machinery the parser takes in the other direction. What stays in `verba.g` is the
choice of `p` and the place of the point.

The test measures it the same way as the parse. It renders 500,000 random bit patterns at seventeen
digits, reads them back with a correctly rounded reader, and compares the bit patterns. The render
is wrong for **0.0000%**.

It costs 1088 bytes at -O2, and one shape rule. The engine is `always_inline`, and so the compiler
copies it to each call site. The correction loop calls it from one place deliberately, because a
second call site is a second copy of the whole thing. With a call before the loop and another inside
it, the module is 1200 bytes larger for no change in output.

`MMGR_G_MAX_SIG` clamps at 18. The result is a `uint64_t`, and at nineteen digits that type can no
longer hold one.

## What is still not exact

Nothing, on the three paths above. `to_float` is `(float)to_double`, and so it rounds twice: once to
a double and once down again. A double round can land on the wrong neighbor where a single round
would not. Against `strtof` over 500,000 patterns no test shows it, but it is a real property of the
shape and not something the numbers rule out.
