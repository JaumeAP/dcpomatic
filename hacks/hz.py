import math

MAX_FPS = 30

X = MAX_FPS
for i in range(MAX_FPS - 1, 1, -1):
    if (X % i) != 0:
        X *= i

for i in (36, 48, 60):
    if (X % i) != 0:
        X *= i

print(f"Timebase: {X}")
print(f"Bits: {math.log2(X)}")

# 5 hours
HOURS = 5
print(f"{HOURS} hour film requires: {math.log2(X * HOURS * 60 * 60)} bits")

for i in range(2, MAX_FPS + 1):
    assert (X % i) == 0

