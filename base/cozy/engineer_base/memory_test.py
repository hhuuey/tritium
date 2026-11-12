import os
import psutil

# 获取当前进程ID
process = psutil.Process(os.getpid())

# 获取当前进程的内存使用（resident set size）
memory_info = process.memory_info()
memory_usage = memory_info.rss / 1024 / 1024  # 转换为MB

print(f"当前进程的内存使用：{memory_usage:.2f} MB")


