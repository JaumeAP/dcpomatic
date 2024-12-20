import math

factors = (
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    30,
    48,
    50,
    60,
    48000,
    96000,
)

X = 1
for factor in factors:
    if (X % factor):
        X *= factor

print(f"Timebase: {X}")
print(f"Bits: {math.log2(X)}")

HOURS = 12
print(f"{HOURS} hour film requires: {math.log2(X * HOURS * 60 * 60)} bits")

