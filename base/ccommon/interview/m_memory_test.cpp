#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <json.hpp>

#include "m_time.h"
#include "m_memory.h"


using POINT_TYPE = double;
using json = nlohmann::json;
const size_t PRECISION = sizeof(POINT_TYPE);

struct GlobalInfo {
    std::string vpr_info;
    std::string vpr_version;
    size_t precision; // 假设这里使用size_t来表示字节数
    size_t max_records_in_binary_file;
};

struct RecordInfo {
    std::array<double, 3> pose;
    size_t section_id;
    size_t index;
    std::vector<POINT_TYPE> data;
};

// 用于序列化和反序列化的类
class MetaData {
public:
    GlobalInfo global;
    std::vector<RecordInfo> records;

    void save(const std::string& filename) const {
        json j;
        j["global"] = {
            {"vpr_info", global.vpr_info},
            {"vpr_version", global.vpr_version},
            {"precision", global.precision},
            {"max_records_in_binary_file", global.max_records_in_binary_file}
        };
        
        j["records"] = json::array();
        for (const auto& record : records) {
            json recordJson;
            recordJson["pose"] = record.pose;
            recordJson["section_id"] = record.section_id;
            recordJson["index"] = record.index;            
            j["records"].push_back(recordJson);
        }

        std::ofstream o(filename);
        o << std::setw(4) << j << std::endl;
    }

    void load(const std::string& filename) {
         std::ifstream i(filename);
        json j;
        i >> j;
        global.vpr_info = j.at("global").at("vpr_info").get<std::string>();
        global.vpr_version = j.at("global").at("vpr_version").get<std::string>();
        global.precision = j.at("global").at("precision").get<size_t>();
        global.max_records_in_binary_file = j.at("global").at("max_records_in_binary_file").get<size_t>();
        records.clear();
        for (const auto& recordJson : j.at("records")) {
            RecordInfo record;
            record.pose = recordJson.at("pose").get<std::array<double, 3>>();
            record.section_id = recordJson.at("section_id").get<size_t>();
            record.index = recordJson.at("index").get<size_t>();
            records.push_back(record);
        }
    }
};

bool writeBinaryFile(const std::string& filename, const std::vector<double>& data, size_t size) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file." << std::endl;
        return false;
    }
    file.write(reinterpret_cast<const char*>(data.data()), size);
    file.close();
    return true;
}

bool readBinaryFile(const std::string& filename, std::vector<double>& data, size_t size) {
    
    
    std::ifstream file_in(filename, std::ios::binary);
    if (!file_in.is_open()) {
        std::cerr << "Error opening file." << std::endl;
        return false;
    }
    
    file_in.read(reinterpret_cast<char*>(data.data()), size);
    file_in.close();
    return true;
}

int main() {
    tritium::MTime mt("VPR_STORAGE_TEST");
    // double area_length = 100.0;
    // double area_width = 100.0;
    // double distance_per_pts = 5.0;
    const size_t record_embedding_dimensions = 8000.0;
    const size_t max_records_in_binary_file = 400;
    const size_t current_record_count = 5000;
    const size_t current_floats_count = current_record_count * record_embedding_dimensions;
    const size_t max_floats_in_binary_file = max_records_in_binary_file * record_embedding_dimensions;
    const size_t max_bytes_in_binary_file = max_floats_in_binary_file * PRECISION;
    const size_t binary_file_count = std::ceil(1.0 * current_record_count / max_records_in_binary_file);

    std::string base_dir = "/home/hhh/v0.0.1/";

    // 使用默认构造函数创建一个mt19937引擎实例, 创建一个双精度浮点数分布器
    std::mt19937 gen;
    std::uniform_real_distribution<POINT_TYPE> dis(0.0, 1.0);
    mt.Count("prepare const variable");
    
    
    MetaData meta;
    meta.global.vpr_version = "v0.0.1";
    meta.global.vpr_info = "Sample Map Info";
    meta.global.precision = PRECISION; // Assuming double precision
    meta.global.max_records_in_binary_file = max_records_in_binary_file;
    meta.records.resize(current_record_count);
    for (size_t i = 0; i < current_record_count; ++i) {
        RecordInfo record = {{dis(gen), dis(gen), dis(gen)}, i, i, {}};
        meta.records[i] = record;
    }
    // 测试程序使用的内存的大小
    PrintMemoryUsage();
    std::string json_file = base_dir + "/meta.json";
    mt.Count("prepare meta data");

    meta.save(json_file);
    mt.Count("save meta data");

    // Load and print back to verify
    MetaData meta_loaded;
    meta_loaded.load(json_file);
    mt.Count("load meta data");

    std::vector<POINT_TYPE> data(current_floats_count);
    // 填充向量
    for (auto &d : data) {
        d = dis(gen);
    }
    mt.Count("prepare bin data");

    for (size_t bin_index = 0; bin_index < binary_file_count; bin_index++)
    {
        std::string bin_file = base_dir + "/embedding_" + std::to_string(bin_index) + ".bin";
        // 写入数据到文件
        auto start = bin_index * max_floats_in_binary_file;
        auto end = std::min((bin_index + 1) * max_floats_in_binary_file, current_floats_count);
        std::vector<POINT_TYPE> cur_bin_data(data.begin() + start, data.begin() + end);
        writeBinaryFile(bin_file, cur_bin_data, cur_bin_data.size() * PRECISION);
    }
    mt.Count("write bin data");

    // 读取数据
    std::vector<std::vector<POINT_TYPE>> data_new(binary_file_count, std::vector<POINT_TYPE>(max_floats_in_binary_file, 0.0));
    for (size_t bin_index = 0; bin_index < binary_file_count; bin_index++)
    {   
        std::string bin_file = base_dir + "/embedding_" + std::to_string(bin_index) + ".bin";
        if (bin_index != binary_file_count - 1) {
            readBinaryFile(bin_file, data_new[bin_index], max_bytes_in_binary_file);
        } else {
            readBinaryFile(bin_file, data_new[bin_index], (current_floats_count - bin_index * max_floats_in_binary_file) * PRECISION);
        }
    }
    mt.Count("read bin data");

    return 0;
}