#include "weight_parser.hpp"
#include <stdexcept>
#include <fstream>
#include <sstream>

std::vector<weight_entry> parse_weights(const std::string &filename)
{
    std::vector<weight_entry> entries;
    std::ifstream file(filename);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open weights file");
    }
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        int weight, profit;
        if (!(iss >> weight >> profit))
        {
            continue; // skip malformed lines
        }
        entries.push_back({weight, profit});
    }
    return entries;
}


