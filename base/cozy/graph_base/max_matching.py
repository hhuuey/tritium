from typing import Sequence

# Berge's Lemma：如果不存在增广路径，当前匹配就是最大匹配

# #include <bits/stdc++.h>
# using namespace std;

# const int MAXN = 1005;

# vector<int> g[MAXN]; // 邻接表：左边 -> 右边
# int matchR[MAXN]; // 右边每个点匹配到的左点（-1 表示未匹配）
# bool vis[MAXN]; // 访问标记（防止重复搜索）

# // dfs(u): 尝试给左节点 u 找匹配
# bool dfs(int u) {
# for (int v : g[u]) { // 遍历 u 能连接的所有右节点
# if (vis[v]) continue; // 如果这个点已经尝试过，跳过
# vis[v] = true;

# // 情况1：v 还没有被匹配
# // 情况2：v 已匹配，但原来的匹配对象可以换位置
# if (matchR[v] == -1 || dfs(matchR[v])) {
# 	matchR[v] = u; // 把 v 分配给 u
# 	return true; // 匹配成功
# }
# }
# return false; // 无法匹配
# }

# // 主函数：求最大匹配
# int maxMatching(int n) {
# int res = 0;
# memset(matchR, -1, sizeof(matchR)); // 初始化右边全部未匹配

# for (int u = 0; u < n; u++) { // 枚举每个左节点
# 	memset(vis, false, sizeof(vis)); // 每次 DFS 前清空访问标记
# 	if (dfs(u)) res++; // 如果成功匹配，计数+1
# }

# return res;
# }

# vis 标记保证了 DFS 是一棵树而不是有环图


def _infer_right_size(graph: Sequence[Sequence[int]]) -> int:
    max_right_index = -1
    for neighbors in graph:
        for right_index in neighbors:
            if right_index > max_right_index:
                max_right_index = right_index
    return max_right_index + 1


def _dfs(
    u: int,
    graph: Sequence[Sequence[int]],
    match_right: list[int],
    vis: list[bool],
) -> bool:
    for v in graph[u]:
        if vis[v]:
            continue

        vis[v] = True
        if match_right[v] == -1 or _dfs(match_right[v], graph, match_right, vis):
            match_right[v] = u
            return True

    return False


def max_matching(
    graph: Sequence[Sequence[int]],
    right_size: int | None = None,
) -> tuple[int, list[int]]:
    if right_size is None:
        right_size = _infer_right_size(graph)

    match_right = [-1] * right_size
    res = 0
    for u in range(len(graph)):
        vis = [False] * right_size
        if _dfs(u, graph, match_right, vis):
            res += 1

    return res, match_right


__all__ = ['max_matching']
