#include <fstream>
#include <unistd.h>
#include <sys/resource.h>


// 获取当前进程的虚拟内存大小
size_t getVirtualMemoryUsage() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        // 如果失败，返回0
        return 0;
    }
    return static_cast<size_t>(usage.ru_maxrss) * 1024; // ru_maxrss以1024字节为单位
}

// 获取当前进程的RSS（实际使用的物理内存）
size_t getResidentSetSize() {
    long rss;
    FILE* fp = nullptr;
    if ((fp = fopen("/proc/self/statm", "r")) == nullptr) {
        // 如果无法读取，返回0
        return 0;
    }
    if (fscanf(fp, "%*s%ld", &rss) != 1) {
        fclose(fp);
        // 如果读取失败，返回0
        return 0;
    }
    fclose(fp);
    // statm中的数据是以页为单位的，需要乘以系统页大小
    return static_cast<size_t>(rss) * sysconf(_SC_PAGE_SIZE);
}