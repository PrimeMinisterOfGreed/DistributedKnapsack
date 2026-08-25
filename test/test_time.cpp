#include "time.hpp"

#include <gtest/gtest.h>

TEST(TimeSectionRegister, GetSectionReturnsZeroForUnknownName)
{
	auto section = TimeSectionRegister::instance().get_section("does_not_exist");
	EXPECT_EQ(section.count, 0u);
	EXPECT_DOUBLE_EQ(section.mean, 0.0);
	EXPECT_DOUBLE_EQ(section.min, 0.0);
	EXPECT_DOUBLE_EQ(section.max, 0.0);
	EXPECT_DOUBLE_EQ(section.variance, 0.0);
}

TEST(TimeSectionRegister, GetSectionReturnsRegisteredStats)
{
	auto &reg = TimeSectionRegister::instance();
	reg.start("test_get_section");
	reg.end("test_get_section");

	auto section = reg.get_section("test_get_section");
	EXPECT_EQ(section.count, 1u);
	EXPECT_GT(section.mean, 0.0);
	EXPECT_LE(section.min, section.mean);
	EXPECT_GE(section.max, section.mean);
}

TEST(TimeSectionRegister, NonOwnerThreadRefused)
{
	auto &reg = TimeSectionRegister::instance();
	reg.start("test_refused");

	std::thread other([&reg]() {
		reg.start("test_refused");
		reg.end("test_refused");
	});
	other.join();

	EXPECT_EQ(reg.get_section("test_refused").count, 0u);
}

TEST(TimeSectionRegister, GetAllSectionsReturnsAll)
{
	auto &reg = TimeSectionRegister::instance();
	reg.start("section_a");
	reg.end("section_a");
	reg.start("section_b");
	reg.end("section_b");

	auto all = reg.get_all_sections();
	EXPECT_NE(all.find("section_a"), all.end());
	EXPECT_NE(all.find("section_b"), all.end());

	auto unknown = TimeSectionRegister::instance().get_all_sections();
	EXPECT_GE(unknown.size(), 2u);
}