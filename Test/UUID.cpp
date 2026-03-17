#include <gtest/gtest.h>

#include "Core/UUID.h"
#include "Core/Logger.h"

using namespace Pengine;

TEST(UUID, UUID)
{
	try
	{
		UUID uuidDefault;
		std::string uuidString = uuidDefault.ToString();
		UUID uuid = UUID::FromString(uuidString);
		EXPECT_TRUE(uuidDefault == uuid);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(UUID, Uniqueness)
{
	try
	{
		UUID a;
		UUID b;
		EXPECT_FALSE(a == b);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(UUID, Invalid)
{
	try
	{
		UUID invalid(0, 0);
		EXPECT_FALSE(invalid.IsValid());

		UUID valid;
		EXPECT_TRUE(valid.IsValid());
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(UUID, CopyAssignment)
{
	try
	{
		UUID original;
		UUID copied(original);
		EXPECT_TRUE(original == copied);
		EXPECT_EQ(original.GetUpper(), copied.GetUpper());
		EXPECT_EQ(original.GetLower(), copied.GetLower());

		UUID moved(std::move(copied));
		EXPECT_TRUE(original == moved);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}
