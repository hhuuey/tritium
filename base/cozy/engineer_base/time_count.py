import time
import functools


def timeit(func):

    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        t0 = time.perf_counter()
        result = func(*args, **kwargs)
        t1 = time.perf_counter()
        print(f"============COZY_TIME========== |||||||| {func.__name__} elapsed: {(t1 - t0)*1000:.2f} ms ||||||||")
        return result

    return wrapper
