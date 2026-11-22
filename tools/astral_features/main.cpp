// astral-features check <feature sources folder> <feature manifests folder>
// Exit code 1 and one line per violation if a feature breaks the rules of ADR-0023.
#include <astral/app/feature_check.hpp>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    if (argc != 4 || std::strcmp(argv[1], "check") != 0) {
        std::fprintf(stderr, "usage: astral-features check <feature sources folder> <feature manifests folder>\n");
        return 2;
    }
    const auto problems = astral::check_features(argv[2], argv[3]);
    for (const std::string& p : problems) std::fprintf(stderr, "%s\n", p.c_str());
    if (!problems.empty()) return 1;
    std::printf("features ok\n");
    return 0;
}
