# EPnP 论文相关问题整理：Clamped DLT、LHM、SVD 求旋转矩阵原理

---

# 一、Clamped DLT 是什么？

EPnP 论文中提到：

> Clamped DLT, the DLT algorithm after clamping the internal parameters with their known values.

这里的 **Clamped DLT** 并不是一种新的算法，而是：

> **DLT + 已知内参 + 强制恢复合法旋转矩阵。**

---

## 1. DLT 求什么？

DLT（Direct Linear Transform）根据

\[
x \sim PX
\]

直接求整个投影矩阵

\[
P = K[R|t]
\]

其中

- K：相机内参
- R：旋转
- t：平移

DLT 会把整个

\[
P \in \mathbb{R}^{3\times4}
\]

全部作为未知量进行求解。

---

## 2. 为什么 DLT 不适合 PnP？

PnP 问题中：

- K 已知
- 只需要估计 R、t

DLT 却仍然估计整个 P，因此：

- 未利用已知内参
- 参数更多
- 数值稳定性较差
- 精度通常较低

---

## 3. Clamped 的含义

DLT 得到

\[
P
\]

之后，

利用已知内参

\[
K^{-1}P=[A|b]
\]

其中

A 理论上应该就是旋转矩阵。

但由于噪声，

A 一般不是合法旋转矩阵：

- 列不正交
- 行列式不等于1

因此需要

> **Project onto SO(3)**

即：

找到距离 A 最近的旋转矩阵。

步骤：

1.

\[
A=U\Sigma V^T
\]

2.

\[
R=UV^T
\]

若

\[
\det(R)<0
\]

则

\[
R=U
\begin{bmatrix}
1&&\\
&1&\\
&&-1
\end{bmatrix}
V^T
\]

最后恢复

t。

因此

Clamped DLT

就是

```text
DLT

↓

利用已知K

↓

SVD恢复合法旋转矩阵

↓

得到R,t
```

---

# 二、为什么 SVD 后 R = UV^T？

这是经典的

> Orthogonal Procrustes Problem

---

## 1. 问题

DLT 得到

\[
A
\]

它接近旋转矩阵，

但不是旋转矩阵。

目标：

找到最近的合法旋转矩阵

\[
\min_R
\|A-R\|_F^2
\]

约束

\[
R^TR=I
\]

---

## 2. Frobenius 范数

定义：

\[
\|M\|_F^2
=
\sum_{ij}M_{ij}^2
\]

有重要恒等式：

\[
\boxed{
\|M\|_F^2
=
tr(M^TM)
}
\]

因此

\[
\|A-R\|_F^2
=
tr((A-R)^T(A-R))
\]

---

## 3. 展开目标函数

展开：

\[
(A-R)^T(A-R)
=
A^TA
-
A^TR
-
R^TA
+
R^TR
\]

利用

\[
tr(A^TR)=tr(R^TA)
\]

得到

\[
\boxed{
\|A-R\|_F^2
=
tr(A^TA)
-
2tr(R^TA)
+
tr(R^TR)
}
\]

---

## 4. 为什么等价于最大化 trace？

由于：

A 已固定，

所以

\[
tr(A^TA)
\]

是常数。

又因为

\[
R^TR=I
\]

所以

\[
tr(R^TR)=3
\]

也是常数。

因此

\[
\|A-R\|_F^2
=
C
-
2tr(R^TA)
\]

唯一可变的是

\[
tr(R^TA)
\]

于是

\[
\boxed{
\min
\|A-R\|_F^2
\Longleftrightarrow
\max
tr(R^TA)
}
\]

---

## 5. 为什么继续 SVD？

设

\[
A=U\Sigma V^T
\]

代入：

\[
tr(R^TA)
=
tr(R^TU\Sigma V^T)
\]

利用 trace 循环不变性：

\[
tr(ABC)=tr(BCA)
\]

得到

\[
=
tr(V^TR^TU\Sigma)
\]

定义

\[
Q=V^TR^TU
\]

则

Q 仍然是正交矩阵。

于是问题变成

\[
\max
tr(Q\Sigma)
\]

由于

\[
\Sigma
=
diag(\sigma_1,\sigma_2,\sigma_3)
\]

所以

\[
tr(Q\Sigma)
=
\sum_i
q_{ii}\sigma_i
\]

其中

\[
|q_{ii}|\le1
\]

因此最大值发生在

\[
Q=I
\]

于是

\[
V^TR^TU=I
\]

最终得到

\[
\boxed{
R=UV^T
}
\]

---

## 6. 几何解释

SVD 可以理解成

\[
A
=
U
\Sigma
V^T
\]

即

```text
旋转

↓

拉伸

↓

旋转
```

其中：

- U：旋转
- Σ：拉伸
- V：旋转

真正的旋转矩阵不能包含拉伸。

因此：

直接把

\[
\Sigma
\]

替换成

\[
I
\]

得到

\[
R
=
UIV^T
=
UV^T
\]

即：

> **保留旋转，去掉缩放。**

---

# 三、为什么要判断 det(R)？

有时

\[
det(UV^T)=-1
\]

得到的是

```text
镜像

+

旋转
```

不是合法旋转。

因此修正：

\[
R=
U
diag(1,1,-1)
V^T
\]

保证

\[
det(R)=1
\]

---

# 四、LHM 是什么？

LHM

即

Lu、Hager、Mjolsness

发表于2000年的经典PnP算法。

它属于：

> **迭代优化算法**

不断优化：

重投影误差。

思想与今天的：

- Gauss-Newton
- Levenberg-Marquardt

类似。

---

## LHM 最大的问题

需要一个好的初始位姿：

\[
(R_0,t_0)
\]

否则：

- 收敛慢
- 收敛失败
- 落入局部最优

---

# 五、Weak Perspective（弱透视）是什么？

真实透视：

\[
u=f\frac{X}{Z}
\]

\[
v=f\frac{Y}{Z}
\]

每个点都有自己的

\[
Z_i
\]

因此：

每个点缩放不同。

---

## 弱透视假设

若：

物体厚度远小于距离。

例如：

```text
距离：

5m

厚度：

10cm
```

则

\[
Z_i
=
Z_0+\Delta Z_i
\]

其中

\[
\Delta Z_i
\ll
Z_0
\]

于是

\[
\frac1{Z_i}
=
\frac1{Z_0+\Delta Z_i}
\]

泰勒展开：

\[
\approx
\frac1{Z_0}
\]

因此

投影变成

\[
u
=
\frac{f}{Z_0}X
\]

\[
v
=
\frac{f}{Z_0}Y
\]

所有点拥有统一缩放。

---

## 为什么好？

原来的

Perspective

是：

```text
旋转

↓

每个点不同缩放

↓

平移
```

现在变成：

```text
旋转

↓

统一缩放

↓

平移
```

问题变成线性的。

因此：

可以快速估计：

- R
- t

作为初始化。

---

# 六、EPnP + LHM 是什么？

LHM 原论文：

初始化采用

Weak Perspective。

流程：

```text
Weak Perspective

↓

得到初值

↓

LHM优化
```

EPnP 论文提出：

为什么不用 EPnP 的结果作为初始化？

于是：

```text
EPnP

↓

得到R,t

↓

LHM继续优化
```

即：

EPnP + LHM

这样：

- 初始化更准确
- 收敛更快
- 更稳定
- 精度更高

---

# 七、EPnP + GN 与 EPnP + LHM 的区别

EPnP 论文中比较了：

### ① EPnP

```text
EPnP

↓

输出最终结果
```

---

### ② EPnP + GN

```text
EPnP

↓

Gauss-Newton优化
```

这里 GN 是论文作者自己加入的优化步骤。

---

### ③ EPnP + LHM

```text
EPnP

↓

LHM优化
```

这里 LHM 是已有算法，

EPnP 只是提供更好的初始化。

---

# 八、整体逻辑总结

```text
DLT

↓

求得近似旋转矩阵A

↓

SVD

↓

A = UΣV^T

↓

去掉缩放Σ

↓

R = UV^T

↓

得到最近合法旋转矩阵

==========================

LHM

↓

需要初始位姿

↓

原论文：

Weak Perspective 初始化

↓

EPnP论文：

EPnP 初始化

↓

LHM继续优化

↓

得到更高精度结果
```

---

# 九、核心结论

### Clamped DLT

> DLT + 已知内参 + SVD恢复合法旋转矩阵。

---

### SVD 为什么得到 R = UV^T

本质是在求解：

\[
\min_R
\|A-R\|_F^2
\]

等价于

\[
\max
tr(R^TA)
\]

SVD 可以把任意矩阵分解为：

> **旋转 → 拉伸 → 旋转**

去掉拉伸

\[
\Sigma
\]

便得到最近旋转矩阵：

\[
\boxed{
R=UV^T
}
\]

---

### Weak Perspective

把

\[
\frac1{Z_i}
\]

近似为

\[
\frac1{Z_0}
\]

于是：

Perspective Projection

近似为

Scaled Orthographic Projection。

问题从非线性变成线性，

便于快速初始化。

---

### EPnP + LHM

不是新的算法。

而是：

> **EPnP 提供高质量初值，LHM 做最终迭代优化。**

相比 Weak Perspective 初始化：

- 更准确
- 更稳定
- 收敛更快
- 对大深度变化场景更鲁棒



# OpenCV EPnP 求解流程整理

## 1. EPnP问题定义

给定：

* 世界坐标系中的 3D 点：

[
P_i^w=(X_i,Y_i,Z_i)^T,\quad i=1,\dots,n
]

* 图像中的 2D 点：

[
p_i=(u_i,v_i)^T
]

* 相机内参：

[
K=
\begin{bmatrix}
f_x&0&c_x\
0&f_y&c_y\
0&0&1
\end{bmatrix}
]

目标：

求相机位姿：

[
P_i^c=RP_i^w+t
]

满足：

[
p_i\sim KP_i^c
]

即：

[
p_i\sim K(RP_i^w+t)
]

---

# 2. OpenCV EPnP整体流程

OpenCV调用：

```cpp
solvePnP(..., SOLVEPNP_EPNP)
```

进入 EPnP 求解流程：

```
3D-2D correspondences
        |
        |
选择4个control points
        |
        |
计算每个点alpha坐标
        |
        |
构建线性系统 M
        |
        |
求M的kernel
        |
        |
恢复camera control points
        |
        |
absolute orientation求R,t
        |
        |
Gauss-Newton refinement
        |
        |
最终pose
```

---

# 3. 选择4个Control Points

EPnP核心思想：

将任意3D点表示成4个控制点的线性组合：

[
P_i^w=
\sum_{j=1}^{4}\alpha_{ij}C_j^w
]

其中：

* (C_j^w)：世界坐标中的控制点
* (\alpha_{ij})：固定权重

---

## 3.1 计算点云质心

首先计算：

[
C_1^w=
\frac1n\sum_iP_i^w
]

作为第一个control point。

---

## 3.2 PCA/SVD获取主方向

中心化：

[
q_i=P_i^w-C_1^w
]

组成矩阵：

[
Q=
[q_1,q_2,...,q_n]
]

如果采用：

[
Q\in R^{3\times n}
]

进行：

[
Q=U\Sigma V^T
]

则：

[
U=[u_1,u_2,u_3]
]

三个列向量为3D空间主方向。

---

如果代码中采用：

[
Q\in R^{n\times3}
]

则：

[
Q=U\Sigma V^T
]

此时：

[
V
]

的列向量为主方向。

NumPy：

```python
U,S,Vt=np.linalg.svd(Q)
```

返回的是：

[
V^T
]

所以：

```python
principal_axis = Vt.T
```

---

## 3.3 生成其他control points

根据PCA方向：

[
C_2=C_1+\sqrt{\lambda_1}u_1
]

[
C_3=C_1+\sqrt{\lambda_2}u_2
]

[
C_4=C_1+\sqrt{\lambda_3}u_3
]

最终得到：

[
C_1^w,C_2^w,C_3^w,C_4^w
]

---

# 4. 计算每个点的alpha坐标

EPnP使用：

[
P_i^w=
\sum_{j=1}^{4}
\alpha_{ij}C_j^w
]

看似：

4个未知量：

[
\alpha_{i1},...,\alpha_{i4}
]

但是加入：

[
\sum_j\alpha_{ij}=1
]

得到：

[
4
]

个方程：

[
\begin{bmatrix}
C_{1x}&C_{2x}&C_{3x}&C_{4x}\
C_{1y}&C_{2y}&C_{3y}&C_{4y}\
C_{1z}&C_{2z}&C_{3z}&C_{4z}\
1&1&1&1
\end{bmatrix}
\begin{bmatrix}
\alpha_1\
\alpha_2\
\alpha_3\
\alpha_4
\end{bmatrix}
=============

\begin{bmatrix}
X_i\Y_i\Z_i\1
\end{bmatrix}
]

求得：

[
\alpha_i
]

---

# 5. 建立线性系统 M

相机坐标：

[
P_i^c=
\sum_j\alpha_{ij}C_j^c
]

未知量：

4个camera control points：

[
C_j^c
]

每个3维：

[
4\times3=12
]

因此：

未知向量：

[
x=
(C_1^c,C_2^c,C_3^c,C_4^c)
]

---

投影约束：

[
u_i=f_x\frac{X_i^c}{Z_i^c}+c_x
]

转换：

[
(u_i-c_x)Z_i^c=f_xX_i^c
]

同理：

[
(v_i-c_y)Z_i^c=f_yY_i^c
]

每个点产生两个约束：

因此：

[
M x=0
]

其中：

[
M\in R^{2n\times12}
]

---

# 6. 求M的Null Space

求：

[
Mx=0
]

即寻找：

[
M
]

的零空间。

通过：

[
M^TM
]

进行特征分解：

得到最小特征值对应向量：

[
v_1,v_2,v_3,v_4
]

因此：

[
x=
\beta_1v_1+
\beta_2v_2+
\beta_3v_3+
\beta_4v_4
]

---

# 7. 求beta系数

利用控制点之间的距离约束：

世界坐标：

[
||C_i^w-C_j^w||
]

应该等于：

相机坐标：

[
||C_i^c-C_j^c||
]

建立：

[
La=\rho
]

求：

[
\beta
]

OpenCV中对应：

```cpp
find_betas_approx_1()
find_betas_approx_2()
find_betas_approx_3()
```

---

# 8. 恢复camera control points

根据：

[
C_j^c=
\beta_1v_{1j}
+\beta_2v_{2j}
+\beta_3v_{3j}
+\beta_4v_{4j}
]

得到：

[
C_1^c,C_2^c,C_3^c,C_4^c
]

---

# 9. 求旋转和平移

现在已知：

世界control points：

[
C_j^w
]

相机control points：

[
C_j^c
]

满足：

[
C_j^c=RC_j^w+t
]

这是 absolute orientation 问题。

---

计算中心：

[
\bar C^w
]

[
\bar C^c
]

计算：

[
H=
\sum
(C_j^w-\bar C^w)
(C_j^c-\bar C^c)^T
]

SVD：

[
H=USV^T
]

旋转：

[
R=VU^T
]

平移：

[
t=
\bar C^c-R\bar C^w
]

---

# 10. 非线性优化 Refinement

初始：

[
R,t
]

进一步优化：

目标：

[
\min_{R,t}
\sum_i
||p_i-\pi(RP_i+t)||^2
]

OpenCV使用：

* Gauss-Newton
* Levenberg-Marquardt

降低reprojection error。

---

# 11. EPnP复杂度 O(n)

论文中的：

[
O(n)
]

其中：

[
n=
\text{3D-2D correspondence数量}
]

原因：

## 构造alpha

每个点独立：

[
O(n)
]

## 构造M

矩阵：

[
M\in R^{2n\times12}
]

每个点贡献两行：

[
O(n)
]

## 求解

虽然M有：

[
2n
]

行，但是列固定：

[
12
]

计算：

[
M^TM
]

复杂度：

[
O(n)
]

---

# 12. EPnP与Planar情况

如果所有点共面：

[
rank(Q)<3
]

PCA第三方向退化。

导致：

* control points退化
* 数值条件变差
* pose ambiguity

因此：

EPnP可以处理平面点，但不是最优。

对于纯平面目标：

例如：

* AprilTag
* ArUco
* 棋盘格

通常使用：

* IPPE
* Homography decomposition

---

# 总结

EPnP核心思想：

[
\boxed{
\text{所有3D点}
\rightarrow
\text{4个control points线性表示}
\rightarrow
\text{线性求camera control points}
\rightarrow
\text{absolute orientation恢复R,t}
}
]

它的优势：

* 非迭代初值
* O(n)
* 支持大量3D点

它的限制：

* 平面情况下存在退化
* 共面pose ambiguity需要特殊处理
