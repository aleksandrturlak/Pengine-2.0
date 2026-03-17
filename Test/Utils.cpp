#include <gtest/gtest.h>

#include "Utils/Utils.h"

using namespace Pengine;

TEST(Utils, ToLower)
{
	EXPECT_EQ(Utils::ToLower("HELLO"), "hello");
	EXPECT_EQ(Utils::ToLower("MixedCase"), "mixedcase");
	EXPECT_EQ(Utils::ToLower("already"), "already");
	EXPECT_EQ(Utils::ToLower(""), "");
}

TEST(Utils, Contains)
{
	EXPECT_TRUE(Utils::Contains("hello world", "world"));
	EXPECT_TRUE(Utils::Contains("hello world", "hello"));
	EXPECT_FALSE(Utils::Contains("hello world", "xyz"));
	EXPECT_TRUE(Utils::Contains("abc", ""));
}

TEST(Utils, GetFileFormat)
{
	EXPECT_EQ(Utils::GetFileFormat("texture.png"), ".png");
	EXPECT_EQ(Utils::GetFileFormat("model.gltf"), ".gltf");
	EXPECT_EQ(Utils::GetFileFormat("noextension"), "");
	EXPECT_EQ(Utils::GetFileFormat("path/to/file.jpg"), ".jpg");
}

TEST(Utils, GetFilename)
{
	EXPECT_EQ(Utils::GetFilename("texture.png"), "texture");
	EXPECT_EQ(Utils::GetFilename("path/to/model.gltf"), "model");
	EXPECT_EQ(Utils::GetFilename("noextension"), "noextension");
}

TEST(Utils, EraseFileFormat)
{
	EXPECT_EQ(Utils::EraseFileFormat("texture.png"), std::filesystem::path("texture"));
	EXPECT_EQ(Utils::EraseFileFormat("path/to/model.gltf"), std::filesystem::path("path/to/model"));
}

TEST(Utils, Replace)
{
	EXPECT_EQ(Utils::Replace("hello world", ' ', '_'), "hello_world");
	EXPECT_EQ(Utils::Replace("a/b/c", '/', '\\'), "a\\b\\c");
	EXPECT_EQ(Utils::Replace("noop", 'z', 'x'), "noop");
}

TEST(Utils, EraseFromBack)
{
	EXPECT_EQ(Utils::EraseFromBack("path/to/file", '/'), "path/to");
	EXPECT_EQ(Utils::EraseFromBack("noslash", '/'), "noslash");
}

TEST(Utils, EraseFromFront)
{
	EXPECT_EQ(Utils::EraseFromFront("path/to/file", '/'), "to/file");
	EXPECT_EQ(Utils::EraseFromFront("noslash", '/'), "noslash");
}

TEST(Utils, StringTypeToSize)
{
	EXPECT_EQ(Utils::StringTypeToSize("int"), 4u);
	EXPECT_EQ(Utils::StringTypeToSize("float"), 4u);
	EXPECT_EQ(Utils::StringTypeToSize("vec2"), 8u);
	EXPECT_EQ(Utils::StringTypeToSize("vec3"), 12u);
	EXPECT_EQ(Utils::StringTypeToSize("vec4"), 16u);
	EXPECT_EQ(Utils::StringTypeToSize("mat4"), 64u);
	EXPECT_EQ(Utils::StringTypeToSize("unknown"), static_cast<size_t>(-1));
}

TEST(Utils, ComputeBarycentric)
{
	// Point at vertex A → (1, 0, 0)
	const glm::vec3 a(0.0f, 0.0f, 0.0f);
	const glm::vec3 b(1.0f, 0.0f, 0.0f);
	const glm::vec3 c(0.0f, 1.0f, 0.0f);

	glm::vec3 bary = Utils::ComputeBarycentric(a, b, c, a);
	EXPECT_NEAR(bary.x, 1.0f, 1e-5f);
	EXPECT_NEAR(bary.y, 0.0f, 1e-5f);
	EXPECT_NEAR(bary.z, 0.0f, 1e-5f);

	// Point at centroid → (1/3, 1/3, 1/3)
	const glm::vec3 centroid = (a + b + c) / 3.0f;
	bary = Utils::ComputeBarycentric(a, b, c, centroid);
	EXPECT_NEAR(bary.x, 1.0f / 3.0f, 1e-5f);
	EXPECT_NEAR(bary.y, 1.0f / 3.0f, 1e-5f);
	EXPECT_NEAR(bary.z, 1.0f / 3.0f, 1e-5f);
}

TEST(Utils, IntersectAABBvsSphere)
{
	const glm::vec3 boxMin(-1.0f, -1.0f, -1.0f);
	const glm::vec3 boxMax(1.0f, 1.0f, 1.0f);

	// Sphere fully inside
	EXPECT_TRUE(Utils::IntersectAABBvsSphere(boxMin, boxMax, glm::vec3(0.0f), 0.5f));

	// Sphere touching the face
	EXPECT_TRUE(Utils::IntersectAABBvsSphere(boxMin, boxMax, glm::vec3(2.0f, 0.0f, 0.0f), 1.0f));

	// Sphere completely outside
	EXPECT_FALSE(Utils::IntersectAABBvsSphere(boxMin, boxMax, glm::vec3(5.0f, 0.0f, 0.0f), 1.0f));
}
