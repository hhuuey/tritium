#!/usr/bin/env bash
set -euo pipefail

# 双向同步 tritium 目录：
#  - 上传到服务器: ./sync_tritium.sh push
#  - 从服务器下载: ./sync_tritium.sh pull
# 需要安装
#  - 本机: ssh, rsync, sshpass (如果使用密码)
# brew install rsync hudochenkov/sshpass/sshpass
#  - 服务器: ssh, rsync

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_CONFIG_FILE="${SCRIPT_DIR}/sync_tritium.conf"
CONFIG_FILE=""

DIRECTION=""
SUBDIRS=()
USE_SSHPASS=0

usage() {
  cat <<'EOF'
用法:
  sync.sh [-c config_file] <push|pull> [path1 path2 ...]

说明:
  push: 本机路径 -> 服务器路径
  pull: 服务器路径 -> 本机路径
  path 参数可传目录或文件相对路径
  -c: 指定配置文件路径（默认: 脚本同目录 sync_tritium.conf）

密码:
  REMOTE_PASSWORD 为空时，使用 SSH key 登录

示例:
  sync.sh push
  sync.sh -c script/sync_tritium.dev.conf push
  sync.sh pull project script
  sync.sh push cozy project
  sync.sh push project/road_edge_clean/config.py
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    -c|--config)
      if [[ $# -lt 2 ]]; then
        echo "参数错误: -c|--config 需要配置文件路径"
        usage
        exit 1
      fi
      CONFIG_FILE="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    push|pull)
      DIRECTION="$1"
      shift
      break
      ;;
    *)
      echo "参数错误: 未知参数 $1"
      usage
      exit 1
      ;;
  esac
done

if [[ -z "$DIRECTION" ]]; then
  echo "方向必须是 push 或 pull"
  usage
  exit 1
fi

if [[ -z "$CONFIG_FILE" ]]; then
  CONFIG_FILE="$DEFAULT_CONFIG_FILE"
fi

if [[ ! -f "$CONFIG_FILE" ]]; then
  echo "配置文件不存在: $CONFIG_FILE"
  echo "请参考默认模板创建配置文件: ${SCRIPT_DIR}/sync_tritium.conf"
  exit 1
fi

# shellcheck disable=SC1090
source "$CONFIG_FILE"

: "${LOCAL_PATH:?配置缺失: LOCAL_PATH}"
: "${REMOTE_PATH:?配置缺失: REMOTE_PATH}"
: "${REMOTE_ALIAS:?配置缺失: REMOTE_ALIAS}"
: "${REMOTE_USER:=}"
: "${REMOTE_HOST:=}"
: "${REMOTE_PORT:=22}"
: "${REMOTE_PASSWORD:=}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --)
      shift
      while [[ $# -gt 0 ]]; do
        SUBDIRS+=("$1")
        shift
      done
      ;;
    *)
      SUBDIRS+=("$1")
      shift
      ;;
  esac
done

if [[ -n "$REMOTE_PASSWORD" ]]; then
  SSHPASS="$REMOTE_PASSWORD"
  export SSHPASS
  USE_SSHPASS=1
fi

# 优先走 ssh 配置别名；如果不可用则使用直连参数
if ssh -G "$REMOTE_ALIAS" >/dev/null 2>&1; then
  REMOTE_LOGIN="$REMOTE_ALIAS"
  REMOTE_USE_ALIAS=1
  RSYNC_SSH_CMD="ssh"
else
  if [[ -z "$REMOTE_USER" || -z "$REMOTE_HOST" ]]; then
    echo "配置缺失: 未命中 SSH 别名时需要 REMOTE_USER 和 REMOTE_HOST"
    exit 1
  fi
  REMOTE_LOGIN="${REMOTE_USER}@${REMOTE_HOST}"
  REMOTE_USE_ALIAS=0
  RSYNC_SSH_CMD="ssh -p ${REMOTE_PORT}"
fi

ssh_remote() {
  if [[ "$USE_SSHPASS" -eq 1 ]]; then
    if [[ "$REMOTE_USE_ALIAS" -eq 1 ]]; then
      sshpass -e ssh "$REMOTE_LOGIN" "$@"
    else
      sshpass -e ssh -p "$REMOTE_PORT" "$REMOTE_LOGIN" "$@"
    fi
  else
    if [[ "$REMOTE_USE_ALIAS" -eq 1 ]]; then
      ssh "$REMOTE_LOGIN" "$@"
    else
      ssh -p "$REMOTE_PORT" "$REMOTE_LOGIN" "$@"
    fi
  fi
}

if ! command -v ssh >/dev/null 2>&1; then
  echo "本机缺少 ssh，请先安装 OpenSSH 客户端"
  exit 1
fi

if [[ "$USE_SSHPASS" -eq 1 ]] && ! command -v sshpass >/dev/null 2>&1; then
  echo "本机缺少 sshpass，请先安装 sshpass"
  exit 1
fi

if ! command -v rsync >/dev/null 2>&1; then
  echo "本机缺少 rsync，请先安装 rsync"
  exit 1
fi

if [[ "$USE_SSHPASS" -eq 1 ]]; then
  if [[ "$REMOTE_USE_ALIAS" -eq 1 ]]; then
    RSYNC_SSH_CMD="sshpass -e ssh"
  else
    RSYNC_SSH_CMD="sshpass -e ssh -p ${REMOTE_PORT}"
  fi
fi

if ssh_remote "command -v rsync >/dev/null 2>&1"; then
  :
else
  echo "远端缺少 rsync，请先在服务器安装 rsync"
  exit 1
fi

normalize_subdir() {
  local raw="$1"
  local norm="${raw#./}"
  norm="${norm#/}"
  norm="${norm%/}"

  if [[ -z "$norm" ]]; then
    echo ""
    return
  fi

  if [[ "$norm" == *".."* ]]; then
    echo ""
    return
  fi

  echo "$norm"
}

sync_one_subdir() {
  local rel="$1"

  if [[ "$DIRECTION" == "push" ]]; then
    if [[ -z "$rel" ]]; then
      echo "[INFO] 本机 -> 服务器 (rsync, 全量)"
      ssh_remote "mkdir -p '${REMOTE_PATH}'"
      rsync -avz --progress -e "$RSYNC_SSH_CMD" "${LOCAL_PATH}/" "${REMOTE_LOGIN}:${REMOTE_PATH}/"
    else
      if [[ -d "${LOCAL_PATH}/${rel}" ]]; then
        echo "[INFO] 本机 -> 服务器 (rsync, 目录: ${rel})"
        ssh_remote "mkdir -p '${REMOTE_PATH}/${rel}'"
        rsync -avz --progress -e "$RSYNC_SSH_CMD" "${LOCAL_PATH}/${rel}/" "${REMOTE_LOGIN}:${REMOTE_PATH}/${rel}/"
      elif [[ -f "${LOCAL_PATH}/${rel}" ]]; then
        echo "[INFO] 本机 -> 服务器 (rsync, 文件: ${rel})"
        local rel_parent
        rel_parent="$(dirname "$rel")"
        if [[ "$rel_parent" != "." ]]; then
          ssh_remote "mkdir -p '${REMOTE_PATH}/${rel_parent}'"
        else
          ssh_remote "mkdir -p '${REMOTE_PATH}'"
        fi
        rsync -avz --progress -e "$RSYNC_SSH_CMD" "${LOCAL_PATH}/${rel}" "${REMOTE_LOGIN}:${REMOTE_PATH}/${rel}"
      else
        echo "本机路径不存在: ${LOCAL_PATH}/${rel}"
        exit 1
      fi
    fi
  else
    if [[ -z "$rel" ]]; then
      echo "[INFO] 服务器 -> 本机 (rsync, 全量)"
      mkdir -p "${LOCAL_PATH}"
      rsync -avz --progress -e "$RSYNC_SSH_CMD" "${REMOTE_LOGIN}:${REMOTE_PATH}/" "${LOCAL_PATH}/"
    else
      local remote_kind
      remote_kind="$(ssh_remote "if [ -d '${REMOTE_PATH}/${rel}' ]; then echo dir; elif [ -f '${REMOTE_PATH}/${rel}' ]; then echo file; else echo missing; fi")"

      if [[ "$remote_kind" == "dir" ]]; then
        echo "[INFO] 服务器 -> 本机 (rsync, 目录: ${rel})"
        mkdir -p "${LOCAL_PATH}/${rel}"
        rsync -avz --progress -e "$RSYNC_SSH_CMD" "${REMOTE_LOGIN}:${REMOTE_PATH}/${rel}/" "${LOCAL_PATH}/${rel}/"
      elif [[ "$remote_kind" == "file" ]]; then
        echo "[INFO] 服务器 -> 本机 (rsync, 文件: ${rel})"
        local rel_parent
        rel_parent="$(dirname "$rel")"
        if [[ "$rel_parent" != "." ]]; then
          mkdir -p "${LOCAL_PATH}/${rel_parent}"
        else
          mkdir -p "${LOCAL_PATH}"
        fi
        rsync -avz --progress -e "$RSYNC_SSH_CMD" "${REMOTE_LOGIN}:${REMOTE_PATH}/${rel}" "${LOCAL_PATH}/${rel}"
      else
        echo "服务器路径不存在: ${REMOTE_PATH}/${rel}"
        exit 1
      fi
    fi
  fi
}

if [[ ${#SUBDIRS[@]} -eq 0 ]]; then
  sync_one_subdir ""
else
  for sub in "${SUBDIRS[@]}"; do
    normalized="$(normalize_subdir "$sub")"
    if [[ -z "$normalized" ]]; then
      echo "非法子目录参数: $sub"
      exit 1
    fi
    sync_one_subdir "$normalized"
  done
fi

echo "[DONE] 同步完成"
