#!/usr/bin/env bash
set -euo pipefail

SOURCE_USER="huanghhdw"
TARGET_USER="hhuuey"

# 环境变量中的 Token 会覆盖 gh 的账号切换，避免操作错账号。
if [[ -n "${GH_TOKEN:-}" || -n "${GITHUB_TOKEN:-}" ]]; then
    echo "请先 unset GH_TOKEN GITHUB_TOKEN，再运行脚本。" >&2
    exit 1
fi

if [[ "$SOURCE_USER" == "$TARGET_USER" ]]; then
    echo "来源账号与目标账号不能相同。" >&2
    exit 1
fi

STAR_LIST="$(mktemp)"
trap 'rm -f "$STAR_LIST"' EXIT

echo "正在读取来源账号：$SOURCE_USER"
gh auth switch --hostname github.com --user "$SOURCE_USER"
gh api --hostname github.com --paginate \
    'user/starred?per_page=100' \
    --jq '.[].full_name' > "$STAR_LIST"

TOTAL="$(wc -l < "$STAR_LIST" | tr -d '[:space:]')"
echo "共读取到 $TOTAL 个项目，准备同步到：$TARGET_USER"

gh auth switch --hostname github.com --user "$TARGET_USER"

COUNT=0
SUCCESS=0
FAILED=()
while IFS= read -r REPO; do
    COUNT=$((COUNT + 1))
    echo "[$COUNT/$TOTAL] Star $REPO"

    if gh api --hostname github.com --method PUT \
        "user/starred/$REPO" --silent; then
        SUCCESS=$((SUCCESS + 1))
    else
        echo "同步失败：${REPO}。已记录，将继续处理后续项目。" >&2
        FAILED+=("$REPO")
    fi

    sleep 2
done < "$STAR_LIST"

echo "处理结束：共 ${COUNT} 项，成功 ${SUCCESS} 项，失败 ${#FAILED[@]} 项。"
if [[ ${#FAILED[@]} -gt 0 ]]; then
    printf '失败项目：%s\n' "${FAILED[@]}" >&2
    exit 1
fi