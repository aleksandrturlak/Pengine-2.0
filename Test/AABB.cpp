#include <gtest/gtest.h>

#include "Core/BoundingBox.h"

#include <limits>

using namespace Pengine;

TEST(AABB, DefaultConstructor)
{
	AABB aabb;
	EXPECT_EQ(aabb.min.x, std::numeric_limits<float>::max());
	EXPECT_EQ(aabb.min.y, std::numeric_limits<float>::max());
	EXPECT_EQ(aabb.min.z, std::numeric_limits<float>::max());
	EXPECT_EQ(aabb.max.x, std::numeric_limits<float>::lowest());
	EXPECT_EQ(aabb.max.y, std::numeric_limits<float>::lowest());
	EXPECT_EQ(aabb.max.z, std::numeric_limits<float>::lowest());
}

TEST(AABB, Center)
{
	AABB aabb(glm::vec3(-1.0f, -2.0f, -3.0f), glm::vec3(1.0f, 2.0f, 3.0f));
	glm::vec3 center = aabb.Center();
	EXPECT_NEAR(center.x, 0.0f, 1e-5f);
	EXPECT_NEAR(center.y, 0.0f, 1e-5f);
	EXPECT_NEAR(center.z, 0.0f, 1e-5f);

	AABB aabb2(glm::vec3(0.0f), glm::vec3(2.0f, 4.0f, 6.0f));
	glm::vec3 center2 = aabb2.Center();
	EXPECT_NEAR(center2.x, 1.0f, 1e-5f);
	EXPECT_NEAR(center2.y, 2.0f, 1e-5f);
	EXPECT_NEAR(center2.z, 3.0f, 1e-5f);
}

TEST(AABB, SurfaceArea)
{
	// Unit cube: 2*(1*1 + 1*1 + 1*1) = 6
	AABB unitCube(glm::vec3(0.0f), glm::vec3(1.0f));
	EXPECT_NEAR(unitCube.SurfaceArea(), 6.0f, 1e-5f);

	// 2x3x4 box: 2*(2*3 + 2*4 + 3*4) = 2*(6+8+12) = 52
	AABB box(glm::vec3(0.0f), glm::vec3(2.0f, 3.0f, 4.0f));
	EXPECT_NEAR(box.SurfaceArea(), 52.0f, 1e-4f);
}

TEST(AABB, Expanded)
{
	AABB a(glm::vec3(-1.0f), glm::vec3(1.0f));
	AABB b(glm::vec3(0.0f), glm::vec3(3.0f));

	AABB merged = a.Expanded(b);
	EXPECT_NEAR(merged.min.x, -1.0f, 1e-5f);
	EXPECT_NEAR(merged.min.y, -1.0f, 1e-5f);
	EXPECT_NEAR(merged.min.z, -1.0f, 1e-5f);
	EXPECT_NEAR(merged.max.x, 3.0f, 1e-5f);
	EXPECT_NEAR(merged.max.y, 3.0f, 1e-5f);
	EXPECT_NEAR(merged.max.z, 3.0f, 1e-5f);
}
