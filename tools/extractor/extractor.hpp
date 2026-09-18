#pragma once

#include <filesystem>
#include <string>

bool extractAction52(const std::filesystem::path& romPath,
                     const std::filesystem::path& outputDirectory,
                     std::string& error);

