import numpy as np



def get_mtx(fx, fy, cx, cy):
    mtx = np.zeros((3, 3))
    mtx[0, 0] = fx
    mtx[0, 2] = cx
    mtx[1, 1] = fy
    mtx[1, 2] = cy
    mtx[2, 2] = 1
    return mtx
