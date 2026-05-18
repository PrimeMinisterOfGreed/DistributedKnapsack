#pragma once
#include <string>
#include <vector>

struct weight_entry
{
	int weight;
	int profit;
};

std::vector<weight_entry> parse_weights(const std::string &filename);