#include <random>

int rollRootBattleDie(std::mt19937& random) {
    return std::uniform_int_distribution<int>(0, 3)(random);
}
