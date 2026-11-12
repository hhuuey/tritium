#!/usr/bin/env python
"""
统一构建脚本 - 支持构建独立包
"""

import subprocess
import sys
import os
import shutil
from pathlib import Path


def run_command(cmd, description="", cwd=None):
    """运行命令并输出结果"""
    if description:
        print(f"\n{'='*60}")
        print(f"  {description}")
        print(f"{'='*60}")
    print(f"$ {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    return result.returncode == 0


def discover_packages():
    """自动发现包含 pyproject.toml 的包"""
    packages = {}
    base_dir = Path(__file__).parent.parent

    # 扫描的目录列表
    scan_dirs = [
        base_dir / "cozy",  # 基础库
        base_dir / "project",  # 项目目录
    ]

    for scan_dir in scan_dirs:
        if not scan_dir.exists():
            continue

        # 检查当前目录
        if (scan_dir / "pyproject.toml").exists():
            package_name = scan_dir.name
            rel_path = os.path.relpath(scan_dir, Path(__file__).parent)
            packages[package_name] = rel_path

        # 检查子目录（只检查一级）
        if scan_dir.is_dir():
            for subdir in scan_dir.iterdir():
                if subdir.is_dir() and (subdir / "pyproject.toml").exists():
                    package_name = subdir.name
                    rel_path = os.path.relpath(subdir, Path(__file__).parent)
                    packages[package_name] = rel_path

    return packages


# 动态发现包配置：名称 -> 源目录路径
PACKAGES = discover_packages()


def list_packages():
    """列出所有可用的包"""
    print("\n可用包列表：")
    print("\n[基础库]")
    print("  1. cozy - 基础库（其他包依赖）")
    print("\n[项目包]")
    idx = 2
    for name, path in PACKAGES.items():
        if name != "cozy":
            print(f"  {idx}. {name}")
            idx += 1
    print(f"\n总计: {len(PACKAGES)} 个包")


def build_package(package_name):
    """构建指定包"""
    if package_name not in PACKAGES:
        print(f"❌ 未知包: {package_name}")
        print(f"可用包: {', '.join(PACKAGES.keys())}")
        return False

    package_dir = Path(__file__).parent / PACKAGES[package_name]

    if not package_dir.exists():
        print(f"❌ 包目录不存在: {package_dir}")
        return False

    if not (package_dir / "pyproject.toml").exists():
        print(f"❌ 缺少 pyproject.toml: {package_dir}")
        return False

    print(f"\n{'='*60}")
    print(f"  构建包: {package_name}")
    print(f"  路径: {package_dir}")
    print(f"{'='*60}")

    # 清理旧文件
    for d in ["dist", "build", "*.egg-info"]:
        for item in package_dir.glob(d):
            if item.is_dir():
                shutil.rmtree(item)

    # 构建
    success = run_command([sys.executable, "-m", "build"], f"构建 tritium-{package_name}", cwd=package_dir)

    if success:
        print(f"\n✓ 构建成功: {package_name}")
        dist_dir = package_dir / "dist"
        if dist_dir.exists():
            print("\n  生成的文件：")
            for f in dist_dir.glob("*"):
                size = f.stat().st_size / (1024 * 1024)
                print(f"    - {f.name} ({size:.2f} MB)")

    return success


def build_all():
    """构建所有包"""
    print("\n构建所有包...")
    results = {}

    # 先构建 cozy（其他包依赖它）
    print("\n[1/4] 构建基础库...")
    results["cozy"] = build_package("cozy")

    # 构建其他包
    other_packages = [k for k in PACKAGES.keys() if k != "cozy"]
    for idx, pkg in enumerate(other_packages, 2):
        print(f"\n[{idx}/{len(PACKAGES)}] 构建 {pkg}...")
        results[pkg] = build_package(pkg)

    # 汇总
    print("\n" + "=" * 60)
    print("构建汇总")
    print("=" * 60)
    for pkg, success in results.items():
        status = "✓ 成功" if success else "✗ 失败"
        print(f"  {pkg:20s} {status}")

    # 构建后自动清理
    clean_all()

    return all(results.values())


def install_package(package_name):
    """安装指定包到本地（开发模式）"""
    if package_name not in PACKAGES:
        print(f"❌ 未知包: {package_name}")
        return False

    package_dir = Path(__file__).parent / PACKAGES[package_name]

    print(f"\n安装包: {package_name} (开发模式)")
    success = run_command([sys.executable, "-m", "pip", "install", "-e", "."],
                          f"安装 tritium-{package_name}",
                          cwd=package_dir)

    if success:
        print(f"\n✓ 安装成功: {package_name}")

    return success


def install_all():
    """安装所有包（开发模式）"""
    print("\n安装所有包（开发模式）...")

    # 先安装 cozy
    if not install_package("cozy"):
        print("\n❌ cozy 安装失败，停止")
        return False

    # 安装其他包
    for pkg in PACKAGES.keys():
        if pkg != "cozy":
            install_package(pkg)

    print("\n✓ 所有包安装完成")

    # 安装后自动清理
    clean_all()

    return True


def clean_all():
    """清理所有构建文件"""
    print("\n清理所有构建文件...")

    for package_name, rel_path in PACKAGES.items():
        package_dir = Path(__file__).parent / rel_path
        if not package_dir.exists():
            continue

        print(f"\n清理: {package_name}")
        for pattern in ["dist", "build", "*.egg-info", "__pycache__"]:
            for item in package_dir.rglob(pattern):
                if item.is_dir():
                    print(f"  删除: {item.relative_to(package_dir)}")
                    shutil.rmtree(item)

    # 清理集中的 pycache 目录（VSCode 调试配置生成）
    pycache_all = Path(__file__).parent.parent / ".pycache_all"
    if pycache_all.exists():
        print(f"\n清理集中缓存: .pycache_all")
        shutil.rmtree(pycache_all)

    # 清理 QGIS 生成的样式数据库
    qgis_db = Path(__file__).parent.parent / "symbology-style.db"
    if qgis_db.exists():
        print(f"\n清理 QGIS 样式数据库: symbology-style.db")
        qgis_db.unlink()

    print("\n✓ 清理完成")


def main():
    """主函数"""

    print("""
    ╔════════════════════════════════════════════════════════════╗
    ║         Tritium Multi-Package Build Tool                   ║
    ║         多包独立构建系统                                     ║
    ╚════════════════════════════════════════════════════════════╝
    """)

    if len(sys.argv) < 2:
        print_usage()
        return True

    command = sys.argv[1].lower()

    if command == "list":
        list_packages()
        return True

    elif command == "build":
        if len(sys.argv) > 2:
            package_name = sys.argv[2]
            return build_package(package_name)
        else:
            return build_all()

    elif command == "install":
        if len(sys.argv) > 2:
            package_name = sys.argv[2]
            return install_package(package_name)
        else:
            return install_all()

    elif command == "clean":
        clean_all()
        return True

    elif command == "publish":
        if len(sys.argv) < 3:
            print("❌ 请指定要发布的包")
            print("   python build.py publish <package_name>")
            return False

        package_name = sys.argv[2]
        package_dir = Path(__file__).parent / PACKAGES.get(package_name, "")

        if not package_dir.exists():
            print(f"❌ 包不存在: {package_name}")
            return False

        print(f"\n发布包: {package_name}")
        print("⚠️  确保已配置 PyPI 认证信息在 ~/.pypirc")

        success = run_command([sys.executable, "-m", "twine", "upload", "dist/*"],
                              f"上传 tritium-{package_name} 到 PyPI",
                              cwd=package_dir)

        # 发布后自动清理
        if success:
            clean_all()

        return success

        # 发布后自动清理
        if success:
            clean_all()

        return success

    else:
        print(f"❌ 未知命令: {command}")
        print_usage()
        return False


def print_usage():
    """打印使用说明"""
    print("""
    使用方法：

    python build.py <command> [package]

    可用命令：

    list                    - 列出所有可用包

    build [package]         - 构建包
                              不指定包：构建所有包
                              指定包：构建单个包
                              示例：
                                python build.py build              # 全部
                                python build.py build cozy         # 只构建基础库
                                python build.py build road-structure

    install [package]       - 安装包（开发模式）
                              不指定包：安装所有包
                              指定包：安装单个包
                              示例：
                                python build.py install            # 全部
                                python build.py install cozy

    clean                   - 清理所有构建文件

    publish <package>       - 发布包到 PyPI
                              示例：
                                python build.py publish cozy
                                python build.py publish road-structure


    快速开始：

    1. 查看所有包
       python build.py list

    2. 开发模式安装（推荐）
       python build.py install              # 安装所有包
       python build.py install cozy         # 只安装 cozy

    3. 构建发布包
       python build.py build cozy           # 构建基础库
       python build.py build road-structure # 构建项目包

    4. 发布到 PyPI
       python build.py publish cozy
       python build.py publish road-structure


    包依赖关系：

    tritium-cozy (基础库)
      ├── tritium-road-structure
      ├── tritium-traffic-light
      └── tritium-chunjian-vis

    用户安装示例：
      pip install tritium-cozy              # 只装基础库
      pip install tritium-road-structure    # 自动安装 cozy + road-structure
    """)


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
