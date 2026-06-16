import struct
import os
import subprocess
import sys
import math

FUZZER = os.path.join(os.path.dirname(__file__), '..', 'build/tests/tests/hexwave_fuzzer_standalone')

CRASH_DIR = os.path.join(os.path.dirname(__file__), 'hexwave_crashes')
os.makedirs(CRASH_DIR, exist_ok=True)

def make_input(t, prev_dt, reflect, pt, hh, zw, reflect_p, pt_p, hh_p, zw_p, freq, payload=b''):
    """Build a 44+ byte fuzz input buffer."""
    parts = [
        struct.pack('<f', t),
        struct.pack('<f', prev_dt),
        struct.pack('<i', reflect),
        struct.pack('<f', pt),
        struct.pack('<f', hh),
        struct.pack('<f', zw),
        struct.pack('<i', reflect_p),
        struct.pack('<f', pt_p),
        struct.pack('<f', hh_p),
        struct.pack('<f', zw_p),
        struct.pack('<f', freq),
    ]
    buf = b''.join(parts)
    buf += payload
    assert len(buf) >= 44
    return buf

def run_test(name, buf):
    path = os.path.join(CRASH_DIR, name)
    with open(path, 'wb') as f:
        f.write(buf)
    try:
        env = os.environ.copy()
        env['ASAN_OPTIONS'] = 'detect_leaks=1:abort_on_error=1'
        env['UBSAN_OPTIONS'] = 'halt_on_error=1:abort_on_error=1'
        result = subprocess.run([FUZZER, path], capture_output=True, timeout=5, env=env)
        print(f"  {name}: exit={result.returncode}, stderr={result.stderr[:200]}")
        return result.returncode
    except subprocess.TimeoutExpired:
        print(f"  {name}: TIMEOUT (>5s)")
        return -1
    except FileNotFoundError:
        print(f"  {name}: fuzzer not found at {FUZZER}")
        return -2

# Normal test (should pass)
normal = make_input(0.0, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, 0.1)
print("=== Normal ===")
run_test("normal", normal)

# NaN freq
nan_freq = make_input(0.0, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, float('nan'))
print("=== NaN freq ===")
run_test("nan_freq", nan_freq)

# Inf freq  
inf_freq = make_input(0.0, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, float('inf'))
print("=== Inf freq ===")
run_test("inf_freq", inf_freq)

# -Inf freq
neginf_freq = make_input(0.0, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, float('-inf'))
print("=== -Inf freq ===")
run_test("neginf_freq", neginf_freq)

# freq=0 (dt=0)
zero_freq = make_input(0.0, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, 0.0)
print("=== Zero freq ===")
run_test("zero_freq", zero_freq)

# Very large t
large_t = make_input(1e10, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, 0.1)
print("=== Large t (1e10) ===")
run_test("large_t", large_t)

# Very negative t
neg_t = make_input(-1e10, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, 0.1)
print("=== Neg t (-1e10) ===")
run_test("neg_t", neg_t)

# NaN t
nan_t = make_input(float('nan'), 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, 0.1)
print("=== NaN t ===")
run_test("nan_t", nan_t)

# Max float freq
max_freq = make_input(0.0, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, 3.4028234e+38)
print("=== Max float freq ===")
run_test("max_freq", max_freq)

# Denormal freq
denorm_freq = make_input(0.0, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, 1.4e-45)
print("=== Denormal freq ===")
run_test("denorm_freq", denorm_freq)

# Negative frequency
neg_freq = make_input(0.0, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, -0.5)
print("=== Negative freq ===")
run_test("neg_freq", neg_freq)

# t very close to segment boundary
edge_t = make_input(0.9999, 0.0, 1, 0.0, 0.0, 0.0, 1, 0.0, 0.0, 0.0, 0.01)
print("=== Edge t (~1.0) ===")
run_test("edge_t", edge_t)

# Extreme half_height
extreme_hh = make_input(0.0, 0.0, 1, 0.0, 1e38, 0.0, 1, 0.0, 1e-38, 0.0, 0.1)
print("=== Extreme half_height ===")
run_test("extreme_hh", extreme_hh)

print("\nResults written to", CRASH_DIR)
