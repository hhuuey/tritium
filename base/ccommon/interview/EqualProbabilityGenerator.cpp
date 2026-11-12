//
// Created by huang on 2023/2/4.
//

#include <iostream>
#include <random>
#include <vector>

// 不等概率生成器
int UnequalProbabilityGenerator(double p = 0.3) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::bernoulli_distribution dist(p);
    return dist(gen);
}

// 等概率生成器（0，1）
int EqualProbabilityGenerator1() {
    int x1 = UnequalProbabilityGenerator();
    int x2 = UnequalProbabilityGenerator();
    if (x1 == 0 && x2 == 1)
        return 0;
    else if (x1 == 1 && x2 == 0)
        return 1;
    else
        return EqualProbabilityGenerator1();
}

// 等概率生成器（0，1，2）
int EqualProbabilityGenerator2() {
    int x1 = UnequalProbabilityGenerator();
    int x2 = UnequalProbabilityGenerator();
    int x3 = UnequalProbabilityGenerator();
    if (x1 == 1 && x2 == 0 && x3 == 0)
        return 0;
    else if (x1 == 0 && x2 == 1 && x3 == 0)
        return 1;
    else if (x1 == 0 && x2 == 0 && x3 == 1)
        return 2;
    else
        return EqualProbabilityGenerator2();
}

// 等概率生成器（0，1，2，...，N-1）
int EqualProbabilityGenerator3(int N) {
    int k = 0;
    int n = N;
    while(n) {
        k++;
        n = n >> 1;
    }
    int res = 0;
    do {
        res = 0;
        for(int i = 0; i < k; i++){
            res |= EqualProbabilityGenerator1() << i;
        }
    } while(res >= N);

    return res;
}

int main() {
    const int numIterations = 100000;

    // Equal probability generator (0, 1)
    std::vector<int> result1(2, 0);
    for (int i = 0; i < numIterations; i++) {
        int num = EqualProbabilityGenerator1();
        result1[num]++;
    }
    std::cout << "Equal probability generator (0, 1) result statistics:" << std::endl;
    std::cout << "0: " << result1[0] << "(" << numIterations << ")" << " times" << std::endl;
    std::cout << "1: " << result1[1] << "(" << numIterations << ")" <<" times" << std::endl;
    std::cout << std::endl;

    // Equal probability generator (0, 1, 2)
    std::vector<int> result2(3, 0);
    for (int i = 0; i < numIterations; i++) {
        int num = EqualProbabilityGenerator2();
        result2[num]++;
    }
    std::cout << "Equal probability generator (0, 1, 2) result statistics:" << std::endl;
    std::cout << "0: " << result2[0] << "(" << numIterations << ")" <<" times" << std::endl;
    std::cout << "1: " << result2[1] << "(" << numIterations << ")" <<" times" << std::endl;
    std::cout << "2: " << result2[2] << "(" << numIterations << ")" <<" times" << std::endl;
    std::cout << std::endl;

    // Equal probability generator (0, 1, 2, ..., N-1)
    const int N = 10;  // Set the range of generated random numbers
    std::vector<int> result3(N, 0);
    for (int i = 0; i < numIterations; i++) {
        int num = EqualProbabilityGenerator3(N);
        result3[num]++;
    }
    std::cout << "Equal probability generator (0, 1, 2, ..., N-1) result statistics:" << std::endl;
    for (int i = 0; i < N; i++) {
        std::cout << i << ": " << result3[i] << "(" << numIterations << ")" << " times" << std::endl;
    }
    return 0;
}