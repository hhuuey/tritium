# EKF、IEKF 与流形 IEKF 更新公式推导

这份笔记从普通 EKF 的卡尔曼增益形式开始，逐步推导到普通向量空间中的 IEKF，再推广到流形状态上的 IEKF。核心目标是解释论文中这类公式为什么会出现：

$$
\hat{x}^{\kappa+1}=\hat{x}^{\kappa}\boxplus\left(-Kz^{\kappa}-(I-KH)(J^{\kappa})^{-1}(\hat{x}^{\kappa}\boxminus\hat{x})\right)
$$

这里最容易困惑的是后半项：

$$
-(I-KH)(J^{\kappa})^{-1}(\hat{x}^{\kappa}\boxminus\hat{x})
$$

它不是额外经验项，而是 IEKF 迭代线性化和流形误差坐标转换自然产生的项。

---

## 1. 普通 Kalman Filter / EKF 的增益形式

先从线性观测模型开始：

$$
z=Hx+n
$$

其中：

$$
n\sim\mathcal{N}(0,R)
$$

先验状态为：

$$
x\sim\mathcal{N}(\hat{x},P)
$$

这里：

- $\hat{x}$ 是预测状态，也就是先验均值；
- $P$ 是先验协方差；
- $z$ 是观测；
- $H$ 是观测矩阵；
- $R$ 是观测噪声协方差。

标准 Kalman gain 是：

$$
K=PH^T(HPH^T+R)^{-1}
$$

状态更新为：

$$
x^+=\hat{x}+K(z-H\hat{x})
$$

协方差更新常写成：

$$
P^+=(I-KH)P
$$

更数值稳定的 Joseph 形式是：

$$
P^+=(I-KH)P(I-KH)^T+KRK^T
$$

---

## 2. 从 MAP 角度推导 EKF 更新

线性高斯情况下，后验概率满足：

$$
p(x|z)\propto p(z|x)p(x)
$$

最大后验估计等价于最小化负对数后验：

$$
J(x)=\frac{1}{2}\|x-\hat{x}\|_P^2+\frac{1}{2}\|z-Hx\|_R^2
$$

其中加权范数定义为：

$$
\|a\|_M^2=a^TM^{-1}a
$$

展开目标函数：

$$
J(x)=\frac{1}{2}(x-\hat{x})^TP^{-1}(x-\hat{x})+\frac{1}{2}(z-Hx)^TR^{-1}(z-Hx)
$$

对 $x$ 求导并令其为零：

$$
P^{-1}(x-\hat{x})-H^TR^{-1}(z-Hx)=0
$$

整理得到信息形式：

$$
(P^{-1}+H^TR^{-1}H)x=P^{-1}\hat{x}+H^TR^{-1}z
$$

因此：

$$
x^+=(P^{-1}+H^TR^{-1}H)^{-1}(P^{-1}\hat{x}+H^TR^{-1}z)
$$

这个形式和 Kalman gain 形式等价。利用矩阵求逆引理可以得到：

$$
x^+=\hat{x}+K(z-H\hat{x})
$$

其中：

$$
K=PH^T(HPH^T+R)^{-1}
$$

下面把这个等价过程展开。

矩阵求逆引理，也叫 Woodbury identity，形式为：

$$
(A+UCV)^{-1}=A^{-1}-A^{-1}U(C^{-1}+VA^{-1}U)^{-1}VA^{-1}
$$

令：

$$
A=P^{-1}
$$

$$
U=H^T
$$

$$
C=R^{-1}
$$

$$
V=H
$$

则：

$$
A+UCV=P^{-1}+H^TR^{-1}H
$$

代入 Woodbury identity：

$$
(P^{-1}+H^TR^{-1}H)^{-1}=P-PH^T(R+HPH^T)^{-1}HP
$$

记：

$$
S=HPH^T+R
$$

于是：

$$
(P^{-1}+H^TR^{-1}H)^{-1}=P-PH^TS^{-1}HP
$$

又因为 Kalman gain 定义为：

$$
K=PH^TS^{-1}
$$

所以：

$$
(P^{-1}+H^TR^{-1}H)^{-1}=P-KHP
$$

也就是：

$$
(P^{-1}+H^TR^{-1}H)^{-1}=(I-KH)P
$$

这个式子说明，信息形式里的后验协方差为：

$$
P^+=(P^{-1}+H^TR^{-1}H)^{-1}=(I-KH)P
$$

接下来证明状态均值也等价。

从信息形式出发：

$$
x^+=P^+(P^{-1}\hat{x}+H^TR^{-1}z)
$$

为了得到 Kalman gain 形式，需要用另一个等价恒等式：

$$
P^+H^TR^{-1}=K
$$

证明如下。由前面已经得到：

$$
P^+=(I-KH)P
$$

所以：

$$
P^+H^TR^{-1}=(I-KH)PH^TR^{-1}
$$

只要证明右边等于 $K$ 即可。

由 Kalman gain 定义：

$$
K=PH^T(HPH^T+R)^{-1}
$$

两边右乘 $HPH^T+R$：

$$
K(HPH^T+R)=PH^T
$$

展开：

$$
KHPH^T+KR=PH^T
$$

移项：

$$
KR=PH^T-KHPH^T
$$

提取右侧公共因子 $PH^T$：

$$
KR=(I-KH)PH^T
$$

右乘 $R^{-1}$：

$$
K=(I-KH)PH^TR^{-1}
$$

因此：

$$
P^+H^TR^{-1}=K
$$

再看信息形式的状态解：

$$
x^+=P^+P^{-1}\hat{x}+P^+H^TR^{-1}z
$$

第二项由上式得到：

$$
P^+H^TR^{-1}z=Kz
$$

第一项可以用 $P^+=(I-KH)P$ 化简：

$$
P^+P^{-1}\hat{x}=(I-KH)\hat{x}
$$

所以：

$$
x^+=(I-KH)\hat{x}+Kz
$$

展开：

$$
x^+=\hat{x}-KH\hat{x}+Kz
$$

最终得到：

$$
x^+=\hat{x}+K(z-H\hat{x})
$$

这就说明，MAP 信息形式：

$$
x^+=(P^{-1}+H^TR^{-1}H)^{-1}(P^{-1}\hat{x}+H^TR^{-1}z)
$$

和 Kalman gain 形式：

$$
x^+=\hat{x}+K(z-H\hat{x})
$$

完全等价。

所以普通 Kalman 更新可以理解为：先验约束和观测约束的 MAP 解。

---

## 3. 非线性 EKF 的一次线性化

非线性观测模型为：

$$
z=h(x)+n
$$

在先验点 $\hat{x}$ 处线性化：

$$
h(x)\approx h(\hat{x})+H\delta x
$$

其中：

$$
\delta x=x-\hat{x}
$$

$$
H=\left.\frac{\partial h}{\partial x}\right|_{\hat{x}}
$$

观测残差为：

$$
r=z-h(\hat{x})
$$

于是线性化观测为：

$$
r\approx H\delta x+n
$$

套用线性 Kalman 更新：

$$
\delta x=Kr
$$

所以：

$$
x^+=\hat{x}+K(z-h(\hat{x}))
$$

这就是普通 EKF 的一次更新形式。

注意：普通 EKF 只在先验点 $\hat{x}$ 线性化一次，因此没有“当前迭代点相对先验点偏移”的额外项。

---

## 4. IEKF 为什么不同

IEKF，也就是 iterated EKF，不只线性化一次。它会重复：

1. 在当前迭代点 $x^{\kappa}$ 处重新计算残差；
2. 在当前迭代点 $x^{\kappa}$ 处重新计算雅可比；
3. 求一个增量；
4. 更新到下一轮 $x^{\kappa+1}$；
5. 直到收敛。

第一轮通常令：

$$
x^0=\hat{x}
$$

但第二轮以后：

$$
x^{\kappa}\neq\hat{x}
$$

这就是 IEKF 相比普通 EKF 多出一项的根本原因。

普通 EKF 的线性化点就是先验点，所以先验偏移为零。IEKF 的线性化点是当前迭代点 $x^{\kappa}$，但先验中心仍然是原来的 $\hat{x}$，所以必须考虑：

$$
x^{\kappa}-\hat{x}
$$

---

## 5. 普通向量空间中的 IEKF 推导

先不考虑流形，假设状态就是普通向量。

MAP 目标函数为：

$$
J(x)=\frac{1}{2}\|x-\hat{x}\|_P^2+\frac{1}{2}\|r(x)\|_R^2
$$

其中 $r(x)$ 是测量残差。为了和很多 LIO / VIO 论文一致，下面使用隐式残差形式：

$$
0=r(x)+n
$$

在第 $\kappa$ 次迭代，当前线性化点是：

$$
x^{\kappa}
$$

令：

$$
x=x^{\kappa}+\delta x
$$

残差线性化为：

$$
r(x^{\kappa}+\delta x)\approx z^{\kappa}+H\delta x
$$

其中：

$$
z^{\kappa}=r(x^{\kappa})
$$

先验项为：

$$
x-\hat{x}=x^{\kappa}+\delta x-\hat{x}
$$

也就是：

$$
x-\hat{x}=(x^{\kappa}-\hat{x})+\delta x
$$

所以第 $\kappa$ 次迭代要求解的是：

$$
\min_{\delta x}\frac{1}{2}\|(x^{\kappa}-\hat{x})+\delta x\|_P^2+\frac{1}{2}\|z^{\kappa}+H\delta x\|_R^2
$$

这一步是理解 IEKF 的关键。

普通 EKF 中 $x^{\kappa}=\hat{x}$，所以第一项变成：

$$
\|\delta x\|_P^2
$$

但 IEKF 后续迭代中 $x^{\kappa}\neq\hat{x}$，所以第一项必须保留：

$$
\|(x^{\kappa}-\hat{x})+\delta x\|_P^2
$$

---

## 6. 普通向量 IEKF 的正规方程

对目标函数求导。

先验项导数为：

$$
P^{-1}\left((x^{\kappa}-\hat{x})+\delta x\right)
$$

测量项导数为：

$$
H^TR^{-1}(z^{\kappa}+H\delta x)
$$

令总导数为零：

$$
P^{-1}\left((x^{\kappa}-\hat{x})+\delta x\right)+H^TR^{-1}(z^{\kappa}+H\delta x)=0
$$

整理关于 $\delta x$ 的项：

$$
(P^{-1}+H^TR^{-1}H)\delta x=-H^TR^{-1}z^{\kappa}-P^{-1}(x^{\kappa}-\hat{x})
$$

这是 IEKF 的信息形式 / 正规方程形式。

令：

$$
S=HPH^T+R
$$

$$
K=PH^TS^{-1}
$$

可以把上式改写成 Kalman gain 形式：

$$
\delta x=-Kz^{\kappa}-(I-KH)(x^{\kappa}-\hat{x})
$$

因此普通向量空间中的 IEKF 更新为：

$$
x^{\kappa+1}=x^{\kappa}+\delta x
$$

即：

$$
x^{\kappa+1}=x^{\kappa}-Kz^{\kappa}-(I-KH)(x^{\kappa}-\hat{x})
$$

这就是论文流形公式在欧式空间中的版本。

---

## 7. 为什么多出这一项

多出来的项是：

$$
-(I-KH)(x^{\kappa}-\hat{x})
$$

它来自先验项：

$$
\|x-\hat{x}\|_P^2
$$

IEKF 在 $x^{\kappa}$ 附近求增量，但是先验仍然约束 $x$ 不要离 $\hat{x}$ 太远。

如果当前迭代点已经偏离先验：

$$
x^{\kappa}-\hat{x}\neq0
$$

那么下一次更新时，先验项就必须把这段偏移考虑进去。

如果是普通 EKF 的第一轮：

$$
x^0=\hat{x}
$$

那么：

$$
x^0-\hat{x}=0
$$

多出来的项消失，IEKF 退化为普通 EKF：

$$
\delta x=-Kz^0
$$

如果残差定义为：

$$
z^0=h(\hat{x})-z
$$

那么：

$$
\delta x=K(z-h(\hat{x}))
$$

这就是熟悉的 EKF 更新。

---

## 8. 从普通向量推广到流形

在 VIO / LIO 中，状态通常包含姿态。姿态属于 $SO(3)$，不是普通欧式向量。

因此不能直接写：

$$
x+\delta x
$$

也不能直接写：

$$
x_1-x_2
$$

通常使用两个操作：

$$
x\boxplus\delta x
$$

$$
x_1\boxminus x_2
$$

其中：

- $\boxplus$ 表示在状态 $x$ 上施加误差状态增量；
- $\boxminus$ 表示两个流形状态之间的误差向量。

在流形 IEKF 中，第 $\kappa$ 次迭代写成：

$$
x=x^{\kappa}\boxplus\delta x
$$

先验误差为：

$$
x\boxminus\hat{x}
$$

代入当前迭代形式：

$$
(x^{\kappa}\boxplus\delta x)\boxminus\hat{x}
$$

这个东西一般不能简单写成：

$$
(x^{\kappa}\boxminus\hat{x})+\delta x
$$

因为流形上的误差坐标会随切空间变化。

---

## 9. 流形中 $J^{\kappa}$ 的定义

对先验误差做一阶线性化：

$$
(x^{\kappa}\boxplus\delta x)\boxminus\hat{x}\approx e^{\kappa}+J^{\kappa}\delta x
$$

其中：

$$
e^{\kappa}=x^{\kappa}\boxminus\hat{x}
$$

$J^{\kappa}$ 定义为：

$$
J^{\kappa}=\left.\frac{\partial\left((x^{\kappa}\boxplus\delta x)\boxminus\hat{x}\right)}{\partial\delta x}\right|_{\delta x=0}
$$

这就是 $J^{\kappa}$ 的来源。

它描述的是：在当前迭代点 $x^{\kappa}$ 的切空间中施加一个小扰动 $\delta x$，会如何改变相对先验 $\hat{x}$ 的误差。

如果状态是普通欧式向量，那么：

$$
J^{\kappa}=I
$$

---

## 10. 流形先验项如何变成论文形式

流形 MAP 先验项是：

$$
\|x\boxminus\hat{x}\|_{\hat{P}}^2
$$

代入线性化结果：

$$
x\boxminus\hat{x}\approx e^{\kappa}+J^{\kappa}\delta x
$$

所以先验项为：

$$
\|e^{\kappa}+J^{\kappa}\delta x\|_{\hat{P}}^2
$$

现在把 $J^{\kappa}$ 提出来。因为：

$$
e^{\kappa}=J^{\kappa}(J^{\kappa})^{-1}e^{\kappa}
$$

所以：

$$
e^{\kappa}+J^{\kappa}\delta x=J^{\kappa}\left(\delta x+(J^{\kappa})^{-1}e^{\kappa}\right)
$$

令：

$$
b=\delta x+(J^{\kappa})^{-1}e^{\kappa}
$$

则：

$$
e^{\kappa}+J^{\kappa}\delta x=J^{\kappa}b
$$

原先验代价为：

$$
(J^{\kappa}b)^T\hat{P}^{-1}(J^{\kappa}b)
$$

展开为：

$$
b^T(J^{\kappa})^T\hat{P}^{-1}J^{\kappa}b
$$

如果希望它写成标准加权范数：

$$
\|b\|_P^2=b^TP^{-1}b
$$

那么必须令：

$$
P^{-1}=(J^{\kappa})^T\hat{P}^{-1}J^{\kappa}
$$

等价地：

$$
P=(J^{\kappa})^{-1}\hat{P}(J^{\kappa})^{-T}
$$

于是：

$$
\|e^{\kappa}+J^{\kappa}\delta x\|_{\hat{P}}^2=\|\delta x+(J^{\kappa})^{-1}e^{\kappa}\|_P^2
$$

这就是论文中定义：

$$
P=(J^{\kappa})^{-1}\hat{P}(J^{\kappa})^{-T}
$$

的原因。

---

## 11. 流形 IEKF 的目标函数

测量残差在当前迭代点线性化为：

$$
r(x^{\kappa}\boxplus\delta x)\approx z^{\kappa}+H\delta x
$$

先验项已经转成当前扰动坐标：

$$
\delta x+(J^{\kappa})^{-1}e^{\kappa}
$$

因此第 $\kappa$ 次迭代求解：

$$
\min_{\delta x}\frac{1}{2}\|\delta x+(J^{\kappa})^{-1}e^{\kappa}\|_P^2+\frac{1}{2}\|z^{\kappa}+H\delta x\|_R^2
$$

其中：

$$
e^{\kappa}=x^{\kappa}\boxminus\hat{x}
$$

$$
P=(J^{\kappa})^{-1}\hat{P}(J^{\kappa})^{-T}
$$

---

## 12. 流形 IEKF 的 Kalman gain 更新

对上面的二次目标求导并令零，可得到和普通向量 IEKF 一样的结构，只是先验偏移从 $x^{\kappa}-\hat{x}$ 变成：

$$
(J^{\kappa})^{-1}(x^{\kappa}\boxminus\hat{x})
$$

Kalman gain 为：

$$
K=PH^T(HPH^T+R)^{-1}
$$

增量为：

$$
\delta x=-Kz^{\kappa}-(I-KH)(J^{\kappa})^{-1}(x^{\kappa}\boxminus\hat{x})
$$

状态更新为：

$$
x^{\kappa+1}=x^{\kappa}\boxplus\delta x
$$

合起来就是：

$$
x^{\kappa+1}=x^{\kappa}\boxplus\left(-Kz^{\kappa}-(I-KH)(J^{\kappa})^{-1}(x^{\kappa}\boxminus\hat{x})\right)
$$

这就是论文公式的来源。

---

## 13. 公式中各项的直观意义

测量残差修正项：

$$
-Kz^{\kappa}
$$

表示当前测量残差还没有归零，所以用 Kalman gain 把残差映射成状态修正。

先验偏移修正项：

$$
-(I-KH)(J^{\kappa})^{-1}(x^{\kappa}\boxminus\hat{x})
$$

表示当前迭代点已经偏离先验点，MAP 目标中的先验项要求状态不能无约束地离开先验。

流形转换项：

$$
(J^{\kappa})^{-1}
$$

表示把“当前迭代点相对先验点的误差”转换到当前迭代点的扰动坐标系中。

如果状态是普通向量，则：

$$
J^{\kappa}=I
$$

公式退化为普通向量 IEKF：

$$
x^{\kappa+1}=x^{\kappa}-Kz^{\kappa}-(I-KH)(x^{\kappa}-\hat{x})
$$

如果再令第一轮：

$$
x^0=\hat{x}
$$

则：

$$
x^0-\hat{x}=0
$$

公式退化为普通 EKF：

$$
x^+=\hat{x}-Kz^0
$$

如果残差定义为：

$$
z^0=h(\hat{x})-z
$$

则：

$$
x^+=\hat{x}+K(z-h(\hat{x}))
$$

---

## 14. 用一个简单表格总结

| 方法 | 线性化点 | 更新形式 | 是否有先验偏移项 |
|---|---|---|---|
| KF | 线性模型，不需要重新线性化 | $x^+=\hat{x}+K(z-H\hat{x})$ | 无 |
| EKF | 先验点 $\hat{x}$ | $x^+=\hat{x}+K(z-h(\hat{x}))$ | 无，因为 $x^0=\hat{x}$ |
| IEKF 普通向量 | 当前迭代点 $x^{\kappa}$ | $x^{\kappa+1}=x^{\kappa}-Kz^{\kappa}-(I-KH)(x^{\kappa}-\hat{x})$ | 有 |
| IEKF 流形 | 当前迭代点 $x^{\kappa}$ 的切空间 | $x^{\kappa+1}=x^{\kappa}\boxplus\delta x$ | 有，并且需要 $J^{\kappa}$ 转换 |

---

## 15. 和 FAST-LIO / LIO 中的关系

在 LIO 中，IMU propagation 提供先验：

$$
\hat{x}_k,\hat{P}_k
$$

LiDAR 点面残差或视觉光度残差提供测量约束：

$$
z_k^{\kappa},H
$$

IEKF 每一轮在当前状态估计 $\hat{x}_k^{\kappa}$ 上重新计算残差和雅可比，再用 Kalman gain 形式求误差状态增量。

论文中的公式本质上就是在解下面的 MAP 问题：

$$
\min_x\frac{1}{2}\|x\boxminus\hat{x}_k\|_{\hat{P}_k}^2+\frac{1}{2}\|r(x)\|_R^2
$$

因为状态包含旋转等流形成分，所以更新必须写成：

$$
x\leftarrow x\boxplus\delta x
$$

因为 IEKF 重复在线性化点 $x^{\kappa}$ 处迭代，所以先验项必须保留当前迭代点相对原始先验点的偏移：

$$
x^{\kappa}\boxminus\hat{x}
$$

因为该偏移和当前扰动 $\delta x$ 所在切空间不同，所以要通过：

$$
(J^{\kappa})^{-1}
$$

转换。

---

## 16. 最重要的一句话

普通 EKF 的公式：

$$
\delta x=K(z-h(\hat{x}))
$$

是 IEKF 在第一轮、线性化点等于先验点时的特例。

完整 IEKF 公式多出来的项：

$$
-(I-KH)(J^{\kappa})^{-1}(x^{\kappa}\boxminus\hat{x})
$$

来自两个事实：

1. IEKF 的当前线性化点 $x^{\kappa}$ 已经不等于先验点 $\hat{x}$；
2. 在流形上，当前扰动坐标和相对先验误差坐标之间需要 $J^{\kappa}$ 做转换。
