# GitHub 账号同步工具

将来源账号的 Star 和 Following（关注的人）追加到目标账号，不取消目标账号已有的收藏或关注。

## 1. 安装

macOS：

```bash
brew install gh
```

## 2. 登录与授权

执行两次登录，浏览器中分别授权来源账号和目标账号：

```bash
gh auth login --hostname github.com --web
gh auth login --hostname github.com --web
gh auth status --hostname github.com
```

切换到目标账号并补充关注权限（将 `hhuuey` 替换为自己的目标账号）：

```bash
gh auth switch --hostname github.com --user hhuuey
gh auth refresh --hostname github.com --scopes user:follow
```

## 3. 执行脚本

修改 [sync-stars.sh](./sync-stars.sh) 和 [sync-following.sh](./sync-following.sh) 中的
`SOURCE_USER`、`TARGET_USER`，然后在仓库根目录执行：

```bash
unset GH_TOKEN GITHUB_TOKEN
bash tools/github-cli/sync-stars.sh
bash tools/github-cli/sync-following.sh
```

单项失败会继续，结束时显示失败名单；可重新运行，已有 Star 或关注不会重复添加。
有失败项时退出码为 `1`，两个脚本不要使用 `&&` 连接，以免跳过后一个脚本。
列表读取或账号切换失败仍会停止；私有仓库需要目标账号及其登录凭据具备访问权限。
遇到限流请按错误提示等待后再运行。执行后当前登录账号保留为目标账号。