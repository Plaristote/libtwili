#pragma once
#include "parser.hpp"
#include <filesystem>
#include <vector>

bool probe_and_run_parser(TwiliParser&, int argc, const char** argv, std::vector<std::filesystem::path>&);
bool probe_and_run_parser(TwiliParser&, int argc = 0, const char** argv = nullptr);
bool run_parser(TwiliParser&, const std::vector<std::filesystem::path>&, int argc = 0, const char** argv = nullptr);
