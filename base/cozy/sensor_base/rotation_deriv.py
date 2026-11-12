import numpy as np


def hat(vec):
    return np.array([[0., -vec[2], vec[1]],
                     [vec[2], 0., -vec[0]],
                     [-vec[1], vec[0], 0.]])


def get_angle_axis(vec):
    angle = np.linalg.norm(vec)
    axis = np.array(vec) / angle
    return angle, axis


def EXP_r(vec):
    angle, axis = get_angle_axis(vec)

    skew_sym_vec = hat(vec)
    vec = vec.reshape((3, 1))
    # Rodrigues`s formula
    # method 1:
    R = np.cos(angle) * np.eye(3) + (1 - np.cos(angle)) * vec * vec.T + np.sin(angle) * hat(skew_sym_vec)
    # method 2:
    # R = (1 - np.cos(angle)) * skew_vec @ skew_vec + np.eye(3) + np.sin(angle) * skew_vec
    return R


def LOG_R(R):
    theta = np.arccos((np.trace(R) - 1) / 2.)
    return


if __name__ == '__main__':

    print("hello, world!")
    exit(0)
