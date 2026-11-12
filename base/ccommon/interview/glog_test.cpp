#include "glog/logging.h"

int main(int argc, char **argv)
{
    google::InitGoogleLogging(argv[0]);
    google::SetLogDestination(google::GLOG_INFO, "c:\\Logs\\INFO_");
    google::SetStderrLogging(google::GLOG_INFO);
    google::SetLogFilenameExtension("log_");
    FLAGS_colorlogtostderr = true;  // Set log color
    FLAGS_logbufsecs = 0;  // Set log output speed(s)
    FLAGS_max_log_size = 1024;  // Set max log file size
    FLAGS_stop_logging_if_full_disk = true;  // If disk is full
    char str[20] = "hello log!";
    LOG(INFO) << str;
    std::string cStr = "hello google!";
    LOG(INFO) << cStr;
    LOG(INFO) << "info test" << "hello log!";  //输出一个Info日志
    LOG(WARNING) << "warning test";  //输出一个Warning日志
    LOG(ERROR) << "error test";  //输出一个Error日志
    google::ShutdownGoogleLogging();
}