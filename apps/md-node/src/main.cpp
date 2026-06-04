#include <iostream>

int main(int argc, char** argv) {
    std::cout << "md-node skeleton" << '\n';
    std::cout << "roles: gateway, controller, or gateway,controller" << '\n';
    std::cout << "args:";
    for (int i = 1; i < argc; ++i) {
        std::cout << ' ' << argv[i];
    }
    std::cout << '\n';
    return 0;
}
