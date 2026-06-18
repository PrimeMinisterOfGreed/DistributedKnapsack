#pragma once
#include "Knapsack/knapsackcopa.hpp"
#include "knapsack.hpp"
#include <concepts>
#include <ranges>
#include <span>

#pragma region Concepts
template <typename T>
concept CopaItem = requires(T v) {
	{ v.totalWeight } -> std::convertible_to<int>;
	{ v.totalValue } -> std::convertible_to<int>;
	{ v.index } -> std::convertible_to<int>;
};

template <typename Range>
concept CopaRange = std::ranges::input_range<Range> && CopaItem<typename Range::value_type>;

template <typename Range>
concept CopaOutputRange = std::ranges::output_range<Range, const typename Range::value_type>;

template <typename Range>
concept CopaBlockOutputRange = std::ranges::output_range<Range, CopaBlock>;

template <typename Range>
concept CopaBlockInputRange = std::ranges::input_range<Range> && std::same_as<typename Range::value_type, CopaBlock>;

template <typename T>
concept CopaBlockPairingOutRange = std::ranges::output_range<T, std::pair<CopaBlock, CopaBlock>>;
template <typename CorankComparable>
concept CoRankComparable = requires(CorankComparable a, CorankComparable b) {
	{ a > b } -> std::convertible_to<bool>;
	{ a >= b } -> std::convertible_to<bool>;
};

template <typename CorankOrderableRange>
concept CoRankOrderableRange =
	std::ranges::input_range<CorankOrderableRange> && CoRankComparable<typename CorankOrderableRange::value_type>;

#pragma endregion