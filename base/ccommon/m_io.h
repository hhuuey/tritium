#ifndef TRITUM_M_IO_H
#define TRITUM_M_IO_H

#include <filesystem>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>


namespace tritium {

namespace fs = std::filesystem;

std::vector<std::string> get_files(const std::string &dir, bool sort = false) {
    std::vector<std::string> files;
    for (const auto &entry : fs::directory_iterator(dir)) {
        files.emplace_back(entry.path().generic_string());
    }
    if (sort) {
        std::sort(files.begin(), files.end());
    }
    return files;
}

void create_dirs(const std::string &dir) {
    bool exist = fs::exists(dir);
    if (!exist) {
        fs::create_directories(dir);
    }
}

void remove_dirs(const std::string &dir) {
    bool exist = fs::exists(dir);
    if (exist) {
        fs::remove_all(dir);
    }
}

template<class T>
void create_txt(const std::string &file, const std::vector<std::vector<T>> &datas, const char delimiter = ' ',
               bool override = true) {
    //! create directory
    auto dir_pos = file.rfind('/');
    if (dir_pos != std::string::npos) {
        create_dirs(file.substr(0, dir_pos));
    }

    //! Write file
    auto strategy = override ? (std::ios::out | std::ios::trunc) : (std::ios::out | std::ios::app);
    std::fstream f(file, strategy);
    if (!f.is_open()) {
        std::cout << "not open" << std::endl;
        return;
    }

    for (const auto &line: datas) {
        for (int i = 0; i < line.size(); ++i) {
            if (i != line.size() - 1) {
                f << line[i] << delimiter;
            }
            else {
                f << line[i] << "\n";
            }
        }
    }
    f.close();
}


template <class T>
void string_to_T(const std::string src, T &dst)
{
    return;
}

template <>
void string_to_T(const std::string src, std::string &dst)
{
    dst = src;
}

template <>
void string_to_T(const std::string src, double &dst)
{
    dst = std::stod(src);
}

template <>
void string_to_T(const std::string src, int &dst)
{
    dst = std::stoi(src);
}

template<class T>
bool read_txt(const std::string &file, std::vector<std::vector<T>> &datas, const char delimiter = ' ') {
    if (!fs::exists(file)) return false;

    std::ifstream f(file);
    std::string line, word;
    while (getline(f, line)) {
        std::stringstream ss(line);
        std::string str_data;
        std::vector<T> line_data;
        while (getline(ss, str_data, delimiter)) {
            T data;
            string_to_T(str_data, data);
            line_data.emplace_back(data);
        }
        datas.emplace_back(line_data);
    }
    return true;
}
}

#endif //TRITUM_M_IO_H
