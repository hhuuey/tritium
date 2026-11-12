import numpy as np


def intrinsic_form_matrix(intrinsic):
    K = np.zeros([3, 3], dtype=np.float64)
    K[0, 0] = intrinsic[0]
    K[1, 1] = intrinsic[1]
    K[2, 0] = intrinsic[2]
    K[2, 1] = intrinsic[3]
    K[2, 2] = 1.0
    return K