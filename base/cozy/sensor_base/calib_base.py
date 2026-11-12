https://docs.opencv.org/4.11.0/d9/d0c/group__calib3d.html
initUndistortRectifyMap:
https://docs.opencv.org/4.11.0/d9/d0c/group__calib3d.html#ga7dfb72c9cf9780a347fbe3d1c47e5d5a

1. OpenCV's pinhole camera model.
pinhole full distortion:(k1, k2, p1, p2 [, k3 [, k4, k5, k6 [, s1, s2, s3, s4 [,τx, τy]]]])
- plumb_bob: (k1, k2, p1, p2, k3)
- rational_polynomial: (k1, k2, p1, p2, k3, k4, k5, k6)

2. OpenvCV fisheye model
kannala_brandt: (k1, k2, k3, k4)