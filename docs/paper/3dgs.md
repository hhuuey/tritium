# 3DGS Rasterizer Backward 原理整理

> 本文整理了阅读 3D Gaussian Splatting (3DGS) Rasterizer 时关于 Backward 实现的一系列疑问与解答，重点解释：
>
> - 为什么 Backward 要重新做 overlap test？
> - 为什么不用保存每个 Pixel 的 Gaussian 链表？
> - Tile 排序与 Pixel 渲染顺序到底是什么关系？
> - Forward 与 Backward 是如何配合工作的？

---

# 一、Forward 到底保存了什么？

Forward 渲染过程中，Tile 内所有 Gaussian 会按照 **View Space Depth** 排序。

例如：

```text
Near

G1
G2
G3
G4
G5

Far
```

然后每个 Tile 使用这一份排序好的 Gaussian 数组进行 Rasterization。

Forward **真正保存的数据只有：**

对于每个 Pixel：

```text
final accumulated opacity（或者 final transmittance）
last contributor（最后一个真正参与 Blend 的 Gaussian）
```

而不会保存：

```text
Pixel A:

G1
G2
G4

Pixel B:

G2
G3
G5
```

这种可变长度链表。

---

# 二、为什么不能保存每个 Pixel 的 Gaussian 链表？

例如：

```text
Pixel0

G3
G5
G7

Pixel1

G2
G8

Pixel2

G1
G3
G4
G9
...
```

这样每个 Pixel 都有不同长度。

对于：

```
1920×1080 ≈ 2,000,000 Pixel
```

如果平均每个 Pixel 有：

```
30 个 Gaussian
```

需要保存：

```
2,000,000 × 30
≈ 6000 万个 Index
```

不仅显存巨大，而且 GPU 很不喜欢这种 Variable-length List。

所以论文明确说：

> 不保存 per-pixel blended list。

---

# 三、Tile 排序是不是所有 Pixel 都一样？

是。

整个 Tile 共用一份排序：

```text
Near

G1
G2
G3
G4
G5

Far
```

论文中的：

> No per-pixel ordering

就是说：

不会针对每个 Pixel 再重新排序。

---

# 四、那为什么不同 Pixel 最终 Blend 的 Gaussian 不一样？

因为：

**顺序一样。**

但是：

**参与 Blend 的 Gaussian 子集不同。**

例如：

Tile 排序：

```text
G1
G2
G3
G4
G5
```

Pixel A：

```
G1 ✓
G2 ✓
G3 ✗
G4 ✓
G5 ✗
```

真正 Blend：

```text
G1

↓

G2

↓

G4
```

Pixel B：

```
G1 ✗
G2 ✓
G3 ✓
G4 ✗
G5 ✓
```

真正 Blend：

```text
G2

↓

G3

↓

G5
```

所以：

**顺序共享。**

**参与者不同。**

---

# 五、为什么会出现这种情况？

因为每个 Gaussian 投影到屏幕后都是一个二维椭圆。

例如：

```
      ______

    /        \

   |   Pixel  |

    \________/
```

Pixel 在椭圆里面：

Forward：

```
Blend
```

Pixel 不在椭圆里面：

```
Skip
```

因此：

不同 Pixel：

会 Skip 不同 Gaussian。

---

# 六、Early Termination 也会导致不同

Forward：

```
G1

↓

G2

↓

G3

↓

G4
```

对于 Pixel A：

```
G1

↓

G2

↓

Opacity 已经饱和

↓

停止
```

对于 Pixel B：

```
G1 没覆盖

↓

继续

↓

G2

↓

G3

↓

G4
```

所以：

即使 Gaussian 都覆盖，

Early Termination 也可能不同。

---

# 七、为什么 Backward 要重新做 overlap test？

Forward 没保存：

```
Pixel

↓

G2

↓

G5

↓

G8
```

这条链。

Backward 如果不知道：

哪些 Gaussian 当初参与 Blend，

梯度就不知道应该传给谁。

因此：

Backward 必须重新判断：

```cpp
if(overlap(pixel))
```

恢复：

```
G2

↓

G5

↓

G8
```

但是：

这里恢复的是：

**参与关系。**

不是重新排序。

排序已经固定。

---

# 八、Backward 为什么不用保存链表？

这是最容易误解的一点。

很多人认为：

Backward：

既然恢复：

```
G2

↓

G5

↓

G8
```

是不是也要保存？

答案：

**不用。**

Backward 是：

```
读一个 Gaussian

↓

Overlap？

↓

Yes

↓

立即计算梯度

↓

丢弃

↓

继续下一个 Gaussian
```

整个过程：

不会产生：

```
[G2,G5,G8]
```

这样的数组。

这叫：

**Streaming（流式计算）**

而不是：

**Store（保存）**

---

# 九、Store 与 Streaming 的区别

## Forward 保存链表

需要：

```cpp
pixel_list[pixel] = {
    G2,
    G5,
    G8
}
```

直到 Backward。

显存一直占用。

---

## Backward Streaming

```text
G8

↓

Gradient

↓

丢弃

↓

G5

↓

Gradient

↓

丢弃

↓

G2

↓

Gradient

↓

结束
```

Gaussian 处理完立即释放。

不会保存。

这是：

GPU 最喜欢的模式。

---

# 十、Backward 真正怎么遍历？

Backward：

不是：

每个 Pixel：

自己：

扫描一遍 Tile。

而是：

整个 CUDA Block：

统一扫描。

例如：

Tile：

```
G5
G4
G3
G2
G1
```

Block：

统一：

```
加载 G5 到 Shared Memory

↓

所有 Pixel 一起处理 G5

↓

加载 G4

↓

所有 Pixel 一起处理 G4

↓

...
```

Gaussian：

只加载一次。

所有 Pixel 共用。

---

# 十一、last contributor 的作用是什么？

很多人理解成：

```
从这里开始遍历 Tile。
```

其实不是。

它真正作用是：

**告诉每个 Pixel：什么时候开始参与计算。**

例如：

P1：

```
last = G3
```

Backward：

```
G5

↓

Skip

↓

G4

↓

Skip

↓

G3

↓

开始计算

↓

G2

↓

计算

↓

G1

↓

计算
```

P2：

```
last = G5
```

Backward：

```
G5

↓

开始计算

↓

G4

↓

计算

↓

...
```

所以：

**Tile 一直统一遍历。**

只是：

不同 Pixel：

开始计算的位置不同。

---

# 十二、为什么论文说：

> each pixel will only start overlap testing...

意思就是：

例如：

P1：

```
last = G3
```

Backward：

```
G5

↓

Skip

↓

G4

↓

Skip

↓

G3

↓

开始做 overlap test

↓

恢复 T

↓

计算 Gradient
```

前面的 Gaussian：

连 overlap 都不用做。

直接 continue。

---

# 十三、为什么要反向遍历？

Forward：

```
Near

↓

Far
```

维护：

```
T_{i+1}=T_i(1-\alpha_i)
```

Backward：

需要恢复：

Forward 每一步的：

```
Transmittance
```

因此：

反向：

```
Far

↓

Near
```

利用：

```
T_i=T_{i+1}/(1-\alpha_i)
```

逐步恢复。

---

# 十四、Backward 为什么不用保存所有 T？

Forward：

实际上：

产生：

```
1

↓

0.6

↓

0.42

↓

0.084

↓

0.042
```

如果全部保存：

显存巨大。

论文：

只保存：

```
Final T
```

Backward：

反向恢复：

```
0.042

↓

0.084

↓

0.42

↓

0.6

↓

1
```

所以：

Forward：

保存很少。

Backward：

重新计算。

---

# 十五、Forward 与 Backward 的整体流程

```text
Forward

Tile 排序

↓

Near → Far

↓

Overlap

↓

Alpha Blend

↓

Early Stop

↓

保存：

- Final Transmittance
- Last Contributor



Backward

读取：

Final Transmittance

↓

Tile：

Far → Near

↓

每个 Pixel：

到达 Last Contributor 之前一直 Skip

↓

开始 Overlap Test

↓

恢复 Transmittance

↓

计算 Gradient

↓

继续恢复

↓

结束
```

---

# 十六、整个设计思想

3DGS Rasterizer 的核心思想就是：

> **Recompute instead of Store**

即：

**宁愿重新计算（Overlap、恢复 Transmittance），**

也不愿：

保存：

```
Pixel

↓

Gaussian

↓

Gaussian

↓

Gaussian
```

这种巨大的可变长度链表。

GPU：

计算很快。

显存很贵。

所以：

重新计算，

比：

保存中间状态，

更高效。

这也是整个 Rasterizer 最巧妙的工程优化之一。