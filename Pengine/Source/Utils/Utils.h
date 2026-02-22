#pragma once

#include "../Core/Core.h"
#include "../Core/Logger.h"
#include "../Core/BoundingBox.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

#include <filesystem>
#include <fstream>
#include <charconv>

namespace Pengine::Utils
{
	inline glm::vec3 GetPosition(const glm::mat4& position)
	{
		const float* matPtr = glm::value_ptr(position);
		return { matPtr[12], matPtr[13], matPtr[14] };
	}

	inline glm::vec3 GetScale(const glm::mat4& scale)
	{
		const float* matPtr = glm::value_ptr(scale);
		return { matPtr[0], matPtr[5], matPtr[10] };
	}

	inline bool Contains(const std::string& string, const std::string& content)
	{
		return string.find(content) != std::string::npos;
	}

	inline std::string ToLower(const std::string& string)
	{
		const size_t size = string.size();
		std::string lowerString;
		lowerString.resize(size);
		for (size_t i = 0; i < size; i++)
		{
			lowerString[i] = std::tolower(string[i]);
		}

		return lowerString;
	}

	inline std::string GetFileFormat(const std::filesystem::path& filepath)
	{
		if (filepath.has_extension())
		{
			return filepath.extension().string();
		}

		return {};
	}

	inline std::string GetFilename(const std::filesystem::path& filepath)
	{
		return filepath.filename().replace_extension().string();
	}

	inline std::filesystem::path EraseFileFormat(const std::filesystem::path& filepath)
	{
		if (filepath.has_extension())
		{
			return std::filesystem::path(filepath).replace_extension("").string();
		}

		return {};
	}

	template<typename T, typename U, typename V = T>
	V Find(const U& key, const std::unordered_map<U, T>& map)
	{
		auto foundItem = map.find(key);
		if (foundItem != map.end())
		{
			return foundItem->second;
		}

		return {};
	}

	template<typename T, typename U, typename V = T>
	V Find(const U& key, const std::map<U, T>& map)
	{
		auto foundItem = map.find(key);
		if (foundItem != map.end())
		{
			return foundItem->second;
		}

		return {};
	}

	template<typename T>
	bool Find(const T& key, const std::vector<T>& vector)
	{
		return std::find(vector.begin(), vector.end(), key) != vector.end();
	}

	template<typename T>
	bool Erase(std::vector<T>& vector, const T& item)
	{
		auto foundItem = std::find(vector.begin(), vector.end(), item);
		if (foundItem != vector.end())
		{
			vector.erase(foundItem);
			return true;
		}

		return false;
	}

	inline std::string Erase(std::string string, const std::string& what)
	{
		if (const size_t index = string.find(what); index != std::string::npos)
		{
			return string.erase(index, index + what.size());
		}

	 	return string;
	}

	inline std::wstring Erase(std::wstring string, const std::wstring& what)
	{
		if (const size_t index = string.find(what); index != std::wstring::npos)
		{
			return string.erase(index, index + what.size());
		}

		return string;
	}

	inline size_t StringTypeToSize(const std::string& type)
	{
		if (type == "int")
		{
			return 4;
		}
		if (type == "sampler")
		{
			return 4;
		}
		if(type == "float")
		{
			return 4;
		}
		if (type == "vec2")
		{
			return 8;
		}
		if (type == "vec3")
		{
			return 12;
		}
		if (type == "vec4")
		{
			return 16;
		}
		if (type == "color")
		{
			return 16;
		}
		if (type == "mat3")
		{
			return 36;
		}
		if (type == "mat4")
		{
			return 64;
		}

		return -1;
	}

	template<typename T>
	void SetValue(void* data, const size_t offset, T& value)
	{
		*(T*)((char*)data + offset) = value;
	}

	template<typename T>
	T& GetValue(void* data, const size_t offset)
	{
		return *(T*)((char*)data + offset);
	}

	inline std::string Replace(const std::string& string, const char what, const char to)
	{
		std::string replacedString = string;
		std::replace(replacedString.begin(), replacedString.end(), what, to);
		return replacedString;
	}

	inline UUID FindUuid(const std::filesystem::path& filepath)
	{
		std::lock_guard<std::mutex> lock(GlobalDataAccessor::GetInstance().GetUUIDMutex());
		auto foundItem = GlobalDataAccessor::GetInstance().GetUuidByFilepath().find(filepath);
		if (foundItem != GlobalDataAccessor::GetInstance().GetUuidByFilepath().end())
		{
			return foundItem->second;
		}

		return UUID(0, 0);
	}

	inline std::filesystem::path FindFilepath(const UUID& uuid)
	{
		std::lock_guard<std::mutex> lock(GlobalDataAccessor::GetInstance().GetUUIDMutex());
		auto foundItem = GlobalDataAccessor::GetInstance().GetFilepathByUuid().find(uuid);
		if (foundItem != GlobalDataAccessor::GetInstance().GetFilepathByUuid().end())
		{
			return foundItem->second;
		}

		return {};
	}

	inline void SetUUID(const UUID& uuid, const std::filesystem::path& filepath)
	{
		assert(!GlobalDataAccessor::GetInstance().GetFilepathByUuid().contains(uuid));

		std::lock_guard<std::mutex> lock(GlobalDataAccessor::GetInstance().GetUUIDMutex());
		GlobalDataAccessor::GetInstance().GetFilepathByUuid()[uuid] = filepath;
		GlobalDataAccessor::GetInstance().GetUuidByFilepath()[filepath] = uuid;
	}

	inline std::string EraseFromBack(std::string string, const char what)
	{
		if (const size_t index = string.find_last_of(what); index != std::string::npos)
		{
			return string.substr(0, index);
		}

		return string;
	}

	inline std::string EraseFromFront(std::string string, const char what)
	{
		if (const size_t index = string.find_first_of(what); index != std::string::npos)
		{
			return string.substr(index + 1, string.size() - 1);
		}

		return string;
	}

	inline std::filesystem::path GetShortFilepath(const std::filesystem::path& filepath)
	{
		const std::filesystem::path directory = std::move(std::filesystem::current_path());

		auto beginFilepath = filepath.begin();
		auto beginDirectory = directory.begin();

		while ((beginFilepath != filepath.end() && beginDirectory != directory.end()) &&
			(beginFilepath->wstring() == beginDirectory->wstring()))
		{
			beginFilepath++;
			beginDirectory++;
		}

		std::filesystem::path shortFilepath;
		while (beginFilepath != filepath.end())
		{
			shortFilepath /= *beginFilepath;
			beginFilepath++;
		}

		return shortFilepath;
	}

	inline std::string ReadFile(const std::filesystem::path& filepath)
	{
		std::ifstream file(filepath, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			FATAL_ERROR("Failed to open file: " + filepath.string());
		}

		const size_t fileSize = file.tellg();

		std::string buffer;
		buffer.resize(fileSize);

		file.seekg(0);
		file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
		file.close();

		return buffer;
	}

	inline bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale)
	{
		// From glm::decompose in matrix_decompose.inl

		using namespace glm;
		using T = float;

		mat4 LocalMatrix(transform);

		// Normalize the matrix.
		if (epsilonEqual(LocalMatrix[3][3], static_cast<float>(0), epsilon<T>())) return false;

		// First, isolate perspective.  This is the messiest.
		if (
			epsilonNotEqual(LocalMatrix[0][3], static_cast<T>(0), epsilon<T>()) ||
			epsilonNotEqual(LocalMatrix[1][3], static_cast<T>(0), epsilon<T>()) ||
			epsilonNotEqual(LocalMatrix[2][3], static_cast<T>(0), epsilon<T>()))
		{
			// Clear the perspective partition
			LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = static_cast<T>(0);
			LocalMatrix[3][3] = static_cast<T>(1);
		}

		// Next take care of translation (easy).
		translation = vec3(LocalMatrix[3]);
		LocalMatrix[3] = vec4(0, 0, 0, LocalMatrix[3].w);

		vec3 Row[3]/*, Pdum3*/;

		// Now get scale and shear.
		for (length_t i = 0; i < 3; ++i)
			for (length_t j = 0; j < 3; ++j)
				Row[i][j] = LocalMatrix[i][j];

		// Compute X scale factor and normalize first row.
		scale.x = length(Row[0]);
		Row[0] = detail::scale(Row[0], static_cast<T>(1));
		scale.y = length(Row[1]);
		Row[1] = detail::scale(Row[1], static_cast<T>(1));
		scale.z = length(Row[2]);
		Row[2] = detail::scale(Row[2], static_cast<T>(1));

		// At this point, the matrix (in rows[]) is orthonormal.
		// Check for a coordinate system flip.  If the determinant
		// is -1, then negate the matrix and the scaling factors.
#if 0
		Pdum3 = cross(Row[1], Row[2]); // v3Cross(row[1], row[2], Pdum3);
		if (dot(Row[0], Pdum3) < 0)
		{
			for (length_t i = 0; i < 3; i++)
			{
				scale[i] *= static_cast<T>(-1);
				Row[i] *= static_cast<T>(-1);
			}
		}
#endif

		rotation.y = asin(-Row[0][2]);
		if (cos(rotation.y) != 0)
		{
			rotation.x = atan2(Row[1][2], Row[2][2]);
			rotation.z = atan2(Row[0][1], Row[0][0]);
		}
		else
		{
			rotation.x = atan2(-Row[2][0], Row[1][1]);
			rotation.z = 0;
		}
		return true;
	}

	inline bool DecomposeRotation(const glm::mat4& rotationMat4, glm::vec3& rotation)
	{
		// From glm::decompose in matrix_decompose.inl

		using namespace glm;
		using T = float;

		mat4 LocalMatrix(rotationMat4);

		// Normalize the matrix.
		if (epsilonEqual(LocalMatrix[3][3], static_cast<float>(0), epsilon<T>())) return false;

		// First, isolate perspective.  This is the messiest.
		if (
			epsilonNotEqual(LocalMatrix[0][3], static_cast<T>(0), epsilon<T>()) ||
			epsilonNotEqual(LocalMatrix[1][3], static_cast<T>(0), epsilon<T>()) ||
			epsilonNotEqual(LocalMatrix[2][3], static_cast<T>(0), epsilon<T>()))
		{
			// Clear the perspective partition
			LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = static_cast<T>(0);
			LocalMatrix[3][3] = static_cast<T>(1);
		}

		LocalMatrix[3] = vec4(0, 0, 0, LocalMatrix[3].w);

		vec3 Row[3]/*, Pdum3*/;

		// Now get scale and shear.
		for (length_t i = 0; i < 3; ++i)
			for (length_t j = 0; j < 3; ++j)
				Row[i][j] = LocalMatrix[i][j];

		// Compute X scale factor and normalize first row.
		Row[0] = detail::scale(Row[0], static_cast<T>(1));
		Row[1] = detail::scale(Row[1], static_cast<T>(1));
		Row[2] = detail::scale(Row[2], static_cast<T>(1));

		// At this point, the matrix (in rows[]) is orthonormal.
		// Check for a coordinate system flip.  If the determinant
		// is -1, then negate the matrix and the scaling factors.
#if 0
		Pdum3 = cross(Row[1], Row[2]); // v3Cross(row[1], row[2], Pdum3);
		if (dot(Row[0], Pdum3) < 0)
		{
			for (length_t i = 0; i < 3; i++)
			{
				scale[i] *= static_cast<T>(-1);
				Row[i] *= static_cast<T>(-1);
			}
		}
#endif

		rotation.y = asin(-Row[0][2]);
		if (cos(rotation.y) != 0)
		{
			rotation.x = atan2(Row[1][2], Row[2][2]);
			rotation.z = atan2(Row[0][1], Row[0][0]);
		}
		else
		{
			rotation.x = atan2(-Row[2][0], Row[1][1]);
			rotation.z = 0;
		}
		return true;
	}

	inline uint64_t stringToUint64(const std::string& string, int base = 10)
	{
		uint64_t result = 0;
		auto [ptr, errorCode] = std::from_chars(
			string.data(),
			string.data() + string.size(),
			result,
			base
		);

		if (errorCode == std::errc::invalid_argument)
		{
			throw std::runtime_error("Not a valid number");
		}
		else if (errorCode == std::errc::result_out_of_range)
		{
			throw std::runtime_error("Value exceeds uint64_t range");
		}
		else if (ptr != string.data() + string.size())
		{
			throw std::runtime_error("Extra characters after number");
		}

		return result;
	}

	inline uint64_t hexStringToUint64(const std::string& hexString)
	{
		const char* start = hexString.c_str();
		const char* end = start + hexString.size();

		// Skip "0x" or "0X" prefix
		if (hexString.size() >= 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
			start += 2;
		}

		uint64_t result = 0;
		auto [ptr, ec] = std::from_chars(start, end, result, 16);

		if (ec == std::errc::invalid_argument)
		{
			throw std::invalid_argument("Invalid hexadecimal characters");
		}
		if (ec == std::errc::result_out_of_range)
		{
			throw std::runtime_error("Hexadecimal value exceeds uint64_t range");
		}
		if (ptr != end)
		{
			throw std::invalid_argument("Extra characters after hex value");
		}

		return result;
	}

	inline glm::vec3 ComputeBarycentric(
		const glm::vec3& a,
		const glm::vec3& b,
		const glm::vec3& c,
		const glm::vec3& p)
	{
		const glm::vec3 v0 = b - a;
		const glm::vec3 v1 = c - a;
		const glm::vec3 v2 = p - a;

		const float d00 = glm::dot(v0, v0);
		const float d01 = glm::dot(v0, v1);
		const float d11 = glm::dot(v1, v1);
		const float d20 = glm::dot(v2, v0);
		const float d21 = glm::dot(v2, v1);
		const float denom = d00 * d11 - d01 * d01;

		const float v = (d11 * d20 - d01 * d21) / denom;
		const float w = (d00 * d21 - d01 * d20) / denom;
		const float u = 1.0f - v - w;

		return glm::vec3(u, v, w);
	}

	inline std::array<glm::vec4, 6> GetFrustumPlanes(const glm::mat4& viewProjectionMat4)
	{
		std::array<glm::vec4, 6> planes;
		const glm::mat4& m = viewProjectionMat4;

		planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]); // Left
		planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]); // Right
		planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]); // Bottom
		planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]); // Top
		planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]); // Near
		planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]); // Far

		for (auto& plane : planes)
		{
			plane /= glm::length(glm::vec3(plane));
		}

		return planes;
	}

	inline bool IsSphereInsideFrustum(
		const std::array<glm::vec4, 6>& planes,
		const glm::vec3& position,
		float radius)
	{
		for (const auto& plane : planes)
		{
			float distance = plane.x * position.x +
				plane.y * position.y +
				plane.z * position.z +
				plane.w;

			if (distance < -radius)
			{
				return false;
			}
		}

		return true;
	}

	inline bool isAABBInsideFrustum(
		const std::array<glm::vec4, 6>& planes,
		const glm::vec3& min,
		const glm::vec3& max)
	{
		for (const auto& plane : planes)
		{
			const float distance = (plane.x >= 0.0f ? max.x : min.x) * plane.x
				+ (plane.y >= 0.0f ? max.y : min.y) * plane.y
				+ (plane.z >= 0.0f ? max.z : min.z) * plane.z
				+ plane.w;
			if (distance < 0.0f) return false;
		}
		return true;
	}

	inline bool IntersectAABBvsSphere(
		const glm::vec3& boxMin,
		const glm::vec3& boxMax,
		const glm::vec3& sphereCenter,
		float sphereRadius)
	{
		float dist2 = 0.0f;
		for (int i = 0; i < 3; i++)
		{
			const float v = sphereCenter[i];
			if      (v < boxMin[i]) { const float d = boxMin[i] - v; dist2 += d * d; }
			else if (v > boxMax[i]) { const float d = v - boxMax[i]; dist2 += d * d; }
		}
		return dist2 <= sphereRadius * sphereRadius;
	}

	inline AABB LocalToWorldAABB(const AABB& localAABB, const glm::mat4& m)
	{
		const glm::vec3 center  = (localAABB.min + localAABB.max) * 0.5f;
		const glm::vec3 extents =  localAABB.max - center;

		glm::vec3 wMin = { m[3][0], m[3][1], m[3][2] };
		glm::vec3 wMax = wMin;

		for (int col = 0; col < 3; col++)
		{
			const float cx = center[col];
			const float ex = extents[col];
			const glm::vec3 colVec = { m[col][0], m[col][1], m[col][2] };
			const glm::vec3 a = colVec * cx;
			const glm::vec3 b = glm::abs(colVec) * ex;
			wMin += a - b;
			wMax += a + b;
		}

		return { wMin, wMax };
	}
}
