# FAST-LIO 中 IEKF 更新公式的推导整理

本文整理 FAST-LIO 论文中式 (14) 到式 (18) 附近的几个问题：

- 状态误差如何定义；
- 先验误差线性化中的 $\mathbf J^\kappa$ 从哪里来；
- $\mathbf J^\kappa$ 中旋转块和右雅可比、左雅可比的关系；
- 为什么协方差要写成 $\mathbf P = \left(\mathbf J^\kappa\right)^{-1}\hat{\mathbf P}_k\left(\mathbf J^\kappa\right)^{-T}$；
- 测量雅可比 $\mathbf H_j^\kappa$ 如何推导；
- MAP 问题如何推出迭代 Kalman 更新；
- 后验协方差 $\mathbf P_k = \left(\mathbf I-\mathbf K\mathbf H\right)\mathbf P$ 如何得到。

## 1. 论文中的误差状态定义

FAST-LIO 的误差状态定义为：

$$
\tilde{\mathbf x}_{k-1}
\doteq
\mathbf x_{k-1}
\boxminus
\bar{\mathbf x}_{k-1}
=
\begin{bmatrix}
\delta\boldsymbol\theta^T &
{}^{G}\tilde{\mathbf p}_{I}^{T} &
{}^{G}\tilde{\mathbf v}_{I}^{T} &
\tilde{\mathbf b}_{\omega}^{T} &
\tilde{\mathbf b}_{a}^{T} &
{}^{G}\tilde{\mathbf g}^{T}
\end{bmatrix}^{T}
$$

其中姿态误差为：

$$
\delta\boldsymbol\theta
=
\operatorname{Log}
\left(
\left(
{}^{G}\bar{\mathbf R}_{I}
\right)^T
{}^{G}\mathbf R_{I}
\right)
$$

这说明 FAST-LIO 使用的是右扰动误差。也就是：

$$
{}^{G}\mathbf R_{I}
=
{}^{G}\bar{\mathbf R}_{I}
\operatorname{Exp}
\left(
\delta\boldsymbol\theta
\right)
$$

因此在第 $k$ 时刻、第 $\kappa$ 次迭代点处，有：

$$
\tilde{\mathbf x}_{k}^{\kappa}
=
\mathbf x_k
\boxminus
\hat{\mathbf x}_{k}^{\kappa}
$$

其中旋转误差分量为：

$$
\delta\boldsymbol\theta^{\kappa}
=
\operatorname{Log}
\left(
\left(
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\right)^T
{}^{G}\mathbf R_{I_k}
\right)
$$

等价地：

$$
{}^{G}\mathbf R_{I_k}
=
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\operatorname{Exp}
\left(
\delta\boldsymbol\theta^{\kappa}
\right)
$$

注意同一个真实状态 $\mathbf x_k$ 可以相对于不同参考点定义不同误差：

$$
\delta\mathbf x_k
=
\mathbf x_k
\boxminus
\hat{\mathbf x}_k
$$

是相对于 IMU 前向传播先验中心 $\hat{\mathbf x}_k$ 的误差；

$$
\tilde{\mathbf x}_k^\kappa
=
\mathbf x_k
\boxminus
\hat{\mathbf x}_k^\kappa
$$

是相对于第 $\kappa$ 次 IEKF 迭代线性化点 $\hat{\mathbf x}_k^\kappa$ 的误差。

## 2. 先验协方差为什么是误差协方差

IMU 前向传播得到先验：

$$
\mathbf x_k
\sim
\mathcal N
\left(
\hat{\mathbf x}_k,
\hat{\mathbf P}_k
\right)
$$

在流形状态上，这个写法是简写。严格来说，它表示：

$$
\mathbf x_k
\boxminus
\hat{\mathbf x}_k
\sim
\mathcal N
\left(
\mathbf 0,
\hat{\mathbf P}_k
\right)
$$

因此：

$$
\hat{\mathbf P}_k
=
\operatorname{Cov}
\left(
\mathbf x_k
\boxminus
\hat{\mathbf x}_k
\right)
$$

若记：

$$
\delta\mathbf x_k
=
\mathbf x_k
\boxminus
\hat{\mathbf x}_k
$$

则：

$$
\hat{\mathbf P}_k
=
\operatorname{Cov}
\left(
\delta\mathbf x_k
\right)
$$

也就是说，EKF 中的协方差描述的是状态相对于估计中心的局部误差，而不是直接描述状态变量本身。

## 3. 先验误差的线性化与 J

第 $\kappa$ 次迭代中，真实状态写成：

$$
\mathbf x_k
=
\hat{\mathbf x}_k^\kappa
\boxplus
\tilde{\mathbf x}_k^\kappa
$$

先验项需要的是相对于传播先验中心 $\hat{\mathbf x}_k$ 的误差：

$$
\mathbf x_k
\boxminus
\hat{\mathbf x}_k
$$

代入当前迭代误差：

$$
\mathbf x_k
\boxminus
\hat{\mathbf x}_k
=
\left(
\hat{\mathbf x}_k^\kappa
\boxplus
\tilde{\mathbf x}_k^\kappa
\right)
\boxminus
\hat{\mathbf x}_k
$$

对 $\tilde{\mathbf x}_k^\kappa=\mathbf 0$ 一阶展开：

$$
\left(
\hat{\mathbf x}_k^\kappa
\boxplus
\tilde{\mathbf x}_k^\kappa
\right)
\boxminus
\hat{\mathbf x}_k
\approx
\hat{\mathbf x}_k^\kappa
\boxminus
\hat{\mathbf x}_k
+
\mathbf J^\kappa
\tilde{\mathbf x}_k^\kappa
$$

其中：

$$
\mathbf J^\kappa
=
\left.
\frac{
\partial
\left[
\left(
\hat{\mathbf x}_k^\kappa
\boxplus
\tilde{\mathbf x}_k^\kappa
\right)
\boxminus
\hat{\mathbf x}_k
\right]
}{
\partial\tilde{\mathbf x}_k^\kappa
}
\right|_{\tilde{\mathbf x}_k^\kappa=\mathbf 0}
$$

这个 $\mathbf J^\kappa$ 的作用是把当前迭代误差坐标映射到先验误差坐标：

$$
\delta\mathbf x_k
\approx
\left(
\hat{\mathbf x}_k^\kappa
\boxminus
\hat{\mathbf x}_k
\right)
+
\mathbf J^\kappa
\tilde{\mathbf x}_k^\kappa
$$

## 4. J 中旋转块的右雅可比推导

按照论文的右扰动误差定义，先验旋转误差为：

$$
\mathbf r_R
=
\operatorname{Log}
\left(
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
{}^{G}\mathbf R_{I_k}
\right)
$$

第 $\kappa$ 次迭代点的右扰动为：

$$
{}^{G}\mathbf R_{I_k}
=
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\operatorname{Exp}
\left(
\delta\boldsymbol\theta^\kappa
\right)
$$

代入得：

$$
\mathbf r_R
\left(
\delta\boldsymbol\theta^\kappa
\right)
=
\operatorname{Log}
\left(
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\operatorname{Exp}
\left(
\delta\boldsymbol\theta^\kappa
\right)
\right)
$$

令：

$$
\Delta\mathbf R
=
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
$$

$$
\boldsymbol\phi
=
\operatorname{Log}
\left(
\Delta\mathbf R
\right)
$$

则：

$$
\mathbf r_R
\left(
\delta\boldsymbol\theta^\kappa
\right)
=
\operatorname{Log}
\left(
\Delta\mathbf R
\operatorname{Exp}
\left(
\delta\boldsymbol\theta^\kappa
\right)
\right)
$$

由于：

$$
\Delta\mathbf R
=
\operatorname{Exp}
\left(
\boldsymbol\phi
\right)
$$

所以：

$$
\mathbf r_R
\left(
\delta\boldsymbol\theta^\kappa
\right)
=
\operatorname{Log}
\left(
\operatorname{Exp}
\left(
\boldsymbol\phi
\right)
\operatorname{Exp}
\left(
\delta\boldsymbol\theta^\kappa
\right)
\right)
$$

根据右雅可比逆的一阶展开：

$$
\operatorname{Log}
\left(
\operatorname{Exp}
\left(
\boldsymbol\phi
\right)
\operatorname{Exp}
\left(
\delta\boldsymbol\theta
\right)
\right)
\approx
\boldsymbol\phi
+
\mathbf J_r^{-1}
\left(
\boldsymbol\phi
\right)
\delta\boldsymbol\theta
$$

因此按照论文的右扰动误差定义，旋转块为：

$$
\boxed{
\mathbf J_R^\kappa
=
\mathbf J_r^{-1}
\left(
\boldsymbol\phi
\right)
}
$$

其中：

$$
\boxed{
\boldsymbol\phi
=
\operatorname{Log}
\left(
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\right)
}
$$

右雅可比为：

$$
\mathbf J_r
\left(
\boldsymbol\phi
\right)
=
\mathbf I
-
\frac{1-\cos\theta}{\theta^2}
\left[
\boldsymbol\phi
\right]_\times
+
\frac{\theta-\sin\theta}{\theta^3}
\left[
\boldsymbol\phi
\right]_\times^2
$$

其中：

$$
\theta
=
\left\|
\boldsymbol\phi
\right\|
$$

右雅可比逆为：

$$
\boxed{
\mathbf J_r^{-1}
\left(
\boldsymbol\phi
\right)
=
\mathbf I
+
\frac{1}{2}
\left[
\boldsymbol\phi
\right]_\times
+
\left(
\frac{1}{\theta^2}
-
\frac{1+\cos\theta}{2\theta\sin\theta}
\right)
\left[
\boldsymbol\phi
\right]_\times^2
}
$$

小角度下：

$$
\mathbf J_r^{-1}
\left(
\boldsymbol\phi
\right)
\approx
\mathbf I
+
\frac{1}{2}
\left[
\boldsymbol\phi
\right]_\times
+
\frac{1}{12}
\left[
\boldsymbol\phi
\right]_\times^2
$$

当 $\hat{\mathbf R}_{I_k}^{\kappa}$ 与 $\hat{\mathbf R}_{I_k}$ 很接近时：

$$
\boldsymbol\phi
\approx
\mathbf 0
$$

因此：

$$
\mathbf J_R^\kappa
\approx
\mathbf I
$$

## 5. 论文中 A 与右雅可比的关系

论文中 $\mathbf J^\kappa$ 的旋转块写成类似：

$$
\mathbf A
\left(
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
\right)^{-T}
$$

注意旋转矩阵满足：

$$
\mathbf R^{-T}
=
\mathbf R
$$

因此：

$$
\left(
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
\right)^{-T}
=
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
$$

这里的 $\mathbf A$ 可以理解为左雅可比逆：

$$
\mathbf A
=
\mathbf J_l^{-1}
\left(
\boldsymbol\psi
\right)
$$

其中：

$$
\boldsymbol\psi
=
\operatorname{Log}
\left(
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
\right)
$$

左右雅可比逆之间有关系：

$$
\mathbf J_r^{-1}
\left(
\boldsymbol\phi
\right)
=
\mathbf J_l^{-1}
\left(
\boldsymbol\phi
\right)
\operatorname{Exp}
\left(
\boldsymbol\phi
\right)
$$

所以可以把论文里的整体乘积理解为右雅可比逆的另一种写法。关键点是：

$$
\boxed{
\text{真正对应右雅可比逆的是整块乘积，而不是单独的 } \mathbf A
}
$$

即：

$$
\boxed{
\mathbf A
\left(
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
\right)^{-T}
\quad
\Longleftrightarrow
\quad
\mathbf J_r^{-1}
\left(
\operatorname{Log}
\left(
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\right)
\right)
}
$$

两种写法的差异主要来自旋转误差是在全局坐标表达，还是在右扰动的局部误差坐标表达。

## 6. J 的完整块形式

除旋转外，位置、速度、bias、重力都是欧氏量。它们的误差坐标变换一阶导数为单位阵。因此：

$$
\boxed{
\mathbf J^\kappa
=
\begin{bmatrix}
\mathbf J_R^\kappa
&
\mathbf 0_{3\times 15}
\\
\mathbf 0_{15\times 3}
&
\mathbf I_{15\times 15}
\end{bmatrix}
}
$$

其中：

$$
\mathbf J_R^\kappa
=
\mathbf J_r^{-1}
\left(
\operatorname{Log}
\left(
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\right)
\right)
$$

或者按论文形式写成：

$$
\mathbf J_R^\kappa
=
\mathbf A
\left(
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
\left(
{}^{G}\hat{\mathbf R}_{I_k}
\right)^T
\right)^{-T}
$$

## 7. 为什么协方差要乘 J 的逆

传播得到的先验协方差是：

$$
\hat{\mathbf P}_k
=
\operatorname{Cov}
\left(
\delta\mathbf x_k
\right)
$$

其中：

$$
\delta\mathbf x_k
=
\mathbf x_k
\boxminus
\hat{\mathbf x}_k
$$

但是第 $\kappa$ 次 IEKF 优化变量是：

$$
\tilde{\mathbf x}_k^\kappa
=
\mathbf x_k
\boxminus
\hat{\mathbf x}_k^\kappa
$$

二者满足一阶关系：

$$
\delta\mathbf x_k
\approx
\left(
\hat{\mathbf x}_k^\kappa
\boxminus
\hat{\mathbf x}_k
\right)
+
\mathbf J^\kappa
\tilde{\mathbf x}_k^\kappa
$$

协方差只与随机扰动的线性部分有关，常数项不影响协方差。因此：

$$
\delta\mathbf x_k
\approx
\mathbf J^\kappa
\tilde{\mathbf x}_k^\kappa
$$

所以：

$$
\tilde{\mathbf x}_k^\kappa
\approx
\left(
\mathbf J^\kappa
\right)^{-1}
\delta\mathbf x_k
$$

利用线性变换的协方差公式：

$$
\operatorname{Cov}
\left(
\mathbf A\mathbf z
\right)
=
\mathbf A
\operatorname{Cov}
\left(
\mathbf z
\right)
\mathbf A^T
$$

可得：

$$
\boxed{
\mathbf P
=
\operatorname{Cov}
\left(
\tilde{\mathbf x}_k^\kappa
\right)
=
\left(
\mathbf J^\kappa
\right)^{-1}
\hat{\mathbf P}_k
\left(
\mathbf J^\kappa
\right)^{-T}
}
$$

这里 $\mathbf P$ 是把先验协方差换到当前迭代误差坐标后的协方差。

## 8. 按代码实现的测量模型与 H 推导

本仓库 `h_share_model()` 中使用的状态顺序来自 `state_ikfom`：

$$
\mathbf x
=
\left(
{}^{G}\mathbf p_I,\,
{}^{G}\mathbf R_I,\,
{}^{I}\mathbf R_L,\,
{}^{I}\mathbf p_L,\,
{}^{G}\mathbf v_I,\,
\mathbf b_g,\,
\mathbf b_a,\,
{}^{G}\mathbf g
\right)
$$

代码中的测量雅可比只显式构造前 12 列：

$$
\delta\mathbf x_H
=
\begin{bmatrix}
\delta{}^{G}\mathbf p_I^T &
\delta\boldsymbol\theta_I^T &
\delta\boldsymbol\theta_{LI}^T &
\delta{}^{I}\mathbf p_L^T
\end{bmatrix}^{T}
$$

对应代码：

```cpp
ekfom_data.h_x = MatrixXd::Zero(effct_feat_num, 12);
```

也就是说，速度、陀螺 bias、加计 bias、重力不直接出现在点到平面量测中。它们通过状态协方差中的相关性被间接更新。

经过 IMU 前向传播和点云去畸变后，当前点在代码中已经表示为当前 LiDAR/body 帧下的点：

$$
{}^{L}\mathbf p_j
$$

代码变量为：

```cpp
V3D point_this_be(laser_p.x, laser_p.y, laser_p.z);
```

先把点从 LiDAR 坐标系变到 IMU 坐标系：

$$
{}^{I}\mathbf p_j
=
{}^{I}\mathbf R_L\,{}^{L}\mathbf p_j
+
{}^{I}\mathbf p_L
$$

代码对应：

```cpp
V3D point_this = s.offset_R_L_I * point_this_be + s.offset_T_L_I;
```

再投到全局坐标系：

$$
{}^{G}\mathbf p_j
=
{}^{G}\mathbf R_I\,{}^{I}\mathbf p_j
+
{}^{G}\mathbf p_I
$$

代码对应：

```cpp
V3D p_global = s.rot * (s.offset_R_L_I * p_body + s.offset_T_L_I) + s.pos;
```

地图中最近邻拟合出的平面写作：

$$
{}^{G}\mathbf n_j^T\,{}^{G}\mathbf p
+
d_j
=
0
$$

代码中：

```cpp
norm_p.x, norm_p.y, norm_p.z
```

就是平面法向：

$$
{}^{G}\mathbf n_j
$$

`norm_p.intensity` 保存当前点到平面的有符号距离：

$$
r_j
=
{}^{G}\mathbf n_j^T
{}^{G}\mathbf p_j
+
d_j
$$

代码对应：

```cpp
float pd2 = pabcd(0) * point_world.x
          + pabcd(1) * point_world.y
          + pabcd(2) * point_world.z
          + pabcd(3);
normvec->points[i].intensity = pd2;
```

因此测量线性化采用：

$$
0
\approx
r_j
+
\mathbf H_j
\delta\mathbf x_H
+
v_j
$$

而代码中将右端量测向量存为：

$$
\mathbf h_j
=
-r_j
$$

对应：

```cpp
ekfom_data.h(i) = -norm_p.intensity;
```

下面推导代码中的一行 $\mathbf H_j$。定义：

$$
\mathbf C
=
\left(
{}^{G}\mathbf R_I
\right)^T
{}^{G}\mathbf n_j
$$

代码对应：

```cpp
V3D C(s.rot.conjugate() * norm_vec);
```

### 对全局位置的雅可比

残差为：

$$
r_j
=
{}^{G}\mathbf n_j^T
\left(
{}^{G}\mathbf R_I\,{}^{I}\mathbf p_j
+
{}^{G}\mathbf p_I
\right)
+
d_j
$$

所以：

$$
\frac{\partial r_j}
{\partial \delta{}^{G}\mathbf p_I}
=
\left(
{}^{G}\mathbf n_j
\right)^T
$$

代码写入 H 的前 3 列：

```cpp
norm_p.x, norm_p.y, norm_p.z
```

### 对 IMU 姿态的雅可比

代码使用右扰动：

$$
{}^{G}\mathbf R_I^{+}
=
{}^{G}\mathbf R_I
\operatorname{Exp}
\left(
\delta\boldsymbol\theta_I
\right)
$$

一阶近似：

$$
\operatorname{Exp}
\left(
\delta\boldsymbol\theta_I
\right)
{}^{I}\mathbf p_j
\approx
{}^{I}\mathbf p_j
-
\left[
{}^{I}\mathbf p_j
\right]_\times
\delta\boldsymbol\theta_I
$$

因此：

$$
\frac{\partial r_j}
{\partial \delta\boldsymbol\theta_I}
=
-
\left(
{}^{G}\mathbf n_j
\right)^T
{}^{G}\mathbf R_I
\left[
{}^{I}\mathbf p_j
\right]_\times
$$

利用：

$$
\mathbf C
=
\left(
{}^{G}\mathbf R_I
\right)^T
{}^{G}\mathbf n_j
$$

可得：

$$
\frac{\partial r_j}
{\partial \delta\boldsymbol\theta_I}
=
\left(
\left[
{}^{I}\mathbf p_j
\right]_\times
\mathbf C
\right)^T
$$

代码中：

```cpp
M3D point_crossmat;
point_crossmat << SKEW_SYM_MATRX(point_this);
V3D A(point_crossmat * C);
```

其中：

$$
\mathbf A
=
\left[
{}^{I}\mathbf p_j
\right]_\times
\mathbf C
$$

写入 H 的第 4 到第 6 列。

### 对 LiDAR-IMU 外参旋转的雅可比

外参旋转右扰动：

$$
\left(
{}^{I}\mathbf R_L
\right)^{+}
=
{}^{I}\mathbf R_L
\operatorname{Exp}
\left(
\delta\boldsymbol\theta_{LI}
\right)
$$

IMU 系点为：

$$
{}^{I}\mathbf p_j
=
{}^{I}\mathbf R_L\,{}^{L}\mathbf p_j
+
{}^{I}\mathbf p_L
$$

对外参旋转一阶展开：

$$
{}^{I}\mathbf R_L
\operatorname{Exp}
\left(
\delta\boldsymbol\theta_{LI}
\right)
{}^{L}\mathbf p_j
\approx
{}^{I}\mathbf R_L
\left(
{}^{L}\mathbf p_j
-
\left[
{}^{L}\mathbf p_j
\right]_\times
\delta\boldsymbol\theta_{LI}
\right)
$$

所以：

$$
\frac{\partial r_j}
{\partial \delta\boldsymbol\theta_{LI}}
=
-
\mathbf C^T
{}^{I}\mathbf R_L
\left[
{}^{L}\mathbf p_j
\right]_\times
$$

等价写成列向量形式：

$$
\frac{\partial r_j}
{\partial \delta\boldsymbol\theta_{LI}}
=
\left(
\left[
{}^{L}\mathbf p_j
\right]_\times
\left(
{}^{I}\mathbf R_L
\right)^T
\mathbf C
\right)^T
$$

代码中：

```cpp
M3D point_be_crossmat;
point_be_crossmat << SKEW_SYM_MATRX(point_this_be);
V3D B(point_be_crossmat * s.offset_R_L_I.conjugate() * C);
```

其中：

$$
\mathbf B
=
\left[
{}^{L}\mathbf p_j
\right]_\times
\left(
{}^{I}\mathbf R_L
\right)^T
\mathbf C
$$

写入 H 的第 7 到第 9 列。

### 对 LiDAR-IMU 外参平移的雅可比

由于：

$$
{}^{I}\mathbf p_j
=
{}^{I}\mathbf R_L\,{}^{L}\mathbf p_j
+
{}^{I}\mathbf p_L
$$

所以：

$$
\frac{\partial r_j}
{\partial \delta{}^{I}\mathbf p_L}
=
\left(
{}^{G}\mathbf n_j
\right)^T
{}^{G}\mathbf R_I
=
\mathbf C^T
$$

代码直接写入：

```cpp
VEC_FROM_ARRAY(C)
```

作为 H 的第 10 到第 12 列。

最终，代码实现的单点测量雅可比为：

$$
\boxed{
\mathbf H_j
=
\begin{bmatrix}
\left({}^{G}\mathbf n_j\right)^T
&
\mathbf A^T
&
\mathbf B^T
&
\mathbf C^T
\end{bmatrix}
}
$$

其中：

$$
\boxed{
\mathbf C
=
\left(
{}^{G}\mathbf R_I
\right)^T
{}^{G}\mathbf n_j
}
$$

$$
\boxed{
\mathbf A
=
\left[
{}^{I}\mathbf p_j
\right]_\times
\mathbf C
}
$$

$$
\boxed{
\mathbf B
=
\left[
{}^{L}\mathbf p_j
\right]_\times
\left(
{}^{I}\mathbf R_L
\right)^T
\mathbf C
}
$$

对应代码：

```cpp
ekfom_data.h_x.block<1, 12>(i,0)
    << norm_p.x, norm_p.y, norm_p.z,
       VEC_FROM_ARRAY(A),
       VEC_FROM_ARRAY(B),
       VEC_FROM_ARRAY(C);
```

如果关闭外参估计，代码将外参旋转和平移两块置零：

```cpp
ekfom_data.h_x.block<1, 12>(i,0)
    << norm_p.x, norm_p.y, norm_p.z,
       VEC_FROM_ARRAY(A),
       0.0, 0.0, 0.0,
       0.0, 0.0, 0.0;
```

## 9. 测量噪声项 v

原始测量中有：

$$
{}^{L_j}\mathbf p_{f_j}
-
{}^{L_j}\mathbf n_{f_j}
$$

所以对 ${}^{L_j}\mathbf n_{f_j}$ 求导：

$$
\frac{\partial h_j}{\partial {}^{L_j}\mathbf n_{f_j}}
=
-
{}^{G}\mathbf u_j^T
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
{}^{I_k}\breve{\mathbf R}_{I_j}
{}^{I}\mathbf R_L
$$

因此：

$$
v_j
\approx
\mathbf G_j
{}^{L_j}\mathbf n_{f_j}
$$

其中：

$$
\mathbf G_j
=
-
{}^{G}\mathbf u_j^T
{}^{G}\hat{\mathbf R}_{I_k}^{\kappa}
{}^{I_k}\breve{\mathbf R}_{I_j}
{}^{I}\mathbf R_L
$$

若：

$$
{}^{L_j}\mathbf n_{f_j}
\sim
\mathcal N
\left(
\mathbf 0,
{}^{L_j}\boldsymbol\Sigma_{f_j}
\right)
$$

则：

$$
v_j
\sim
\mathcal N
\left(
0,
R_j
\right)
$$

其中：

$$
R_j
=
\mathbf G_j
{}^{L_j}\boldsymbol\Sigma_{f_j}
\mathbf G_j^T
$$

## 10. MAP 目标函数

堆叠所有点的残差：

$$
\mathbf z_k^\kappa
=
\begin{bmatrix}
\left(
z_1^\kappa
\right)^T &
\cdots &
\left(
z_m^\kappa
\right)^T
\end{bmatrix}^{T}
$$

$$
\mathbf H
=
\begin{bmatrix}
\mathbf H_1^\kappa \\
\vdots \\
\mathbf H_m^\kappa
\end{bmatrix}
$$

$$
\mathbf R
=
\operatorname{diag}
\left(
R_1,
\cdots,
R_m
\right)
$$

测量线性化为：

$$
\mathbf 0
\approx
\mathbf z_k^\kappa
+
\mathbf H
\tilde{\mathbf x}
+
\mathbf v
$$

其中：

$$
\tilde{\mathbf x}
=
\tilde{\mathbf x}_k^\kappa
$$

先验线性化为：

$$
\mathbf x_k
\boxminus
\hat{\mathbf x}_k
\approx
\hat{\mathbf x}_k^\kappa
\boxminus
\hat{\mathbf x}_k
+
\mathbf J^\kappa
\tilde{\mathbf x}
$$

定义：

$$
\mathbf e^\kappa
=
\hat{\mathbf x}_k^\kappa
\boxminus
\hat{\mathbf x}_k
$$

则 MAP 目标为：

$$
\min_{\tilde{\mathbf x}}
\left(
\left\|
\mathbf e^\kappa
+
\mathbf J^\kappa
\tilde{\mathbf x}
\right\|_{\hat{\mathbf P}_k^{-1}}^2
+
\left\|
\mathbf z_k^\kappa
+
\mathbf H
\tilde{\mathbf x}
\right\|_{\mathbf R^{-1}}^2
\right)
$$

利用：

$$
\mathbf P
=
\left(
\mathbf J^\kappa
\right)^{-1}
\hat{\mathbf P}_k
\left(
\mathbf J^\kappa
\right)^{-T}
$$

以及：

$$
\mathbf a^\kappa
=
\left(
\mathbf J^\kappa
\right)^{-1}
\mathbf e^\kappa
=
\left(
\mathbf J^\kappa
\right)^{-1}
\left(
\hat{\mathbf x}_k^\kappa
\boxminus
\hat{\mathbf x}_k
\right)
$$

有：

$$
\mathbf e^\kappa
+
\mathbf J^\kappa
\tilde{\mathbf x}
=
\mathbf J^\kappa
\left(
\tilde{\mathbf x}
+
\mathbf a^\kappa
\right)
$$

因此先验项可写为：

$$
\left\|
\mathbf e^\kappa
+
\mathbf J^\kappa
\tilde{\mathbf x}
\right\|_{\hat{\mathbf P}_k^{-1}}^2
=
\left\|
\tilde{\mathbf x}
+
\mathbf a^\kappa
\right\|_{\mathbf P^{-1}}^2
$$

MAP 目标变成：

$$
\min_{\tilde{\mathbf x}}
\left(
\left\|
\tilde{\mathbf x}
+
\mathbf a^\kappa
\right\|_{\mathbf P^{-1}}^2
+
\left\|
\mathbf z_k^\kappa
+
\mathbf H
\tilde{\mathbf x}
\right\|_{\mathbf R^{-1}}^2
\right)
$$

## 11. 从 MAP 推导迭代 Kalman 更新

令目标函数为：

$$
J
\left(
\tilde{\mathbf x}
\right)
=
\left(
\tilde{\mathbf x}
+
\mathbf a^\kappa
\right)^T
\mathbf P^{-1}
\left(
\tilde{\mathbf x}
+
\mathbf a^\kappa
\right)
+
\left(
\mathbf z_k^\kappa
+
\mathbf H
\tilde{\mathbf x}
\right)^T
\mathbf R^{-1}
\left(
\mathbf z_k^\kappa
+
\mathbf H
\tilde{\mathbf x}
\right)
$$

对 $\tilde{\mathbf x}$ 求导并令零：

$$
\mathbf P^{-1}
\left(
\tilde{\mathbf x}
+
\mathbf a^\kappa
\right)
+
\mathbf H^T
\mathbf R^{-1}
\left(
\mathbf z_k^\kappa
+
\mathbf H
\tilde{\mathbf x}
\right)
=
\mathbf 0
$$

整理得：

$$
\left(
\mathbf P^{-1}
+
\mathbf H^T
\mathbf R^{-1}
\mathbf H
\right)
\tilde{\mathbf x}
=
-
\mathbf P^{-1}
\mathbf a^\kappa
-
\mathbf H^T
\mathbf R^{-1}
\mathbf z_k^\kappa
$$

所以 MAP 最优扰动为：

$$
\tilde{\mathbf x}^{*}
=
-
\left(
\mathbf P^{-1}
+
\mathbf H^T
\mathbf R^{-1}
\mathbf H
\right)^{-1}
\left(
\mathbf P^{-1}
\mathbf a^\kappa
+
\mathbf H^T
\mathbf R^{-1}
\mathbf z_k^\kappa
\right)
$$

定义 Kalman 增益：

$$
\boxed{
\mathbf K
=
\mathbf P
\mathbf H^T
\left(
\mathbf H
\mathbf P
\mathbf H^T
+
\mathbf R
\right)^{-1}
}
$$

利用矩阵恒等式：

$$
\left(
\mathbf P^{-1}
+
\mathbf H^T
\mathbf R^{-1}
\mathbf H
\right)^{-1}
\mathbf H^T
\mathbf R^{-1}
=
\mathbf K
$$

以及：

$$
\left(
\mathbf P^{-1}
+
\mathbf H^T
\mathbf R^{-1}
\mathbf H
\right)^{-1}
\mathbf P^{-1}
=
\mathbf I
-
\mathbf K
\mathbf H
$$

可得：

$$
\tilde{\mathbf x}^{*}
=
-
\mathbf K
\mathbf z_k^\kappa
-
\left(
\mathbf I
-
\mathbf K
\mathbf H
\right)
\mathbf a^\kappa
$$

代回 $\mathbf a^\kappa$：

$$
\boxed{
\tilde{\mathbf x}^{*}
=
-
\mathbf K
\mathbf z_k^\kappa
-
\left(
\mathbf I
-
\mathbf K
\mathbf H
\right)
\left(
\mathbf J^\kappa
\right)^{-1}
\left(
\hat{\mathbf x}_k^\kappa
\boxminus
\hat{\mathbf x}_k
\right)
}
$$

最后用 $\boxplus$ 更新状态：

$$
\boxed{
\hat{\mathbf x}_k^{\kappa+1}
=
\hat{\mathbf x}_k^\kappa
\boxplus
\left(
-
\mathbf K
\mathbf z_k^\kappa
-
\left(
\mathbf I
-
\mathbf K
\mathbf H
\right)
\left(
\mathbf J^\kappa
\right)^{-1}
\left(
\hat{\mathbf x}_k^\kappa
\boxminus
\hat{\mathbf x}_k
\right)
\right)
}
$$

这就是论文式 (18) 的来源。

如果是第一次迭代，通常：

$$
\hat{\mathbf x}_k^\kappa
=
\hat{\mathbf x}_k
$$

则：

$$
\hat{\mathbf x}_k^\kappa
\boxminus
\hat{\mathbf x}_k
=
\mathbf 0
$$

此时更新退化为普通 Kalman 形式：

$$
\hat{\mathbf x}_k^{\kappa+1}
=
\hat{\mathbf x}_k^\kappa
\boxplus
\left(
-
\mathbf K
\mathbf z_k^\kappa
\right)
$$

## 12. 为什么后验信息矩阵相加

考虑简化后的 MAP 目标：

$$
J
\left(
\tilde{\mathbf x}
\right)
=
\frac{1}{2}
\tilde{\mathbf x}^T
\mathbf P^{-1}
\tilde{\mathbf x}
+
\frac{1}{2}
\left(
\mathbf z
+
\mathbf H
\tilde{\mathbf x}
\right)^T
\mathbf R^{-1}
\left(
\mathbf z
+
\mathbf H
\tilde{\mathbf x}
\right)
$$

展开测量项：

$$
\left(
\mathbf z
+
\mathbf H
\tilde{\mathbf x}
\right)^T
\mathbf R^{-1}
\left(
\mathbf z
+
\mathbf H
\tilde{\mathbf x}
\right)
=
\tilde{\mathbf x}^T
\mathbf H^T
\mathbf R^{-1}
\mathbf H
\tilde{\mathbf x}
+
2
\mathbf z^T
\mathbf R^{-1}
\mathbf H
\tilde{\mathbf x}
+
\mathbf z^T
\mathbf R^{-1}
\mathbf z
$$

因此整个目标函数的二次项矩阵是：

$$
\mathbf P^{-1}
+
\mathbf H^T
\mathbf R^{-1}
\mathbf H
$$

对于高斯分布，负对数概率的一般形式为：

$$
-\log p
\left(
\tilde{\mathbf x}
\right)
=
\frac{1}{2}
\left(
\tilde{\mathbf x}
-
\boldsymbol\mu
\right)^T
\boldsymbol\Sigma^{-1}
\left(
\tilde{\mathbf x}
-
\boldsymbol\mu
\right)
+
\text{const}
$$

其中二次项矩阵就是信息矩阵 $\boldsymbol\Sigma^{-1}$。因此后验信息矩阵为：

$$
\boxed{
\mathbf P_k^{-1}
=
\mathbf P^{-1}
+
\mathbf H^T
\mathbf R^{-1}
\mathbf H
}
$$

这里并不是忽略一次项。一次项决定后验均值，二次项决定后验协方差。

一般二次函数：

$$
J
\left(
\mathbf x
\right)
=
\frac{1}{2}
\mathbf x^T
\mathbf A
\mathbf x
+
\mathbf b^T
\mathbf x
+
c
$$

其最优解为：

$$
\mathbf x^{*}
=
-
\mathbf A^{-1}
\mathbf b
$$

配方后：

$$
J
\left(
\mathbf x
\right)
=
\frac{1}{2}
\left(
\mathbf x
-
\mathbf x^{*}
\right)^T
\mathbf A
\left(
\mathbf x
-
\mathbf x^{*}
\right)
+
\text{const}
$$

所以：

$$
\boxed{
\text{一次项决定均值}
}
$$

$$
\boxed{
\text{二次项决定协方差}
}
$$

## 13. 后验协方差更新公式

由上一节可知：

$$
\mathbf P_k
=
\left(
\mathbf P^{-1}
+
\mathbf H^T
\mathbf R^{-1}
\mathbf H
\right)^{-1}
$$

使用 Woodbury 恒等式：

$$
\left(
\mathbf P^{-1}
+
\mathbf H^T
\mathbf R^{-1}
\mathbf H
\right)^{-1}
=
\mathbf P
-
\mathbf P
\mathbf H^T
\left(
\mathbf H
\mathbf P
\mathbf H^T
+
\mathbf R
\right)^{-1}
\mathbf H
\mathbf P
$$

定义：

$$
\mathbf K
=
\mathbf P
\mathbf H^T
\left(
\mathbf H
\mathbf P
\mathbf H^T
+
\mathbf R
\right)^{-1}
$$

代入得：

$$
\mathbf P_k
=
\mathbf P
-
\mathbf K
\mathbf H
\mathbf P
$$

即：

$$
\boxed{
\mathbf P_k
=
\left(
\mathbf I
-
\mathbf K
\mathbf H
\right)
\mathbf P
}
$$

更数值稳定的 Joseph 形式为：

$$
\mathbf P_k
=
\left(
\mathbf I
-
\mathbf K
\mathbf H
\right)
\mathbf P
\left(
\mathbf I
-
\mathbf K
\mathbf H
\right)^T
+
\mathbf K
\mathbf R
\mathbf K^T
$$

当 $\mathbf K$ 取最优 Kalman 增益时，上式可以化简为：

$$
\mathbf P_k
=
\left(
\mathbf I
-
\mathbf K
\mathbf H
\right)
\mathbf P
$$

直观上，$\mathbf H$ 描述测量能观测到的状态方向，$\mathbf K$ 描述测量残差应该如何反馈到状态。融合测量后，这些可观测方向上的不确定性被压缩，所以从先验协方差 $\mathbf P$ 变成后验协方差 $\mathbf P_k$。

## 14. 总结

FAST-LIO 这一段可以按下面的链条理解：

$$
\text{IMU propagation}
\Rightarrow
\left(
\hat{\mathbf x}_k,
\hat{\mathbf P}_k
\right)
$$

$$
\text{change error coordinates}
\Rightarrow
\mathbf P
=
\left(
\mathbf J^\kappa
\right)^{-1}
\hat{\mathbf P}_k
\left(
\mathbf J^\kappa
\right)^{-T}
$$

$$
\text{LiDAR residual linearization}
\Rightarrow
\mathbf 0
\approx
\mathbf z_k^\kappa
+
\mathbf H
\tilde{\mathbf x}_k^\kappa
+
\mathbf v
$$

$$
\text{MAP quadratic optimization}
\Rightarrow
\mathbf K
=
\mathbf P
\mathbf H^T
\left(
\mathbf H
\mathbf P
\mathbf H^T
+
\mathbf R
\right)^{-1}
$$

$$
\text{iterated error update}
\Rightarrow
\tilde{\mathbf x}^{*}
=
-
\mathbf K
\mathbf z_k^\kappa
-
\left(
\mathbf I
-
\mathbf K
\mathbf H
\right)
\left(
\mathbf J^\kappa
\right)^{-1}
\left(
\hat{\mathbf x}_k^\kappa
\boxminus
\hat{\mathbf x}_k
\right)
$$

$$
\text{state update}
\Rightarrow
\hat{\mathbf x}_k^{\kappa+1}
=
\hat{\mathbf x}_k^\kappa
\boxplus
\tilde{\mathbf x}^{*}
$$

$$
\text{posterior covariance}
\Rightarrow
\mathbf P_k
=
\left(
\mathbf I
-
\mathbf K
\mathbf H
\right)
\mathbf P
$$

其中最容易混淆的是两个误差：

$$
\mathbf x_k
\boxminus
\hat{\mathbf x}_k
$$

是先验误差；

$$
\mathbf x_k
\boxminus
\hat{\mathbf x}_k^\kappa
$$

是当前迭代误差。$\mathbf J^\kappa$ 正是二者之间的一阶坐标变换。

## 15. 误差传播矩阵 F 的具体推导

这一节推导 FAST-LIO 中 IMU 前向传播对应的误差传播矩阵：

$$
\tilde{\mathbf x}_{i+1}
\approx
\mathbf F_{\tilde{\mathbf x}}
\tilde{\mathbf x}_i
+
\mathbf F_{\mathbf w}
\mathbf w_i
$$

误差状态顺序是：

$$
\tilde{\mathbf x}_i
=
\begin{bmatrix}
\delta\boldsymbol\theta_i \\
{}^{G}\tilde{\mathbf p}_{I_i} \\
{}^{G}\tilde{\mathbf v}_{I_i} \\
\tilde{\mathbf b}_{\omega i} \\
\tilde{\mathbf b}_{a i} \\
{}^{G}\tilde{\mathbf g}_i
\end{bmatrix}
$$

噪声顺序是：

$$
\mathbf w_i
=
\begin{bmatrix}
\mathbf n_{\omega i} \\
\mathbf n_{a i} \\
\mathbf n_{b\omega i} \\
\mathbf n_{ba i}
\end{bmatrix}
$$

### 15.1 离散 IMU 传播模型

定义去 bias 后的名义角速度和加速度：

$$
\hat{\boldsymbol\omega}_i
=
\boldsymbol\omega_{m i}
-
\hat{\mathbf b}_{\omega i}
$$

$$
\hat{\mathbf a}_i
=
\mathbf a_{m i}
-
\hat{\mathbf b}_{a i}
$$

名义状态传播为：

$$
{}^{G}\hat{\mathbf R}_{I_{i+1}}
=
{}^{G}\hat{\mathbf R}_{I_i}
\operatorname{Exp}
\left(
\hat{\boldsymbol\omega}_i \Delta t
\right)
$$

$$
{}^{G}\hat{\mathbf p}_{I_{i+1}}
=
{}^{G}\hat{\mathbf p}_{I_i}
+
{}^{G}\hat{\mathbf v}_{I_i}
\Delta t
$$

$$
{}^{G}\hat{\mathbf v}_{I_{i+1}}
=
{}^{G}\hat{\mathbf v}_{I_i}
+
\left(
{}^{G}\hat{\mathbf R}_{I_i}
\hat{\mathbf a}_i
+
{}^{G}\hat{\mathbf g}_i
\right)
\Delta t
$$

bias 和重力名义上保持不变。

真实状态传播为：

$$
{}^{G}\mathbf R_{I_{i+1}}
=
{}^{G}\mathbf R_{I_i}
\operatorname{Exp}
\left(
\left(
\hat{\boldsymbol\omega}_i
-
\tilde{\mathbf b}_{\omega i}
-
\mathbf n_{\omega i}
\right)
\Delta t
\right)
$$

$$
{}^{G}\mathbf p_{I_{i+1}}
=
{}^{G}\mathbf p_{I_i}
+
{}^{G}\mathbf v_{I_i}
\Delta t
$$

$$
{}^{G}\mathbf v_{I_{i+1}}
=
{}^{G}\mathbf v_{I_i}
+
\left(
{}^{G}\mathbf R_{I_i}
\left(
\hat{\mathbf a}_i
-
\tilde{\mathbf b}_{a i}
-
\mathbf n_{a i}
\right)
+
{}^{G}\mathbf g_i
\right)
\Delta t
$$

$$
\mathbf b_{\omega,i+1}
=
\mathbf b_{\omega,i}
+
\mathbf n_{b\omega i}
\Delta t
$$

$$
\mathbf b_{a,i+1}
=
\mathbf b_{a,i}
+
\mathbf n_{ba i}
\Delta t
$$

$$
{}^{G}\mathbf g_{i+1}
=
{}^{G}\mathbf g_i
$$

### 15.2 姿态误差传播

论文误差定义是右扰动：

$$
{}^{G}\mathbf R_{I_i}
=
{}^{G}\hat{\mathbf R}_{I_i}
\operatorname{Exp}
\left(
\delta\boldsymbol\theta_i
\right)
$$

下一时刻姿态误差为：

$$
\delta\boldsymbol\theta_{i+1}
=
\operatorname{Log}
\left(
\left(
{}^{G}\hat{\mathbf R}_{I_{i+1}}
\right)^T
{}^{G}\mathbf R_{I_{i+1}}
\right)
$$

代入真实传播和名义传播：

$$
\delta\boldsymbol\theta_{i+1}
=
\operatorname{Log}
\left(
\operatorname{Exp}
\left(
-
\hat{\boldsymbol\omega}_i
\Delta t
\right)
\operatorname{Exp}
\left(
\delta\boldsymbol\theta_i
\right)
\operatorname{Exp}
\left(
\left(
\hat{\boldsymbol\omega}_i
-
\tilde{\mathbf b}_{\omega i}
-
\mathbf n_{\omega i}
\right)
\Delta t
\right)
\right)
$$

令：

$$
\boldsymbol\phi_i
=
\hat{\boldsymbol\omega}_i
\Delta t
$$

对小量 $\delta\boldsymbol\theta_i$、$\tilde{\mathbf b}_{\omega i}$、$\mathbf n_{\omega i}$ 一阶线性化，有：

$$
\delta\boldsymbol\theta_{i+1}
\approx
\operatorname{Exp}
\left(
-
\boldsymbol\phi_i
\right)
\delta\boldsymbol\theta_i
-
\mathbf J_r
\left(
\boldsymbol\phi_i
\right)
\Delta t
\tilde{\mathbf b}_{\omega i}
-
\mathbf J_r
\left(
\boldsymbol\phi_i
\right)
\Delta t
\mathbf n_{\omega i}
$$

论文里记：

$$
\mathbf A
\left(
\boldsymbol\phi_i
\right)
=
\mathbf J_l
\left(
\boldsymbol\phi_i
\right)
$$

而 $\mathrm{SO}(3)$ 上有：

$$
\mathbf J_r
\left(
\boldsymbol\phi_i
\right)
=
\mathbf J_l
\left(
\boldsymbol\phi_i
\right)^T
=
\mathbf A
\left(
\boldsymbol\phi_i
\right)^T
$$

所以：

$$
\delta\boldsymbol\theta_{i+1}
\approx
\operatorname{Exp}
\left(
-
\hat{\boldsymbol\omega}_i
\Delta t
\right)
\delta\boldsymbol\theta_i
-
\mathbf A
\left(
\hat{\boldsymbol\omega}_i
\Delta t
\right)^T
\Delta t
\tilde{\mathbf b}_{\omega i}
-
\mathbf A
\left(
\hat{\boldsymbol\omega}_i
\Delta t
\right)^T
\Delta t
\mathbf n_{\omega i}
$$

这对应矩阵第一行：

$$
\mathbf F_{\tilde{\mathbf x}}^{\theta,\theta}
=
\operatorname{Exp}
\left(
-
\hat{\boldsymbol\omega}_i
\Delta t
\right)
$$

$$
\mathbf F_{\tilde{\mathbf x}}^{\theta,b_\omega}
=
-
\mathbf A
\left(
\hat{\boldsymbol\omega}_i
\Delta t
\right)^T
\Delta t
$$

$$
\mathbf F_{\mathbf w}^{\theta,n_\omega}
=
-
\mathbf A
\left(
\hat{\boldsymbol\omega}_i
\Delta t
\right)^T
\Delta t
$$

### 15.3 位置误差传播

位置传播为：

$$
{}^{G}\mathbf p_{I_{i+1}}
=
{}^{G}\mathbf p_{I_i}
+
{}^{G}\mathbf v_{I_i}
\Delta t
$$

名义传播为：

$$
{}^{G}\hat{\mathbf p}_{I_{i+1}}
=
{}^{G}\hat{\mathbf p}_{I_i}
+
{}^{G}\hat{\mathbf v}_{I_i}
\Delta t
$$

两者相减：

$$
{}^{G}\tilde{\mathbf p}_{I_{i+1}}
=
{}^{G}\tilde{\mathbf p}_{I_i}
+
{}^{G}\tilde{\mathbf v}_{I_i}
\Delta t
$$

因此：

$$
\mathbf F_{\tilde{\mathbf x}}^{p,p}
=
\mathbf I
$$

$$
\mathbf F_{\tilde{\mathbf x}}^{p,v}
=
\mathbf I
\Delta t
$$

位置这一行对噪声没有直接项，所以：

$$
\mathbf F_{\mathbf w}^{p,\cdot}
=
\mathbf 0
$$

### 15.4 速度误差传播

真实速度传播：

$$
{}^{G}\mathbf v_{I_{i+1}}
=
{}^{G}\mathbf v_{I_i}
+
\left(
{}^{G}\mathbf R_{I_i}
\left(
\hat{\mathbf a}_i
-
\tilde{\mathbf b}_{a i}
-
\mathbf n_{a i}
\right)
+
{}^{G}\mathbf g_i
\right)
\Delta t
$$

名义速度传播：

$$
{}^{G}\hat{\mathbf v}_{I_{i+1}}
=
{}^{G}\hat{\mathbf v}_{I_i}
+
\left(
{}^{G}\hat{\mathbf R}_{I_i}
\hat{\mathbf a}_i
+
{}^{G}\hat{\mathbf g}_i
\right)
\Delta t
$$

两者相减：

$$
{}^{G}\tilde{\mathbf v}_{I_{i+1}}
=
{}^{G}\tilde{\mathbf v}_{I_i}
+
\left(
{}^{G}\mathbf R_{I_i}
\left(
\hat{\mathbf a}_i
-
\tilde{\mathbf b}_{a i}
-
\mathbf n_{a i}
\right)
-
{}^{G}\hat{\mathbf R}_{I_i}
\hat{\mathbf a}_i
+
{}^{G}\tilde{\mathbf g}_i
\right)
\Delta t
$$

用右扰动：

$$
{}^{G}\mathbf R_{I_i}
=
{}^{G}\hat{\mathbf R}_{I_i}
\operatorname{Exp}
\left(
\delta\boldsymbol\theta_i
\right)
$$

小角度下：

$$
\operatorname{Exp}
\left(
\delta\boldsymbol\theta_i
\right)
\hat{\mathbf a}_i
\approx
\hat{\mathbf a}_i
-
\left[
\hat{\mathbf a}_i
\right]_\wedge
\delta\boldsymbol\theta_i
$$

所以：

$$
{}^{G}\mathbf R_{I_i}
\hat{\mathbf a}_i
-
{}^{G}\hat{\mathbf R}_{I_i}
\hat{\mathbf a}_i
\approx
-
{}^{G}\hat{\mathbf R}_{I_i}
\left[
\hat{\mathbf a}_i
\right]_\wedge
\delta\boldsymbol\theta_i
$$

忽略二阶小量后：

$$
{}^{G}\tilde{\mathbf v}_{I_{i+1}}
\approx
{}^{G}\tilde{\mathbf v}_{I_i}
-
{}^{G}\hat{\mathbf R}_{I_i}
\left[
\hat{\mathbf a}_i
\right]_\wedge
\Delta t
\delta\boldsymbol\theta_i
-
{}^{G}\hat{\mathbf R}_{I_i}
\Delta t
\tilde{\mathbf b}_{a i}
+
\mathbf I
\Delta t
{}^{G}\tilde{\mathbf g}_i
-
{}^{G}\hat{\mathbf R}_{I_i}
\Delta t
\mathbf n_{a i}
$$

因此：

$$
\mathbf F_{\tilde{\mathbf x}}^{v,\theta}
=
-
{}^{G}\hat{\mathbf R}_{I_i}
\left[
\hat{\mathbf a}_i
\right]_\wedge
\Delta t
$$

$$
\mathbf F_{\tilde{\mathbf x}}^{v,v}
=
\mathbf I
$$

$$
\mathbf F_{\tilde{\mathbf x}}^{v,b_a}
=
-
{}^{G}\hat{\mathbf R}_{I_i}
\Delta t
$$

$$
\mathbf F_{\tilde{\mathbf x}}^{v,g}
=
\mathbf I
\Delta t
$$

$$
\mathbf F_{\mathbf w}^{v,n_a}
=
-
{}^{G}\hat{\mathbf R}_{I_i}
\Delta t
$$

### 15.5 bias 和重力误差传播

陀螺 bias：

$$
\tilde{\mathbf b}_{\omega,i+1}
=
\tilde{\mathbf b}_{\omega,i}
+
\mathbf n_{b\omega i}
\Delta t
$$

所以：

$$
\mathbf F_{\tilde{\mathbf x}}^{b_\omega,b_\omega}
=
\mathbf I
$$

$$
\mathbf F_{\mathbf w}^{b_\omega,n_{b\omega}}
=
\mathbf I
\Delta t
$$

加速度 bias：

$$
\tilde{\mathbf b}_{a,i+1}
=
\tilde{\mathbf b}_{a,i}
+
\mathbf n_{ba i}
\Delta t
$$

所以：

$$
\mathbf F_{\tilde{\mathbf x}}^{b_a,b_a}
=
\mathbf I
$$

$$
\mathbf F_{\mathbf w}^{b_a,n_{ba}}
=
\mathbf I
\Delta t
$$

重力：

$$
{}^{G}\tilde{\mathbf g}_{i+1}
=
{}^{G}\tilde{\mathbf g}_i
$$

所以：

$$
\mathbf F_{\tilde{\mathbf x}}^{g,g}
=
\mathbf I
$$

噪声项为零。

### 15.6 最终组合成矩阵

于是：

$$
\boxed{
\mathbf F_{\tilde{\mathbf x}}
=
\begin{bmatrix}
\operatorname{Exp}
\left(
-
\hat{\boldsymbol\omega}_i
\Delta t
\right)
&
\mathbf 0
&
\mathbf 0
&
-
\mathbf A
\left(
\hat{\boldsymbol\omega}_i
\Delta t
\right)^T
\Delta t
&
\mathbf 0
&
\mathbf 0
\\
\mathbf 0
&
\mathbf I
&
\mathbf I
\Delta t
&
\mathbf 0
&
\mathbf 0
&
\mathbf 0
\\
-
{}^{G}\hat{\mathbf R}_{I_i}
\left[
\hat{\mathbf a}_i
\right]_\wedge
\Delta t
&
\mathbf 0
&
\mathbf I
&
\mathbf 0
&
-
{}^{G}\hat{\mathbf R}_{I_i}
\Delta t
&
\mathbf I
\Delta t
\\
\mathbf 0
&
\mathbf 0
&
\mathbf 0
&
\mathbf I
&
\mathbf 0
&
\mathbf 0
\\
\mathbf 0
&
\mathbf 0
&
\mathbf 0
&
\mathbf 0
&
\mathbf I
&
\mathbf 0
\\
\mathbf 0
&
\mathbf 0
&
\mathbf 0
&
\mathbf 0
&
\mathbf 0
&
\mathbf I
\end{bmatrix}
}
$$

以及：

$$
\boxed{
\mathbf F_{\mathbf w}
=
\begin{bmatrix}
-
\mathbf A
\left(
\hat{\boldsymbol\omega}_i
\Delta t
\right)^T
\Delta t
&
\mathbf 0
&
\mathbf 0
&
\mathbf 0
\\
\mathbf 0
&
\mathbf 0
&
\mathbf 0
&
\mathbf 0
\\
\mathbf 0
&
-
{}^{G}\hat{\mathbf R}_{I_i}
\Delta t
&
\mathbf 0
&
\mathbf 0
\\
\mathbf 0
&
\mathbf 0
&
\mathbf I
\Delta t
&
\mathbf 0
\\
\mathbf 0
&
\mathbf 0
&
\mathbf 0
&
\mathbf I
\Delta t
\\
\mathbf 0
&
\mathbf 0
&
\mathbf 0
&
\mathbf 0
\end{bmatrix}
}
$$

核心记忆方式是：

$$
\mathbf F_{\tilde{\mathbf x}}
$$

来自每个误差项对上一时刻误差的偏导；

$$
\mathbf F_{\mathbf w}
$$

来自每个误差项对 IMU 噪声的偏导。

其中最特殊的是姿态项：

$$
\operatorname{Exp}
\left(
-
\hat{\boldsymbol\omega}_i
\Delta t
\right)
$$

来自右扰动误差坐标的旋转传播；

$$
-
\mathbf A
\left(
\hat{\boldsymbol\omega}_i
\Delta t
\right)^T
\Delta t
$$

来自陀螺 bias 和陀螺噪声对姿态积分增量的影响。
