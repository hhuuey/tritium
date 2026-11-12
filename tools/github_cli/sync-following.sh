#!/usr/bin/env bash
set -euo pipefail

SOURCE_USER="huanghhdw"
TARGET_USER="hhuuey"

if [[ -n "${GH_TOKEN:-}" || -n "${GITHUB_TOKEN:-}" ]]; then
    echo "请先 unset GH_TOKEN GITHUB_TOKEN，再运行脚本。" >&2
    exit 1
fi

if [[ "$SOURCE_USER" == "$TARGET_USER" ]]; then
    echo "来源账号与目标账号不能相同。" >&2
    exit 1
fi

FOLLOW_LIST="$(mktemp)"
trap 'rm -f "$FOLLOW_LIST"' EXIT

echo "正在读取来源账号的关注列表：$SOURCE_USER"
gh auth switch --hostname github.com --user "$SOURCE_USER"
gh api --hostname github.com --paginate \
    'user/following?per_page=100' \
    --jq '.[].login' > "$FOLLOW_LIST"

TOTAL="$(wc -l < "$FOLLOW_LIST" | tr -d '[:space:]')"
echo "共读取到 $TOTAL 个账号，准备同步到：$TARGET_USER"

gh auth switch --hostname github.com --user "$TARGET_USER"

# 使用实际登录名判断自身，避免用户名大小写或更名导致判断失效。
TARGET_LOGIN="$(gh api --hostname github.com user --jq '.login')"

COUNT=0
SUCCESS=0
SKIPPED=0
FAILED=()

while IFS= read -r LOGIN; do
    COUNT=$((COUNT + 1))

    if [[ "$LOGIN" == "$TARGET_LOGIN" ]]; then
        echo "[$COUNT/$TOTAL] 跳过目标账号自身：$LOGIN"
        SKIPPED=$((SKIPPED + 1))
        continue
    fi

    echo "[$COUNT/$TOTAL] Follow $LOGIN"

    if gh api --hostname github.com --method PUT \
        "user/following/$LOGIN" \
        --header 'Content-Length: 0' --silent; then
        SUCCESS=$((SUCCESS + 1))
    else
        echo "关注失败：${LOGIN}。已记录，将继续处理后续账号。" >&2
        FAILED+=("$LOGIN")
    fi

    sleep 2
done < "$FOLLOW_LIST"

echo "处理结束：共 ${COUNT} 项，成功 ${SUCCESS} 项，失败 ${#FAILED[@]} 项，跳过 ${SKIPPED} 项。"
if [[ ${#FAILED[@]} -gt 0 ]]; then
    printf '失败账号：%s\n' "${FAILED[@]}" >&2
    exit 1
fi