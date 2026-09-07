"""Exact-rational checks for finite normal binary32 observations (plus zero).

Not a complete SH4/SSE emulator: subnormal inputs/results, infinities, NaNs,
overflow and signed-zero subtleties are outside this bounded capture contract.
"""
from fractions import Fraction
import struct


def fraction(bits):
    if not 0 <= bits <= 0xffffffff:
        raise ValueError('binary32 word out of range')
    exponent, mantissa = (bits >> 23) & 255, bits & 0x7fffff
    if exponent == 255 or (exponent == 0 and mantissa != 0):
        raise ValueError('nonfinite/subnormal operand unsupported')
    if exponent == 0:
        return Fraction(0)
    significand = (1 << 23) | mantissa
    scale = exponent - 150
    value = Fraction(significand << scale) if scale >= 0 else Fraction(significand, 1 << -scale)
    return -value if bits >> 31 else value


def rounded_bits(value, mode):
    """Round an exact rational using MXCSR RC: nearest-even/down/up/toward-zero."""
    if mode not in range(4):
        raise ValueError('invalid rounding mode')
    if value == 0:
        return 0
    if not fraction(0x00800000) <= abs(value) <= fraction(0x7f7fffff):
        raise ValueError('subnormal/overflow result unsupported')
    # A binary64 approximation locates neighboring binary32 values only. The
    # final selection and all tie/direction decisions use exact rational values.
    center = struct.unpack('!I', struct.pack('!f', float(value)))[0]
    candidates = []
    for word in (center - 1, center, center + 1):
        try:
            represented = fraction(word)
        except ValueError:
            continue
        if mode == 1 and represented > value:
            continue
        if mode == 2 and represented < value:
            continue
        if mode == 3 and ((value > 0 and represented > value)
                          or (value < 0 and represented < value)):
            continue
        candidates.append((abs(value - represented), word & 1, word))
    if not candidates:
        raise ValueError('rounding neighborhood missing')
    return min(candidates)[2]


def evaluate(operation, a, b, c, mode, fused=False):
    a, b, c = fraction(a), fraction(b), fraction(c)
    if operation == 'div':
        if b == 0:
            raise ValueError('division by zero unsupported')
        value = a / b
    elif operation == 'mul':
        value = a * b
    elif operation == 'madd':
        product = b * c if fused else fraction(rounded_bits(b * c, mode))
        value = a + product
    else:
        raise ValueError('unknown operation')
    return rounded_bits(value, mode)
