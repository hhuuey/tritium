# 《On-Manifold Preintegration for Real-Time Visual-Inertial Odometry》Structureless Vision Factor 详解

本文解释论文中关于 structureless vision factor 的一段推导，重点说明：

- 为什么要使用 \(E_l\) 的左零空间；
- \(E_l^\perp\) 是什么；
- \(Q\,\text{residual}\) 是什么意思；
- 为什么最终可以把代价函数写成更小、更高效的形式。

## 1. 核心直觉

对某个 landmark \(l\)，线性化后的视觉残差可以写成：

\[
r_l = F_l \delta x + E_l \delta p_l - b_l
\]

其中：

\[
E_l \in \mathbb{R}^{2n_l \times 3}, 
\qquad
\delta p_l \in \mathbb{R}^3,
\qquad
r_l \in \mathbb{R}^{2n_l}
\]

这里 \(n_l\) 是看到这个 landmark 的相机数量。每个相机贡献二维像素残差，所以总残差维度是 \(2n_l\)。

\(E_l\) 的含义是：

\[
E_l \delta p_l
\]

表示“如果三维 landmark 的位置变化 \(\delta p_l\)，它会在所有图像观测残差里造成什么变化”。

所以 \(E_l\) 的列空间：

\[
\operatorname{Col}(E_l)
=
\{E_l \delta p_l \mid \delta p_l \in \mathbb{R}^3\}
\]

表示：

\[
\text{所有可以通过移动 landmark 来解释的残差变化方向}
\]

structureless vision factor 的目标就是：消掉 landmark 变量 \(\delta p_l\)，只保留真正约束相机/IMU 状态 \(\delta x\) 的那部分视觉残差。

## 2. 为什么对应 \(E_l\) 的左零空间

我们想去掉 landmark 变量 \(\delta p_l\)。也就是说，我们只想保留那些不能通过调整 landmark 位置来解释的残差方向。

这些方向应该和 \(E_l\) 的列空间正交。

假设有一个方向：

\[
w \in \mathbb{R}^{2n_l}
\]

如果它满足：

\[
w^\top E_l = 0
\]

等价于：

\[
E_l^\top w = 0
\]

那么说明 \(w\) 和 \(E_l\) 的每一列都正交。

而 \(E_l\) 的每一列代表一种 landmark 位置变化造成的残差方向，所以：

\[
w^\top E_l = 0
\]

的意思就是：

\[
w \text{ 方向上的残差不可能通过移动 landmark 来解释}
\]

所有满足这个条件的 \(w\) 构成的空间叫做 \(E_l\) 的左零空间：

\[
\mathcal{N}(E_l^\top)
=
\{w \in \mathbb{R}^{2n_l} \mid E_l^\top w = 0\}
\]

注意为什么是左零空间，而不是普通零空间。

普通零空间是：

\[
E_l v = 0
\]

这里：

\[
v \in \mathbb{R}^3
\]

它生活在 landmark 增量空间里。

但我们现在要投影的是残差：

\[
r_l \in \mathbb{R}^{2n_l}
\]

残差生活在 \(2n_l\) 维观测空间里。所以我们需要的是这个观测空间里的无关方向，也就是：

\[
E_l^\top w = 0
\]

这就是左零空间。

## 3. \(E_l^\perp\) 是什么

\(E_l^\perp\) 是一个矩阵，它的每一列都是 \(E_l\) 左零空间的一组基向量。

也就是：

\[
E_l^\perp =
\begin{bmatrix}
w_1 & w_2 & \cdots & w_{2n_l - 3}
\end{bmatrix}
\]

其中每个 \(w_i\) 都满足：

\[
E_l^\top w_i = 0
\]

所以整体写成：

\[
(E_l^\perp)^\top E_l = 0
\]

它的维度是：

\[
E_l^\perp \in \mathbb{R}^{2n_l \times (2n_l - 3)}
\]

原因是：

\[
E_l \in \mathbb{R}^{2n_l \times 3}
\]

如果 \(E_l\) 满秩，也就是：

\[
\operatorname{rank}(E_l) = 3
\]

那么根据秩-零度定理，左零空间维度是：

\[
\dim \mathcal{N}(E_l^\top)
=
2n_l - 3
\]

所以 \(E_l^\perp\) 有 \(2n_l - 3\) 列。

简单说：

\[
E_l^\perp
\]

不是一个新的物理量，而是一组“筛选方向”。它用来筛出残差里那些无法被 landmark 位置变化解释的部分。

## 4. \(Q\,\text{residual}\) 是什么意思

论文定义：

\[
Q =
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\]

这是一个正交投影矩阵。

其中：

\[
E_l(E_l^\top E_l)^{-1}E_l^\top
\]

是把向量投影到 \(E_l\) 的列空间上，也就是投影到：

\[
\operatorname{Col}(E_l)
\]

而：

\[
Q =
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\]

就是把这部分减掉，因此它把向量投影到 \(E_l\) 列空间的正交补空间，也就是 \(E_l\) 的左零空间：

\[
\mathcal{N}(E_l^\top)
\]

所以：

\[
Q\,\text{residual}
\]

的意思是：

\[
\text{把 residual 投影到 } E_l \text{ 的左零空间}
\]

也就是：

\[
\text{去掉 residual 中可以由 landmark 移动解释的部分}
\]

只保留：

\[
\text{无法由 landmark 移动解释的部分}
\]

如果令：

\[
\text{residual} = F_l \delta x - b_l
\]

那么：

\[
Q(F_l \delta x - b_l)
\]

就是消掉 landmark 后，真正约束状态变量 \(\delta x\) 的残差。

## 5. 一个简单二维例子

假设残差是二维的：

\[
r =
\begin{bmatrix}
3 \\
5
\end{bmatrix}
\]

假设 landmark 的变化只能影响第一个方向：

\[
E =
\begin{bmatrix}
1 \\
0
\end{bmatrix}
\]

那么 \(E\) 的列空间是：

\[
\operatorname{Col}(E)
=
\left\{
\begin{bmatrix}
a \\
0
\end{bmatrix}
\right\}
\]

这代表：第一维误差可以通过移动 landmark 来解释。

和它正交的方向是：

\[
E^\perp =
\begin{bmatrix}
0 \\
1
\end{bmatrix}
\]

因为：

\[
(E^\perp)^\top E
=
\begin{bmatrix}
0 & 1
\end{bmatrix}
\begin{bmatrix}
1 \\
0
\end{bmatrix}
=
0
\]

投影矩阵是：

\[
Q =
I - E(E^\top E)^{-1}E^\top
\]

代入：

\[
E^\top E = 1
\]

所以：

\[
Q =
\begin{bmatrix}
1 & 0 \\
0 & 1
\end{bmatrix}
-
\begin{bmatrix}
1 \\
0
\end{bmatrix}
\begin{bmatrix}
1 & 0
\end{bmatrix}
=
\begin{bmatrix}
0 & 0 \\
0 & 1
\end{bmatrix}
\]

于是：

\[
Qr =
\begin{bmatrix}
0 & 0 \\
0 & 1
\end{bmatrix}
\begin{bmatrix}
3 \\
5
\end{bmatrix}
=
\begin{bmatrix}
0 \\
5
\end{bmatrix}
\]

这表示：

\[
\begin{bmatrix}
3 \\
0
\end{bmatrix}
\]

这部分误差可以通过移动 landmark 消掉，而：

\[
\begin{bmatrix}
0 \\
5
\end{bmatrix}
\]

这部分不能通过移动 landmark 消掉，所以它保留下来约束相机/IMU 状态。

等价地，也可以直接算：

\[
(E^\perp)^\top r
=
\begin{bmatrix}
0 & 1
\end{bmatrix}
\begin{bmatrix}
3 \\
5
\end{bmatrix}
=
5
\]

这就是论文为什么最后把：

\[
\left\|Q(F_l \delta x - b_l)\right\|^2
\]

改写成：

\[
\left\|(E_l^\perp)^\top(F_l \delta x - b_l)\right\|^2
\]

因为后者维度更小，而且不用显式构造 \(Q\)。

## 6. 和论文公式的对应关系

论文中：

\[
Q =
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\]

表示把残差投影到 \(E_l\) 的左零空间。

如果 \(E_l^\perp\) 是 \(E_l\) 左零空间的一组基，那么：

\[
E_l^\perp
\left((E_l^\perp)^\top E_l^\perp\right)^{-1}
(E_l^\perp)^\top
=
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\]

如果通过 SVD 得到的 \(E_l^\perp\) 是单位正交基，则：

\[
(E_l^\perp)^\top E_l^\perp = I
\]

于是投影矩阵可以简化为：

\[
Q = E_l^\perp (E_l^\perp)^\top
\]

因此：

\[
\left\|Q(F_l \delta x - b_l)\right\|^2
=
\left\|E_l^\perp (E_l^\perp)^\top(F_l \delta x - b_l)\right\|^2
\]

由于 \(E_l^\perp\) 是单位正交基：

\[
(E_l^\perp)^\top E_l^\perp = I
\]

所以范数可以进一步化简为：

\[
\left\|(E_l^\perp)^\top(F_l \delta x - b_l)\right\|^2
\]

这个形式不再显式包含 landmark 增量 \(\delta p_l\)，因此就是 structureless vision factor。

## 7. 公式 (82) 的含义

公式 (82) 是：

\[
E_l^\perp
\left(
(E_l^\perp)^\top E_l^\perp
\right)^{-1}
(E_l^\perp)^\top
=
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\]

它的核心含义是：

\[
\text{左边和右边是同一个投影矩阵，只是写法不同}
\]

右边：

\[
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\]

表示把向量投影到 \(E_l\) 列空间的正交补空间，也就是：

\[
\mathcal{N}(E_l^\top)
\]

原因是：

\[
E_l(E_l^\top E_l)^{-1}E_l^\top
\]

是投影到 \(E_l\) 的列空间：

\[
\operatorname{Col}(E_l)
\]

所以：

\[
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\]

就是去掉 \(E_l\) 列空间里的分量，留下和它正交的分量。

左边：

\[
E_l^\perp
\left(
(E_l^\perp)^\top E_l^\perp
\right)^{-1}
(E_l^\perp)^\top
\]

表示把向量投影到 \(E_l^\perp\) 的列空间。

而 \(E_l^\perp\) 的列向量正好是一组 \(E_l\) 左零空间的基，所以：

\[
\operatorname{Col}(E_l^\perp)
=
\mathcal{N}(E_l^\top)
\]

因此左边也是投影到：

\[
\mathcal{N}(E_l^\top)
\]

所以公式 (82) 本质上是在说：

\[
\text{投影到 } E_l \text{ 左零空间}
\]

可以写成：

\[
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\]

也可以写成：

\[
E_l^\perp
\left(
(E_l^\perp)^\top E_l^\perp
\right)^{-1}
(E_l^\perp)^\top
\]

两者完全等价。

如果 \(E_l^\perp\) 是单位正交基，那么：

\[
(E_l^\perp)^\top E_l^\perp = I
\]

于是公式 (82) 进一步变成：

\[
E_l^\perp(E_l^\perp)^\top
=
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\]

一句话总结：

\[
\text{公式 (82) 说明：用 } E_l \text{ 构造的投影矩阵，等价于用 } E_l^\perp \text{ 构造的投影矩阵。}
\]

它们都在做同一件事：

\[
\text{把残差投影到 landmark 解释不了的方向上}
\]

## 8. 如何记住 \(E_l(E_l^\top E_l)^{-1}E_l^\top\)

可以直接把下面这个形式记住：

\[
\boxed{
A(A^\top A)^{-1}A^\top
}
\]

它表示：

\[
\boxed{
\text{投影到 } A \text{ 的列空间}
}
\]

前提是 \(A\) 的列满秩。

所以看到：

\[
E_l(E_l^\top E_l)^{-1}E_l^\top
\]

就可以直接读成：

\[
\text{投影到 } E_l \text{ 的列空间}
\]

也就是：

\[
\text{投影到 landmark 可以解释的残差方向}
\]

为什么它是投影矩阵？可以从最小二乘角度理解。

给定一个残差向量 \(r\)，我们想在 \(E_l\) 的列空间里找一个最接近 \(r\) 的向量：

\[
\min_a \|E_l a - r\|^2
\]

这个问题的解是：

\[
a^\star = (E_l^\top E_l)^{-1}E_l^\top r
\]

所以投影结果是：

\[
E_l a^\star
=
E_l(E_l^\top E_l)^{-1}E_l^\top r
\]

因此：

\[
E_l(E_l^\top E_l)^{-1}E_l^\top r
\]

就是 \(r\) 在 \(E_l\) 列空间里的投影。

在论文语境里：

\[
E_l(E_l^\top E_l)^{-1}E_l^\top r
=
r \text{ 中 landmark 可以解释的部分}
\]

相应地：

\[
\left(
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\right)r
\]

就是：

\[
r \text{ 中 landmark 解释不了的部分}
\]

配套记忆如下：

\[
\boxed{
E_l(E_l^\top E_l)^{-1}E_l^\top
=
\text{投到 } E_l \text{ 的列空间}
}
\]

\[
\boxed{
I - E_l(E_l^\top E_l)^{-1}E_l^\top
=
\text{投到 } E_l \text{ 列空间的正交补，也就是 } E_l^\top \text{ 的零空间}
}
\]

## 9. 为什么 SVD 得到的 \(E_l^\perp\) 满足 \((E_l^\perp)^\top E_l^\perp = I\)

论文中说：

\[
\text{A basis for the null space can be easily computed from } E_l \text{ using SVD.}
\]

意思是：可以用 SVD 从 \(E_l\) 里很容易算出它左零空间的一组基。

对 \(E_l\) 做 SVD：

\[
E_l = U \Sigma V^\top
\]

其中：

\[
E_l \in \mathbb{R}^{2n_l \times 3}
\]

如果 \(E_l\) 满秩：

\[
\operatorname{rank}(E_l) = 3
\]

那么 \(U\) 是一个正交矩阵。可以把 \(U\) 的列分成两部分：

\[
U =
\begin{bmatrix}
U_1 & U_0
\end{bmatrix}
\]

其中：

\[
U_1 \in \mathbb{R}^{2n_l \times 3}
\]

对应非零奇异值，也就是 \(E_l\) 的列空间方向；

\[
U_0 \in \mathbb{R}^{2n_l \times (2n_l - 3)}
\]

对应零奇异值，也就是 \(E_l\) 的左零空间方向。

因此可以取：

\[
E_l^\perp = U_0
\]

为什么这时满足：

\[
(E_l^\perp)^\top E_l^\perp = I
\]

因为 SVD 中的 \(U\) 是正交矩阵：

\[
U^\top U = I
\]

正交矩阵的列向量有两个性质：

\[
\text{每个列向量长度为 } 1
\]

\[
\text{不同列向量之间互相垂直}
\]

既然 \(E_l^\perp = U_0\) 是从 \(U\) 中取出的一部分列，那么这些列仍然两两正交、长度为 1。所以：

\[
U_0^\top U_0 = I
\]

也就是：

\[
(E_l^\perp)^\top E_l^\perp = I
\]

如果写成基向量形式：

\[
E_l^\perp =
\begin{bmatrix}
w_1 & w_2 & \cdots & w_k
\end{bmatrix}
\]

那么：

\[
(E_l^\perp)^\top E_l^\perp
=
\begin{bmatrix}
w_1^\top w_1 & w_1^\top w_2 & \cdots \\
w_2^\top w_1 & w_2^\top w_2 & \cdots \\
\vdots & \vdots & \ddots
\end{bmatrix}
\]

单位正交基满足：

\[
w_i^\top w_j =
\begin{cases}
1, & i=j \\
0, & i \neq j
\end{cases}
\]

所以：

\[
(E_l^\perp)^\top E_l^\perp = I
\]

论文强调这一点，是因为公式 (82) 原本有一项：

\[
\left((E_l^\perp)^\top E_l^\perp\right)^{-1}
\]

如果：

\[
(E_l^\perp)^\top E_l^\perp = I
\]

那么：

\[
\left((E_l^\perp)^\top E_l^\perp\right)^{-1}
=
I^{-1}
=
I
\]

于是投影矩阵可以简化为：

\[
E_l^\perp(E_l^\perp)^\top
\]

这就是后面代价函数能够进一步化简的原因。

## 10. 欧拉角速率变换矩阵的推导

论文最后还提到用欧拉角积分陀螺仪角速度：

\[
\theta_{k+1}
=
\theta_k
+
[E'(\theta_k)]^{-1}
(\tilde{\omega}_k - \eta_k^g)
\Delta t
\]

这里的关键是：

\[
\dot{\theta} \neq \omega
\]

欧拉角速率 \(\dot{\theta}\) 和刚体角速度 \(\omega\) 之间需要一个转换矩阵。

最核心的推导思路是：

\[
\boxed{
\text{把每个欧拉角速率对应的旋转轴，都表达到同一个坐标系里，然后把它们加起来。}
}
\]

下面用常见的 ZYX，也就是 yaw-pitch-roll 约定来推导。

设欧拉角为：

\[
\theta =
\begin{bmatrix}
\phi \\
\vartheta \\
\psi
\end{bmatrix}
\]

其中：

\[
\phi = \text{roll}
\]

\[
\vartheta = \text{pitch}
\]

\[
\psi = \text{yaw}
\]

旋转矩阵写成：

\[
R = R_z(\psi)R_y(\vartheta)R_x(\phi)
\]

也就是先 roll，再 pitch，再 yaw。

欧拉角速率是：

\[
\dot{\theta}
=
\begin{bmatrix}
\dot{\phi} \\
\dot{\vartheta} \\
\dot{\psi}
\end{bmatrix}
\]

但是刚体角速度一般不是：

\[
\omega =
\begin{bmatrix}
\dot{\phi} \\
\dot{\vartheta} \\
\dot{\psi}
\end{bmatrix}
\]

原因是三个欧拉角速率对应的旋转轴不在同一个坐标系里。

对于 ZYX 欧拉角：

- \(\dot{\psi}\)：绕世界系 \(z\) 轴转；
- \(\dot{\vartheta}\)：绕 yaw 之后的中间坐标系 \(y\) 轴转；
- \(\dot{\phi}\)：绕最终 body 系 \(x\) 轴转。

所以刚体角速度是这三个旋转分量的叠加。

如果把角速度表达在 body 坐标系中，记为：

\[
\omega^b =
\begin{bmatrix}
p \\
q \\
r
\end{bmatrix}
\]

那么三个旋转轴在 body 系下分别是：

roll 轴：

\[
\begin{bmatrix}
1 \\
0 \\
0
\end{bmatrix}
\]

pitch 轴：

\[
\begin{bmatrix}
0 \\
\cos\phi \\
-\sin\phi
\end{bmatrix}
\]

yaw 轴：

\[
\begin{bmatrix}
-\sin\vartheta \\
\sin\phi\cos\vartheta \\
\cos\phi\cos\vartheta
\end{bmatrix}
\]

因此：

\[
\omega^b
=
\dot{\phi}
\begin{bmatrix}
1 \\
0 \\
0
\end{bmatrix}
+
\dot{\vartheta}
\begin{bmatrix}
0 \\
\cos\phi \\
-\sin\phi
\end{bmatrix}
+
\dot{\psi}
\begin{bmatrix}
-\sin\vartheta \\
\sin\phi\cos\vartheta \\
\cos\phi\cos\vartheta
\end{bmatrix}
\]

把它写成矩阵形式：

\[
\omega^b
=
E(\phi,\vartheta)
\begin{bmatrix}
\dot{\phi} \\
\dot{\vartheta} \\
\dot{\psi}
\end{bmatrix}
\]

其中：

\[
E(\phi,\vartheta)
=
\begin{bmatrix}
1 & 0 & -\sin\vartheta \\
0 & \cos\phi & \sin\phi\cos\vartheta \\
0 & -\sin\phi & \cos\phi\cos\vartheta
\end{bmatrix}
\]

这就是一种常见的欧拉角速率变换矩阵。

所以：

\[
\boxed{
\omega^b = E(\phi,\vartheta)\dot{\theta}
}
\]

反过来，如果陀螺仪给的是 body 系角速度：

\[
\omega^b =
\begin{bmatrix}
p \\
q \\
r
\end{bmatrix}
\]

那么欧拉角速率为：

\[
\dot{\theta}
=
E(\phi,\vartheta)^{-1}\omega^b
\]

也就是：

\[
\begin{bmatrix}
\dot{\phi} \\
\dot{\vartheta} \\
\dot{\psi}
\end{bmatrix}
=
\begin{bmatrix}
1 & \sin\phi\tan\vartheta & \cos\phi\tan\vartheta \\
0 & \cos\phi & -\sin\phi \\
0 & \frac{\sin\phi}{\cos\vartheta} & \frac{\cos\phi}{\cos\vartheta}
\end{bmatrix}
\begin{bmatrix}
p \\
q \\
r
\end{bmatrix}
\]

这就是论文公式中类似：

\[
\dot{\theta}
=
[E'(\theta)]^{-1}\omega
\]

的来源。

论文中的 \(E'(\theta)\) 具体形式会取决于它采用的欧拉角顺序、坐标系约定，以及角速度是在 body frame 还是 world frame 表达。因此最好不要脱离上下文死记某一个矩阵形式，而是记住它的作用：

\[
\boxed{
E'(\theta): \dot{\theta} \mapsto \omega
}
\]

\[
\boxed{
[E'(\theta)]^{-1}: \omega \mapsto \dot{\theta}
}
\]

欧拉角速率矩阵还有一个重要性质。对上面的 ZYX-body 形式：

\[
\det E(\phi,\vartheta) = \cos\vartheta
\]

所以当：

\[
\cos\vartheta = 0
\]

也就是：

\[
\vartheta = \pm \frac{\pi}{2}
\]

时，矩阵不可逆。这就是欧拉角的奇异性，也就是常说的 gimbal lock。

一句话总结：

\[
\boxed{
\text{欧拉角速率矩阵的每一列，就是对应欧拉角旋转轴在同一个坐标系下的表达。}
}
\]

它乘上欧拉角速率：

\[
\dot{\theta}
\]

就得到真实刚体角速度：

\[
\omega
\]

## 11. 最后总结

\(E_l\) 描述 landmark 位置变化会造成哪些残差变化。

\[
\operatorname{Col}(E_l)
\]

是 landmark 可以解释的残差方向。

\[
\mathcal{N}(E_l^\top)
\]

是和这些方向正交的部分，也就是 landmark 解释不了的残差方向。

\(E_l^\perp\) 是这个左零空间的一组基：

\[
(E_l^\perp)^\top E_l = 0
\]

\(Q\) 是投影矩阵：

\[
Q =
I - E_l(E_l^\top E_l)^{-1}E_l^\top
\]

它的作用是：

\[
Q\,\text{residual}
=
\text{residual 中无法被 landmark 解释的部分}
\]

所以论文这么做的目的就是：

\[
\text{消掉 landmark 变量，只保留真正约束相机/IMU 状态的视觉残差}
\]
