#include "m_io.h"

#include <iostream>
#include <iterator>

int main()
{
    auto files = tritium::get_files("D:/");
    std::copy(files.begin(), files.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
    std::cout << std::endl;

    std::string path = "D:/huang/tmp/tmp1";
    tritium::create_dirs(path);

    std::string file = "D:/huang/tmp//tmp1/tmp1.txt";
    std::vector<std::vector<double>> test_data1 = {
            {1.2, 2.2, 3.2},
            {2.3, 3.4, 3.1}
    };
    tritium::create_txt(file, test_data1, ',');
    tritium::create_txt(file, test_data1, ',', false);
    std::vector<std::vector<double>> test_data2;
    tritium::read_txt(file, test_data2, ',');

    for (int i = 0; i < test_data2.size(); ++i) {
        for (int j = 0; j < test_data2[i].size(); ++j) {
            std::cout <<  test_data2[i][j] << " ";
        }
        std::cout << std::endl;
    }

    std::string path1 = "D:/huang";
    tritium::remove_dirs(path1);
    return 0;
}