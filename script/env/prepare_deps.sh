#!/usr/bin/env bash
set -eo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
load_context "${1:-}" "${2:-}"
[[ $# == 2 ]] || fail "用法：prepare_deps.sh <项目> <环境>"
[[ "$project" != shell ]] || fail "shell 环境不需要项目源码依赖。"
[[ -z "${TRITIUM_PROJECT:-}" ]] || fail "请在宿主机运行依赖下载脚本。"
command -v jq >/dev/null 2>&1 || fail "缺少 jq，请先安装 jq。"
mkdir -p "$deps_root"

for name in "${dependencies[@]}"; do
  url="$(jq -er --arg name "$name" '.[$name].url' "$script_dir/deps.lock.json")"
  commit="$(jq -er --arg name "$name" '.[$name].commit' "$script_dir/deps.lock.json")"
  target="$deps_root/$name"
  if [[ ! -e "$target" ]]; then
    echo "下载 ${name}，版本：$commit"
    git init "$target"
    git -C "$target" remote add origin "$url"
    git -C "$target" fetch --depth 1 origin "$commit"
    git -C "$target" checkout --detach FETCH_HEAD
    git -C "$target" submodule update --init --recursive
  fi
  [[ "$(git -C "$target" remote get-url origin)" == "$url" ]] || fail "$target 的来源不符合锁定清单。"
  [[ "$(git -C "$target" rev-parse HEAD)" == "$commit" ]] || fail "$target 版本不同，请手动处理，脚本不会覆盖。"
  patch="$(jq -r --arg name "$name" '.[$name].patch // empty' "$script_dir/deps.lock.json")"
  if [[ -n "$patch" ]]; then
    patch="$script_dir/patches/$patch"
    [[ -f "$patch" ]] || fail "缺少依赖补丁：$patch"
    if git -C "$target" apply --reverse --check "$patch" 2>/dev/null; then
      echo "$name 所需补丁已存在。"
    else
      git -C "$target" apply --check "$patch"
      git -C "$target" apply "$patch"
    fi
  fi
  validate_dependency "$name" "$deps_root"
done
echo "第三方源码已准备，可在 $deps_root 查看和修改。"
