import sys
import matplotlib.pyplot as plt
import matplotlib.image as mpimg
import numpy as np
from scipy.interpolate import griddata
from scipy.ndimage import gaussian_filter

sn = 7
name = '%06d' % sn  # 6 digit zeropadding
img = f'/home/hhh/code/kitti-velo2cam/data_object_image_2/testing/image_2/{name}.png'
binary = f'/home/hhh/code/kitti-velo2cam//data_object_velodyne/testing/velodyne/{name}.bin'
with open(f'/home/hhh/code/kitti-velo2cam/testing/calib/{name}.txt', 'r') as f:
    calib = f.readlines()

# P2 (3 x 4) for left eye
P2 = np.array([float(x) for x in calib[2].strip('\n').split(' ')[1:]]).reshape(3, 4)
R0_rect = np.array([float(x) for x in calib[4].strip('\n').split(' ')[1:]]).reshape(3, 3)
# Add a 1 in bottom-right, reshape to 4 x 4
R0_rect = np.insert(R0_rect, 3, values=[0, 0, 0], axis=0)
R0_rect = np.insert(R0_rect, 3, values=[0, 0, 0, 1], axis=1)
Tr_velo_to_cam = np.array([float(x) for x in calib[5].strip('\n').split(' ')[1:]]).reshape(3, 4)
Tr_velo_to_cam = np.insert(Tr_velo_to_cam, 3, values=[0, 0, 0, 1], axis=0)

# read raw data from binary
scan = np.fromfile(binary, dtype=np.float32).reshape((-1, 4))
points = scan[:, :3]  # lidar xyz (front, left, up)
# TODO: use fov filter?
velo = np.insert(points, 3, 1, axis=1).T
velo = np.delete(velo, np.where(velo[0, :] < 0), axis=1)
cam = P2 @ R0_rect @ Tr_velo_to_cam @ velo
cam = np.delete(cam, np.where(cam[2, :] < 0), axis=1)
# get u,v,z
cam[:2] /= cam[2, :]

# do projection staff
plt.figure(figsize=(12, 5), dpi=96, tight_layout=True)
png = mpimg.imread(img)
IMG_H, IMG_W, _ = png.shape
# restrict canvas in range
plt.axis([0, IMG_W, IMG_H, 0])
plt.imshow(png)
# filter point out of canvas
u, v, z = cam
u_out = np.logical_or(u < 0, u > IMG_W)
v_out = np.logical_or(v < 0, v > IMG_H)
outlier = np.logical_or(u_out, v_out)
cam = np.delete(cam, np.where(outlier), axis=1)

# Generate dense depth map using interpolation
u, v, z = cam
dense_u = np.arange(IMG_W)
dense_v = np.arange(IMG_H)
dense_U, dense_V = np.meshgrid(dense_u, dense_v)
dense_Z = griddata((u, v), z, (dense_U, dense_V), method='linear')

# Smooth dense depth map
smooth_dense_Z = gaussian_filter(dense_Z, sigma=3)

# Visualize dense depth map
plt.imshow(smooth_dense_Z, cmap='jet', vmin=np.min(z), vmax=np.max(z))
plt.colorbar()
plt.title(name)
plt.savefig(f'./{name}_depth_smooth.png', bbox_inches='tight')
plt.show()

