#include "Serializer.h"

#include "AsyncAssetLoader.h"
#include "FileFormatNames.h"
#include "Logger.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include "RenderPassManager.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "ThreadPool.h"
#include "ViewportManager.h"
#include "Viewport.h"
#include "WindowManager.h"
#include "Profiler.h"
#include "ClayManager.h"
#include "ReflectionSystem.h"

#include "../EventSystem/EventSystem.h"
#include "../EventSystem/NextFrameEvent.h"

#include "../Components/Camera.h"
#include "../Components/Decal.h"
#include "../Components/DirectionalLight.h"
#include "../Components/PointLight.h"
#include "../Components/SpotLight.h"
#include "../Components/Renderer3D.h"
#include "../Components/SkeletalAnimator.h"
#include "../Components/Transform.h"
#include "../Components/Canvas.h"
#include "../Components/RigidBody.h"

#include "../ComponentSystems/PhysicsSystem.h"

#include "../Utils/Utils.h"

#include "../Graphics/Vertex.h"

#include <stbi/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stbi/stb_image_write.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "fastgltf/tools.hpp"
#include "meshoptimizer/src/meshoptimizer.h"

using namespace Pengine;

namespace YAML
{

	template<>
	struct convert<UUID>
	{
		static Node encode(const UUID& rhs)
		{
			// ? Maybe no need, don't use encode.
			return {};
		}

		static bool decode(const Node& node, UUID& rhs)
		{
			const std::string uuidString = node.as<std::string>();
			rhs = std::move(UUID::FromString(uuidString));

			return true;
		}
	};

	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::quat>
	{
		static Node encode(const glm::quat& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::quat& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::mat4>
	{
		static Node encode(const glm::mat4& rhs)
		{
			Node node;
			node.push_back(rhs[0][0]);
			node.push_back(rhs[0][1]);
			node.push_back(rhs[0][2]);
			node.push_back(rhs[0][3]);

			node.push_back(rhs[1][0]);
			node.push_back(rhs[1][1]);
			node.push_back(rhs[1][2]);
			node.push_back(rhs[1][3]);

			node.push_back(rhs[2][0]);
			node.push_back(rhs[2][1]);
			node.push_back(rhs[2][2]);
			node.push_back(rhs[2][3]);

			node.push_back(rhs[3][0]);
			node.push_back(rhs[3][1]);
			node.push_back(rhs[3][2]);
			node.push_back(rhs[3][3]);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::mat4& rhs)
		{
			if (!node.IsSequence() || node.size() != 16)
				return false;

			rhs[0][0] = node[0].as<float>();
			rhs[0][1] = node[1].as<float>();
			rhs[0][2] = node[2].as<float>();
			rhs[0][3] = node[3].as<float>();

			rhs[1][0] = node[4].as<float>();
			rhs[1][1] = node[5].as<float>();
			rhs[1][2] = node[6].as<float>();
			rhs[1][3] = node[7].as<float>();

			rhs[2][0] = node[8].as<float>();
			rhs[2][1] = node[9].as<float>();
			rhs[2][2] = node[10].as<float>();
			rhs[2][3] = node[11].as<float>();

			rhs[3][0] = node[12].as<float>();
			rhs[3][1] = node[13].as<float>();
			rhs[3][2] = node[14].as<float>();
			rhs[3][3] = node[15].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::ivec2>
	{
		static Node encode(const glm::ivec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::ivec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<int>();
			rhs.y = node[1].as<int>();
			return true;
		}
	};

	template<>
	struct convert<glm::ivec3>
	{
		static Node encode(const glm::ivec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::ivec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<int>();
			rhs.y = node[1].as<int>();
			rhs.z = node[2].as<int>();
			return true;
		}
	};

	template<>
	struct convert<glm::ivec4>
	{
		static Node encode(const glm::ivec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::ivec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<int>();
			rhs.y = node[1].as<int>();
			rhs.z = node[2].as<int>();
			rhs.w = node[3].as<int>();
			return true;
		}
	};

	template<>
	struct convert<glm::dvec2>
	{
		static Node encode(const glm::dvec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::dvec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<double>();
			rhs.y = node[1].as<double>();
			return true;
		}
	};

	template<>
	struct convert<glm::dvec3>
	{
		static Node encode(const glm::dvec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::dvec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<double>();
			rhs.y = node[1].as<double>();
			rhs.z = node[2].as<double>();
			return true;
		}
	};

	template<>
	struct convert<glm::dvec4>
	{
		static Node encode(const glm::dvec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::dvec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<double>();
			rhs.y = node[1].as<double>();
			rhs.z = node[2].as<double>();
			rhs.w = node[3].as<double>();
			return true;
		}
	};
}

YAML::Emitter& operator<<(YAML::Emitter& out, const UUID& uuid)
{
	std::string uuidString = uuid.ToString();
	out << uuidString;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const std::filesystem::path &filepath)
{
	out << filepath.string();
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::quat& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::ivec2& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::ivec3& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::ivec4& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::dvec2& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::dvec3& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const glm::dvec4& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return out;
}

YAML::Emitter& operator<<(YAML::Emitter& out, const float* v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq <<
		v[0] << v[1] << v[2] << v[3] <<
		v[4] << v[5] << v[6] << v[7] <<
		v[8] << v[9] << v[10] << v[11] <<
		v[12] << v[13] << v[14] << v[15] <<
		YAML::EndSeq;
	return out;
}

EngineConfig Serializer::DeserializeEngineConfig(const std::filesystem::path& filepath)
{
	if (filepath.empty() || filepath == none)
	{
		FATAL_ERROR(filepath.string() + ":Failed to load engine config! Filepath is incorrect!");
	}

	std::ifstream stream(filepath);
	std::stringstream stringStream;

	stringStream << stream.rdbuf();

	stream.close();

	YAML::Node data = YAML::LoadMesh(stringStream.str());
	if (!data)
	{
		FATAL_ERROR(filepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
	}

	EngineConfig engineConfig{};
	if (YAML::Node graphicsAPIData = data["GraphicsAPI"])
	{
		engineConfig.graphicsAPI = static_cast<GraphicsAPI>(graphicsAPIData.as<int>());
	}
	else
	{
		engineConfig.graphicsAPI = GraphicsAPI::Vk;
	}

	Logger::Log("Engine config has been loaded!", BOLDGREEN);
	Logger::Log("Graphics API:" + std::to_string(static_cast<int>(engineConfig.graphicsAPI)));

	return engineConfig;
}

void Serializer::GenerateFilesUUID(const std::filesystem::path& directory)
{
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
	{
		if (!entry.is_directory())
		{
			const std::filesystem::path filepath = Utils::GetShortFilepath(entry.path());
			std::filesystem::path metaFilepath = filepath;
			metaFilepath.concat(FileFormats::Meta());

			/*if (std::filesystem::exists(metaFilepath))
			{
				std::filesystem::remove(metaFilepath);
				continue;
			}*/

			if (Utils::FindUuid(filepath).IsValid())
			{
				continue;
			}

			if (FileFormats::IsAsset(Utils::GetFileFormat(filepath)))
			{
				if (!std::filesystem::exists(metaFilepath))
				{
					GenerateFileUUID(filepath);
				}
				else
				{
					std::ifstream stream(metaFilepath);
					std::stringstream stringStream;

					stringStream << stream.rdbuf();

					stream.close();

					YAML::Node data = YAML::LoadMesh(stringStream.str());
					if (!data)
					{
						FATAL_ERROR(filepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
					}

					UUID uuid;
					if (YAML::Node uuidData = data["UUID"]; !uuidData)
					{
						FATAL_ERROR(filepath.string() + ":Failed to load meta file!");
					}
					else
					{
						uuid = uuidData.as<UUID>();
					}

					Utils::SetUUID(uuid, filepath);
				}
			}
		}
	}
}

UUID Serializer::GenerateFileUUID(const std::filesystem::path& filepath)
{
	UUID uuid = Utils::FindUuid(filepath);
	if (uuid.IsValid())
	{
		return uuid;
	}

	YAML::Emitter out;

	out << YAML::BeginMap;

	uuid = UUID();
	out << YAML::Key << "UUID" << YAML::Value << uuid;

	out << YAML::EndMap;

	std::filesystem::path metaFilepath = filepath;
	metaFilepath.concat(FileFormats::Meta()); 
	std::ofstream fout(metaFilepath);
	fout << out.c_str();
	fout.close();

	Utils::SetUUID(uuid, filepath);

	return uuid;
}

void Serializer::DeserializeDescriptorSets(
	const YAML::detail::iterator_value& pipelineData,
	const std::string& passName,
	std::map<Pipeline::DescriptorSetIndexType, std::map<std::string, uint32_t>>& descriptorSetIndicesByType)
{
	for (const auto& descriptorSetData : pipelineData["DescriptorSets"])
	{
		Pipeline::DescriptorSetIndexType type{};
		if (const auto& typeData = descriptorSetData["Type"])
		{
			std::string typeName = typeData.as<std::string>();
			if (typeName == "Bindless")
			{
				type = Pipeline::DescriptorSetIndexType::BINDLESS;
			}
			if (typeName == "Renderer")
			{
				type = Pipeline::DescriptorSetIndexType::RENDERER;
			}
			if (typeName == "Scene")
			{
				type = Pipeline::DescriptorSetIndexType::SCENE;
			}
			else if (typeName == "RenderPass")
			{
				type = Pipeline::DescriptorSetIndexType::RENDERPASS;
			}
			else if (typeName == "BaseMaterial")
			{
				type = Pipeline::DescriptorSetIndexType::BASE_MATERIAL;
			}
			else if (typeName == "Material")
			{
				type = Pipeline::DescriptorSetIndexType::MATERIAL;
			}
			else if (typeName == "Object")
			{
				type = Pipeline::DescriptorSetIndexType::OBJECT;
			}
		}

		std::string attachedPassName = passName;
		if (const auto& passNameData = descriptorSetData["RenderPass"])
		{
			attachedPassName = passNameData.as<std::string>();
		}

		uint32_t set = 0;
		if (const auto& setData = descriptorSetData["Set"])
		{
			set = setData.as<uint32_t>();
		}

		descriptorSetIndicesByType[type][attachedPassName] = set;
	}
}

void Serializer::DeserializeShaderFilepaths(
	const YAML::detail::iterator_value& pipelineData,
	std::map<ShaderModule::Type, std::filesystem::path>& shaderFilepathsByType)
{
	auto getShaderFilepath = [](const YAML::Node& node)
	{
		const std::filesystem::path filepathOrUUID = node.as<std::string>();
		if (std::filesystem::exists(filepathOrUUID))
		{
			return filepathOrUUID;
		}
		else
		{
			return Utils::FindFilepath(node.as<UUID>());
		}
	};

	if (const auto& vertexData = pipelineData["Vertex"])
	{
		shaderFilepathsByType[ShaderModule::Type::VERTEX] = getShaderFilepath(vertexData);
	}

	if (const auto& fragmentData = pipelineData["Fragment"])
	{
		shaderFilepathsByType[ShaderModule::Type::FRAGMENT] = getShaderFilepath(fragmentData);
	}

	if (const auto& geometryData = pipelineData["Geometry"])
	{
		shaderFilepathsByType[ShaderModule::Type::GEOMETRY] = getShaderFilepath(geometryData);
	}

	if (const auto& computeData = pipelineData["Compute"])
	{
		shaderFilepathsByType[ShaderModule::Type::COMPUTE] = getShaderFilepath(computeData);
	}
}

void Serializer::SerializeTexture(const std::filesystem::path& filepath, std::shared_ptr<Texture> texture, bool* isLoaded)
{
	Texture::CreateInfo createInfo{};
	createInfo.aspectMask = texture->GetAspectMask();
	createInfo.instanceSize = texture->GetInstanceSize();
	createInfo.filepath = "Copy";
	createInfo.name = "Copy";
	createInfo.format = texture->GetFormat();
	createInfo.size = texture->GetSize();
	createInfo.usage = { Texture::Usage::TRANSFER_DST, Texture::Usage::SAMPLED };
	createInfo.memoryType = MemoryType::CPU;
	auto copy = Texture::Create(createInfo);

	Texture::Region region{};
	region.extent = { texture->GetSize().x, texture->GetSize().y, 1 };
	copy->Copy(texture, region);

	uint8_t* data = (uint8_t*)copy->GetData();
	const auto subresourceLayout = copy->GetSubresourceLayout();
	data += subresourceLayout.offset;

	ThreadPool::GetInstance().EnqueueAsync([filepath, copy, data, subresourceLayout, isLoaded]()
	{
		Texture::Meta meta{};
		meta.uuid = UUID();
		meta.createMipMaps = false;
		meta.srgb = true;
		meta.filepath = filepath;
		meta.filepath.concat(FileFormats::Meta());

		Utils::SetUUID(meta.uuid, meta.filepath);
		SerializeTextureMeta(meta);

		stbi_flip_vertically_on_write(false);
		stbi_write_png(filepath.string().c_str(), copy->GetSize().x, copy->GetSize().y, 4, data, subresourceLayout.rowPitch);

		Logger::Log("Texture:" + filepath.string() + " has been saved!", BOLDGREEN);

		if (isLoaded)
		{
			*isLoaded = true;
		}
	});	
}

GraphicsPipeline::CreateGraphicsInfo Serializer::DeserializeGraphicsPipeline(const YAML::detail::iterator_value& pipelineData)
{
	std::string passName;

	GraphicsPipeline::CreateGraphicsInfo createGraphicsInfo{};
	if (const auto& renderPassData = pipelineData["RenderPass"])
	{
		passName = renderPassData.as<std::string>();
		createGraphicsInfo.renderPass = RenderPassManager::GetInstance().GetRenderPass(passName);
	}

	if (const auto& depthTestData = pipelineData["DepthTest"])
	{
		createGraphicsInfo.depthTest = depthTestData.as<bool>();
	}

	if (const auto& depthWriteData = pipelineData["DepthWrite"])
	{
		createGraphicsInfo.depthWrite = depthWriteData.as<bool>();
	}

	if (const auto& depthClampData = pipelineData["DepthClamp"])
	{
		createGraphicsInfo.depthClamp = depthClampData.as<bool>();
	}

	if (const auto& depthCompareData = pipelineData["DepthCompare"])
	{
		const std::string depthCompare = depthCompareData.as<std::string>();
		if (depthCompare == "Never")
		{
			createGraphicsInfo.depthCompare = GraphicsPipeline::DepthCompare::NEVER;
		}
		else if (depthCompare == "Less")
		{
			createGraphicsInfo.depthCompare = GraphicsPipeline::DepthCompare::LESS;
		}
		else if (depthCompare == "Equal")
		{
			createGraphicsInfo.depthCompare = GraphicsPipeline::DepthCompare::EQUAL;
		}
		else if (depthCompare == "LessOrEqual")
		{
			createGraphicsInfo.depthCompare = GraphicsPipeline::DepthCompare::LESS_OR_EQUAL;
		}
		else if (depthCompare == "Greater")
		{
			createGraphicsInfo.depthCompare = GraphicsPipeline::DepthCompare::GREATER;
		}
		else if (depthCompare == "NotEqual")
		{
			createGraphicsInfo.depthCompare = GraphicsPipeline::DepthCompare::NOT_EQUAL;
		}
		else if (depthCompare == "GreaterOrEqual")
		{
			createGraphicsInfo.depthCompare = GraphicsPipeline::DepthCompare::GREATER_OR_EQUAL;
		}
		else if (depthCompare == "Always")
		{
			createGraphicsInfo.depthCompare = GraphicsPipeline::DepthCompare::ALWAYS;
		}
	}

	if (const auto& cullModeData = pipelineData["CullMode"])
	{
		if (const std::string& cullMode = cullModeData.as<std::string>(); cullMode == "Front")
		{
			createGraphicsInfo.cullMode = GraphicsPipeline::CullMode::FRONT;
		}
		else if(cullMode == "Back")
		{
			createGraphicsInfo.cullMode = GraphicsPipeline::CullMode::BACK;
		}
		else if (cullMode == "FrontAndBack")
		{
			createGraphicsInfo.cullMode = GraphicsPipeline::CullMode::FRONT_AND_BACK;
		}
		else if (cullMode == "None")
		{
			createGraphicsInfo.cullMode = GraphicsPipeline::CullMode::NONE;
		}
	}

	if (const auto& topologyModeData = pipelineData["TopologyMode"])
	{
		if (const std::string& topologyMode = topologyModeData.as<std::string>(); topologyMode == "LineList")
		{
			createGraphicsInfo.topologyMode = GraphicsPipeline::TopologyMode::LINE_LIST;
		}
		else if (topologyMode == "PointList")
		{
			createGraphicsInfo.topologyMode = GraphicsPipeline::TopologyMode::POINT_LIST;
		}
		else if (topologyMode == "TriangleList")
		{
			createGraphicsInfo.topologyMode = GraphicsPipeline::TopologyMode::TRIANGLE_LIST;
		}
	}

	if (const auto& polygonModeData = pipelineData["PolygonMode"])
	{
		if (const std::string& polygonMode = polygonModeData.as<std::string>(); polygonMode == "Fill")
		{
			createGraphicsInfo.polygonMode = GraphicsPipeline::PolygonMode::FILL;
		}
		else if (polygonMode == "Line")
		{
			createGraphicsInfo.polygonMode = GraphicsPipeline::PolygonMode::LINE;
		}
	}

	DeserializeShaderFilepaths(pipelineData, createGraphicsInfo.shaderFilepathsByType);

	DeserializeDescriptorSets(pipelineData, passName, createGraphicsInfo.descriptorSetIndicesByType);

	for (const auto& vertexInputBindingDescriptionData : pipelineData["VertexInputBindingDescriptions"])
	{
		auto& bindingDescription = createGraphicsInfo.bindingDescriptions.emplace_back();

		if (const auto& bindingData = vertexInputBindingDescriptionData["Binding"])
		{
			bindingDescription.binding = bindingData.as<uint32_t>();
		}

		if (const auto& inputRateData = vertexInputBindingDescriptionData["InputRate"])
		{
			std::string inputRateName = inputRateData.as<std::string>();
			if (inputRateName == "Vertex")
			{
				bindingDescription.inputRate = GraphicsPipeline::InputRate::VERTEX;
			}
			else if (inputRateName == "Instance")
			{
				bindingDescription.inputRate = GraphicsPipeline::InputRate::INSTANCE;
			}
		}

		for (const auto& nameData : vertexInputBindingDescriptionData["Names"])
		{
			bindingDescription.names.emplace_back(nameData.as<std::string>());
		}

		if (const auto& tagData = vertexInputBindingDescriptionData["Tag"])
		{
			bindingDescription.tag = tagData.as<std::string>();
		}
	}

	for (const auto& colorBlendStateData : pipelineData["ColorBlendStates"])
	{
		GraphicsPipeline::BlendStateAttachment blendStateAttachment{};

		if (const auto& blendEnabledData = colorBlendStateData["BlendEnabled"])
		{
			blendStateAttachment.blendEnabled = blendEnabledData.as<bool>();
		}

		auto blendOpParse = [](const std::string& blendOpType)
		{
			if (blendOpType == "add")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendOp::ADD;
			}
			else if (blendOpType == "subtract")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendOp::SUBTRACT;
			}
			else if (blendOpType == "reverse subtract")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendOp::REVERSE_SUBTRACT;
			}
			else if (blendOpType == "max")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendOp::MAX;
			}
			else if (blendOpType == "min")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendOp::MIN;
			}

			Logger::Error(blendOpType + ":Can't parse blend op type!");
			return GraphicsPipeline::BlendStateAttachment::BlendOp::ADD;
		};

		auto blendFactorParse = [](const std::string& blendFactorType)
		{
			if (blendFactorType == "constant alpha")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::CONSTANT_ALPHA;
			}
			else if (blendFactorType == "constant color")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::CONSTANT_COLOR;
			}
			else if (blendFactorType == "dst alpha")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::DST_ALPHA;
			}
			else if (blendFactorType == "dst color")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::DST_COLOR;
			}
			else if (blendFactorType == "one")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE;
			}
			else if (blendFactorType == "one minus constant alpha")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_CONSTANT_ALPHA;
			}
			else if (blendFactorType == "one minus constant color")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_CONSTANT_COLOR;
			}
			else if (blendFactorType == "one minus dst alpha")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_DST_ALPHA;
			}
			else if (blendFactorType == "one minus dst color")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_DST_COLOR;
			}
			else if (blendFactorType == "one minus src alpha")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_SRC_ALPHA;
			}
			else if (blendFactorType == "one minus src color")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_SRC_COLOR;
			}
			else if (blendFactorType == "src alpha")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::SRC_ALPHA;
			}
			else if (blendFactorType == "src alpha saturate")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::SRC_ALPHA_SATURATE;
			}
			else if (blendFactorType == "src color")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::SRC_COLOR;
			}
			else if (blendFactorType == "zero")
			{
				return GraphicsPipeline::BlendStateAttachment::BlendFactor::ZERO;
			}

			Logger::Error(blendFactorType + ":Can't parse blend factor type!");
			return GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE;
		};

		if (const auto& blendOpData = colorBlendStateData["ColorBlendOp"])
		{
			blendStateAttachment.colorBlendOp = blendOpParse(blendOpData.as<std::string>());
		}

		if (const auto& blendOpData = colorBlendStateData["AlphaBlendOp"])
		{
			blendStateAttachment.alphaBlendOp = blendOpParse(blendOpData.as<std::string>());
		}

		if (const auto& srcBlendFactorData = colorBlendStateData["SrcColorBlendFactor"])
		{
			blendStateAttachment.srcColorBlendFactor = blendFactorParse(srcBlendFactorData.as<std::string>());
		}

		if (const auto& dstBlendFactorData = colorBlendStateData["DstColorBlendFactor"])
		{
			blendStateAttachment.dstColorBlendFactor = blendFactorParse(dstBlendFactorData.as<std::string>());
		}

		if (const auto& srcBlendFactorData = colorBlendStateData["SrcAlphaBlendFactor"])
		{
			blendStateAttachment.srcAlphaBlendFactor = blendFactorParse(srcBlendFactorData.as<std::string>());
		}

		if (const auto& dstBlendFactorData = colorBlendStateData["DstAlphaBlendFactor"])
		{
			blendStateAttachment.dstAlphaBlendFactor = blendFactorParse(dstBlendFactorData.as<std::string>());
		}

		createGraphicsInfo.colorBlendStateAttachments.emplace_back(blendStateAttachment);
	}

	ParseUniformValues(pipelineData, createGraphicsInfo.uniformInfo);

	return createGraphicsInfo;
}


ComputePipeline::CreateComputeInfo Serializer::DeserializeComputePipeline(const YAML::detail::iterator_value& pipelineData)
{
	std::string passName;

	ComputePipeline::CreateComputeInfo createComputeInfo{};
	if (const auto& renderPassData = pipelineData["RenderPass"])
	{
		passName = renderPassData.as<std::string>();
		createComputeInfo.passName = passName;
	}

	DeserializeShaderFilepaths(pipelineData, createComputeInfo.shaderFilepathsByType);

	DeserializeDescriptorSets(pipelineData, passName, createComputeInfo.descriptorSetIndicesByType);

	ParseUniformValues(pipelineData, createComputeInfo.uniformInfo);

	return createComputeInfo;
}

BaseMaterial::CreateInfo Serializer::LoadBaseMaterial(const std::filesystem::path& filepath)
{
	PROFILER_SCOPE(__FUNCTION__);

	if (!std::filesystem::exists(filepath))
	{
		FATAL_ERROR(filepath.string() + ":Failed to load! The file doesn't exist");
	}

	std::ifstream stream(filepath);
	std::stringstream stringStream;

	stringStream << stream.rdbuf();

	stream.close();

	YAML::Node data = YAML::LoadMesh(stringStream.str());
	if (!data)
	{
		FATAL_ERROR(filepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
	}

	YAML::Node materialData = data["Basemat"];
	if (!materialData)
	{
		FATAL_ERROR(filepath.string() + ":Base material file doesn't contain identification line or is empty!");
	}

	BaseMaterial::CreateInfo createInfo{};

	for (const auto& pipelineData : materialData["Pipelines"])
	{
		Pipeline::Type type = Pipeline::Type::GRAPHICS;
		if (const auto& typeData = pipelineData["Type"])
		{
			std::string typeString = typeData.as<std::string>();
			if (typeString == "Graphics")
			{
				type = Pipeline::Type::GRAPHICS;
			}
			else if (typeString == "Compute")
			{
				type = Pipeline::Type::COMPUTE;
			}
		}

		auto checkFilepaths = [](
			const std::map<ShaderModule::Type, std::filesystem::path>& shaderFilepathsByType,
			const std::filesystem::path& debugBaseMaterialFilepath)
		{
			for (const auto& [type, shaderFilepath] : shaderFilepathsByType)
			{
				if (shaderFilepath.empty())
				{
					FATAL_ERROR("BaseMaterial: " + debugBaseMaterialFilepath.string() + " has empty shader filepath for type " + std::to_string((int)type) + "!");
				}
			}
		};

		if (type == Pipeline::Type::GRAPHICS)
		{
			createInfo.pipelineCreateGraphicsInfos.emplace_back(DeserializeGraphicsPipeline(pipelineData));
			createInfo.pipelineCreateGraphicsInfos.back().type = type;

			for (const auto& pipelineCreateGraphicsInfo : createInfo.pipelineCreateGraphicsInfos)
			{
				checkFilepaths(pipelineCreateGraphicsInfo.shaderFilepathsByType, filepath);
			}
		}
		else if (type == Pipeline::Type::COMPUTE)
		{
			createInfo.pipelineCreateComputeInfos.emplace_back(DeserializeComputePipeline(pipelineData));
			createInfo.pipelineCreateComputeInfos.back().type = type;

			for (const auto& pipelineCreateGraphicsInfo : createInfo.pipelineCreateGraphicsInfos)
			{
				checkFilepaths(pipelineCreateGraphicsInfo.shaderFilepathsByType, filepath);
			}
		}
	}

	Logger::Log("BaseMaterial:" + filepath.string() + " has been loaded!", BOLDGREEN);

	return createInfo;
}

Material::CreateInfo Serializer::LoadMaterial(const std::filesystem::path& filepath)
{
	PROFILER_SCOPE(__FUNCTION__);

	if (!std::filesystem::exists(filepath))
	{
		FATAL_ERROR(filepath.string() + ":Failed to load! The file doesn't exist");
	}

	std::ifstream stream(filepath);
	std::stringstream stringStream;

	stringStream << stream.rdbuf();

	stream.close();

	YAML::Node data = YAML::LoadMesh(stringStream.str());
	if (!data)
	{
		FATAL_ERROR(filepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
	}

	YAML::Node materialData = data["Mat"];
	if (!materialData)
	{
		FATAL_ERROR(filepath.string() + ":Base material file doesn't contain identification line or is empty!");
	}

	Material::CreateInfo createInfo;

	if (const auto& baseMaterialData = materialData["Basemat"])
	{
		const std::string filepathOrUUID = baseMaterialData.as<std::string>();
		if (std::filesystem::exists(filepathOrUUID))
		{
			createInfo.baseMaterial = filepathOrUUID;
		}
		else
		{
			createInfo.baseMaterial = Utils::FindFilepath(baseMaterialData.as<UUID>());
		}

		if (!std::filesystem::exists(createInfo.baseMaterial))
		{
			FATAL_ERROR(baseMaterialData.as<std::string>() + ":There is no such basemat!");
		}
	}

	std::unordered_map<std::string, Material::Option>& optionsByName = createInfo.optionsByName;
	for (const auto& optionData : materialData["Options"])
	{
		std::string name;
		if (const auto& nameData = optionData["Name"])
		{
			name = nameData.as<std::string>();
		}

		if (name.empty())
		{
			Logger::Warning(filepath.string() + ": Option name is empty, option will be ignored!");
			continue;
		}

		Material::Option option{};
		if (const auto& isEnabledData = optionData["IsEnabled"])
		{
			option.m_IsEnabled = isEnabledData.as<bool>();
		}

		for (const auto& activeData : optionData["Active"])
		{
			option.m_Active.emplace_back(activeData.as<std::string>());
		}

		for (const auto& activeData : optionData["Inactive"])
		{
			option.m_Inactive.emplace_back(activeData.as<std::string>());
		}

		optionsByName.emplace(name, option);
	}

	for (const auto& pipelineData : materialData["Pipelines"])
	{
		std::string renderPass = none;
		if (const auto& renderPassData = pipelineData["RenderPass"])
		{
			renderPass = renderPassData.as<std::string>();
		}
		else
		{
			FATAL_ERROR(filepath.string() + ":There is no RenderPass field!");
		}

		Pipeline::UniformInfo uniformInfo{};
		ParseUniformValues(pipelineData, uniformInfo);

		createInfo.uniformInfos.emplace(renderPass, uniformInfo);
	}

	Logger::Log("Material:" + filepath.string() + " has been loaded!", BOLDGREEN);

	return createInfo;
}

void Serializer::SerializeMaterial(const std::shared_ptr<Material>& material, bool useLog)
{
	YAML::Emitter out;

	out << YAML::BeginMap;

	out << YAML::Key << "Mat";

	out << YAML::BeginMap;

	out << YAML::Key << "Basemat" << YAML::Value << Utils::FindUuid(material->GetBaseMaterial()->GetFilepath());

	out << YAML::Key << "Options";

	out << YAML::BeginSeq;

	for (const auto& [name, option] : material->GetOptionsByName())
	{
		out << YAML::BeginMap;

		out << YAML::Key << "Name" << YAML::Value << name;
		out << YAML::Key << "IsEnabled" << YAML::Value << option.m_IsEnabled;

		out << YAML::Key << "Active";

		out << YAML::BeginSeq;

		for (const auto& active : option.m_Active)
		{
			out << active;
		}

		out << YAML::EndSeq;

		out << YAML::Key << "Inactive";

		out << YAML::BeginSeq;

		for (const auto& inactive : option.m_Inactive)
		{
			out << inactive;
		}

		out << YAML::EndSeq;

		out << YAML::EndMap;
	}

	out << YAML::EndSeq;

	out << YAML::Key << "Pipelines";

	out << YAML::BeginSeq;

	for (const auto& [passName, pipeline] : material->GetBaseMaterial()->GetPipelinesByPass())
	{
		out << YAML::BeginMap;

		out << YAML::Key << "RenderPass" << YAML::Value << passName;

		out << YAML::Key << "Uniforms";

		out << YAML::BeginSeq;

		std::optional<uint32_t> descriptorSetIndex = pipeline->GetDescriptorSetIndexByType(Pipeline::DescriptorSetIndexType::MATERIAL, passName);
		if (!descriptorSetIndex)
		{
			out << YAML::EndSeq;

			out << YAML::EndMap;

			continue;
		}
		for (const auto& binding : pipeline->GetUniformLayout(*descriptorSetIndex)->GetBindings())
		{
			out << YAML::BeginMap;

			out << YAML::Key << "Name" << YAML::Value << binding.name;

			if (binding.type == ShaderReflection::Type::COMBINED_IMAGE_SAMPLER)
			{
				const std::shared_ptr<Texture> texture = material->GetUniformWriter(passName)->GetTexture(binding.name).back();
				if (texture)
				{
					const auto uuid = Utils::FindUuid(texture->GetFilepath());
					if (uuid.IsValid())
					{
						out << YAML::Key << "Value" << YAML::Value << uuid;
					}
					else
					{
						out << YAML::Key << "Value" << YAML::Value << texture->GetFilepath();
					}
				}
			}
			else if (binding.type == ShaderReflection::Type::UNIFORM_BUFFER)
			{
				std::shared_ptr<Buffer> buffer = material->GetBuffer(binding.name);
				void* data = buffer->GetData();

				out << YAML::Key << "Values";

				out << YAML::BeginSeq;

				std::function<void(const ShaderReflection::ReflectVariable&, std::string)> saveValue = [&saveValue, &out, &data, &material]
				(const ShaderReflection::ReflectVariable& value, std::string parentName)
				{
					parentName += value.name;

					if (value.type == ShaderReflection::ReflectVariable::Type::STRUCT)
					{
						for (const auto& memberValue : value.variables)
						{
							saveValue(memberValue, parentName + ".");
						}
						return;
					}

					out << YAML::BeginMap;

					out << YAML::Key << "Name" << YAML::Value << parentName;

					if (value.type == ShaderReflection::ReflectVariable::Type::VEC2)
					{
						out << YAML::Key << "Value" << YAML::Value << Utils::GetValue<glm::vec2>(data, value.offset);
					}
					else if (value.type == ShaderReflection::ReflectVariable::Type::VEC3)
					{
						out << YAML::Key << "Value" << YAML::Value << Utils::GetValue<glm::vec3>(data, value.offset);
					}
					else if (value.type == ShaderReflection::ReflectVariable::Type::VEC4)
					{
						out << YAML::Key << "Value" << YAML::Value << Utils::GetValue<glm::vec4>(data, value.offset);
					}
					else if (value.type == ShaderReflection::ReflectVariable::Type::FLOAT)
					{
						out << YAML::Key << "Value" << YAML::Value << Utils::GetValue<float>(data, value.offset);
					}
					else if (value.type == ShaderReflection::ReflectVariable::Type::INT)
					{
						out << YAML::Key << "Value" << YAML::Value << Utils::GetValue<int>(data, value.offset);
					}
					else if (value.type == ShaderReflection::ReflectVariable::Type::TEXTURE)
					{
						const int bindlessTextureIndex = Utils::GetValue<int>(data, value.offset);
						const std::shared_ptr<Texture> texture = material->GetBindlessTexture(bindlessTextureIndex);
						if (texture)
						{
							const auto uuid = Utils::FindUuid(texture->GetFilepath());
							if (uuid.IsValid())
							{
								out << YAML::Key << "Value" << YAML::Value << uuid;
							}
							else
							{
								out << YAML::Key << "Value" << YAML::Value << texture->GetFilepath();
							}
						}
					}

					out << YAML::Key << "Type" << ShaderReflection::ConvertTypeToString(value.type);

					out << YAML::EndMap;
				};
				for (const auto& value : binding.buffer->variables)
				{
					saveValue(value, "");
				}

				out << YAML::EndSeq;
			}

			out << YAML::EndMap;
		}

		out << YAML::EndSeq;

		out << YAML::EndMap;
	}

	out << YAML::EndSeq;

	out << YAML::EndMap;

	out << YAML::EndMap;

	std::ofstream fout(material->GetFilepath());
	fout << out.c_str();
	fout.close();

	GenerateFileUUID(material->GetFilepath());

	if (useLog)
	{
		Logger::Log("Material:" + material->GetFilepath().string() + " has been saved!", BOLDGREEN);
	}
}

void Serializer::SerializeMesh(const std::filesystem::path& directory,  const std::shared_ptr<Mesh>& mesh)
{
	PROFILER_SCOPE(__FUNCTION__);

	const std::string meshName = mesh->GetName();

	size_t vertexLayoutsSize = 0;
	for (const VertexLayout& vertexLayout : mesh->GetVertexLayouts())
	{
		// Size.
		vertexLayoutsSize += sizeof(uint32_t);
		
		// Tag Size.
		vertexLayoutsSize += sizeof(uint32_t);
	
		// Tag.
		vertexLayoutsSize += vertexLayout.tag.size();
	}

	size_t dataSize = mesh->GetVertexCount() * mesh->GetVertexSize() +
		vertexLayoutsSize +
		mesh->GetIndexCount() * sizeof(uint32_t) +
		mesh->GetName().size() +
		mesh->GetFilepath().string().size() +
		mesh->GetCreateInfo().sourceFileInfo.meshName.size() +
		mesh->GetCreateInfo().sourceFileInfo.filepath.string().size() +
		sizeof(BoundingBox) +
		sizeof(Mesh::Lod) * mesh->GetLods().size() +
		11 * 4;
	// Type, Primitive Index, Source Mesh Size, Source Filepath Size, Vertex Count,
	// Vertex Size, Index Count, Mesh Size, Filepath Size, Vertex Layout Count, Lod Count.

	uint32_t offset = 0;

	uint8_t* data = new uint8_t[dataSize];

	// Type.
	{
		Utils::GetValue<uint32_t>(data, offset) = (uint32_t)mesh->GetType();
		offset += sizeof(uint32_t);
	}

	// Name.
	{
		Utils::GetValue<uint32_t>(data, offset) = static_cast<uint32_t>(meshName.size());
		offset += sizeof(uint32_t);

		memcpy(&Utils::GetValue<uint8_t>(data, offset), meshName.data(), meshName.size());
		offset += static_cast<uint32_t>(meshName.size());
	}

	// BoundingBox.
	{
		memcpy(&Utils::GetValue<uint8_t>(data, offset), &mesh->GetBoundingBox(), sizeof(BoundingBox));
		offset += sizeof(BoundingBox);
	}

	// Lods.
	{
		const auto& lods = mesh->GetLods();
		const uint32_t lodCount = lods.size();
		memcpy(&Utils::GetValue<uint8_t>(data, offset), &lodCount, sizeof(uint32_t));
		offset += sizeof(uint32_t);

		for (size_t i = 0; i < lodCount; i++)
		{
			memcpy(&Utils::GetValue<uint8_t>(data, offset), &lods[i], sizeof(Mesh::Lod));
			offset += sizeof(Mesh::Lod);
		}
	}

	// SourceFile.
	{
		const Mesh::CreateInfo::SourceFileInfo& sourceFileInfo = mesh->GetCreateInfo().sourceFileInfo;
		// Filepath.
		{
			const std::string filepath = sourceFileInfo.filepath.string();
			Utils::GetValue<uint32_t>(data, offset) = static_cast<uint32_t>(filepath.size());
			offset += sizeof(uint32_t);

			memcpy(&Utils::GetValue<uint8_t>(data, offset), filepath.data(), filepath.size());
			offset += static_cast<uint32_t>(filepath.size());
		}

		// Name.
		{
			Utils::GetValue<uint32_t>(data, offset) = static_cast<uint32_t>(sourceFileInfo.meshName.size());
			offset += sizeof(uint32_t);

			memcpy(&Utils::GetValue<uint8_t>(data, offset), sourceFileInfo.meshName.data(), sourceFileInfo.meshName.size());
			offset += static_cast<uint32_t>(sourceFileInfo.meshName.size());
		}

		// Primitive index.
		{
			Utils::GetValue<uint32_t>(data, offset) = sourceFileInfo.primitiveIndex;
			offset += sizeof(uint32_t);
		}
	}

	// Vertices.
	{
		Utils::GetValue<uint32_t>(data, offset) = mesh->GetVertexCount();
		offset += sizeof(uint32_t);
		Utils::GetValue<uint32_t>(data, offset) = mesh->GetVertexSize();
		offset += sizeof(uint32_t);

		const void* vertices = mesh->GetRawVertices();
		memcpy(&Utils::GetValue<uint8_t>(data, offset), vertices, mesh->GetVertexCount() * mesh->GetVertexSize());
		offset += mesh->GetVertexCount() * mesh->GetVertexSize();
	}

	// Vertex Layouts.
	{
		Utils::GetValue<uint32_t>(data, offset) = mesh->GetVertexLayouts().size();
		offset += sizeof(uint32_t);

		for (const VertexLayout& vertexLayout : mesh->GetVertexLayouts())
		{
			// Tag.
			{
				Utils::GetValue<uint32_t>(data, offset) = static_cast<uint32_t>(vertexLayout.tag.size());
				offset += sizeof(uint32_t);

				memcpy(&Utils::GetValue<uint8_t>(data, offset), vertexLayout.tag.data(), vertexLayout.tag.size());
				offset += static_cast<uint32_t>(vertexLayout.tag.size());
			}

			// Size.
			{
				memcpy(&Utils::GetValue<uint8_t>(data, offset), &vertexLayout.size, sizeof(uint32_t));
				offset += sizeof(uint32_t);
			}
		}
	}

	// Indices.
	{
		Utils::GetValue<uint32_t>(data, offset) = mesh->GetIndexCount();
		offset += sizeof(uint32_t);

		// Potential optimization of using 2 bit instead of 4 bit.
		std::vector<uint32_t> indices = mesh->GetRawIndices();
		memcpy(&Utils::GetValue<uint8_t>(data, offset), indices.data(), mesh->GetIndexCount() * sizeof(uint32_t));
		offset += mesh->GetIndexCount() * sizeof(uint32_t);
	}

	std::filesystem::path outMeshFilepath = directory / (meshName + FileFormats::Mesh());
	std::ofstream out(outMeshFilepath, std::ostream::binary);

	out.write((char*)data, static_cast<std::streamsize>(dataSize));

	delete[] data;

	out.close();

	GenerateFileUUID(mesh->GetFilepath());

	Logger::Log("Mesh:" + outMeshFilepath.string() + " has been saved!", BOLDGREEN);
}

Mesh::CreateInfo Serializer::DeserializeMesh(const std::filesystem::path& filepath)
{
	PROFILER_SCOPE(__FUNCTION__);

	if (!std::filesystem::exists(filepath))
	{
		Logger::Error(filepath.string() + ":Doesn't exist!");
		return {};
	}

	if (FileFormats::Mesh() != Utils::GetFileFormat(filepath))
	{
		Logger::Error(filepath.string() + ":Is not mesh asset!");
		return {};
	}

	std::ifstream in(filepath, std::ifstream::binary);

	in.seekg(0, std::ifstream::end);
	const int size = in.tellg();
	in.seekg(0, std::ifstream::beg);

	uint8_t* data = new uint8_t[size];

	in.read((char*)data, size);

	in.close();

	int offset = 0;

	Mesh::Type type;
	// Type.
	{
		type = (Mesh::Type)Utils::GetValue<uint32_t>(data, offset);
		offset += sizeof(uint32_t);
	}

	std::string meshName;
	// Name.
	{
		const uint32_t meshNameSize = Utils::GetValue<uint32_t>(data, offset);
		offset += sizeof(uint32_t);

		meshName.resize(meshNameSize);
		memcpy(meshName.data(), &Utils::GetValue<uint8_t>(data, offset), meshNameSize);
		offset += meshNameSize;
	}

	// BoundingBox.
	BoundingBox boundingBox{};
	{
		memcpy(&boundingBox, &Utils::GetValue<uint8_t>(data, offset), sizeof(BoundingBox));
		offset += sizeof(BoundingBox);
	}

	//// Lods.
	std::vector<Mesh::Lod> lods;
	{
		size_t lodCount = 0;
		memcpy(&lodCount, &Utils::GetValue<uint8_t>(data, offset), sizeof(uint32_t));
		offset += sizeof(uint32_t);

		lods.resize(lodCount);
		for (size_t i = 0; i < lodCount; i++)
		{
			memcpy(&lods[i], &Utils::GetValue<uint8_t>(data, offset), sizeof(Mesh::Lod));
			offset += sizeof(Mesh::Lod);
		}
	}

	// SourceFile.
	Mesh::CreateInfo::SourceFileInfo sourceFileInfo{};
	{
		std::string sourceFilepath;
		// Filepath.
		{
			const uint32_t sourcefilepathSize = Utils::GetValue<uint32_t>(data, offset);
			offset += sizeof(uint32_t);

			sourceFilepath.resize(sourcefilepathSize);
			memcpy(sourceFilepath.data(), &Utils::GetValue<uint8_t>(data, offset), sourcefilepathSize);
			offset += sourcefilepathSize;
		}
		sourceFileInfo.filepath = sourceFilepath;

		// MeshName.
		{
			const uint32_t meshNameSize = Utils::GetValue<uint32_t>(data, offset);
			offset += sizeof(uint32_t);

			sourceFileInfo.meshName.resize(meshNameSize);
			memcpy(sourceFileInfo.meshName.data(), &Utils::GetValue<uint8_t>(data, offset), meshNameSize);
			offset += meshNameSize;
		}

		uint32_t primitiveIndex;
		// Primitive Index.
		{
			sourceFileInfo.primitiveIndex = Utils::GetValue<uint32_t>(data, offset);
			offset += sizeof(uint32_t);
		}
	}

	// Vertices.
	uint8_t* vertices;
	const uint32_t vertexCount = Utils::GetValue<uint32_t>(data, offset);
	offset += sizeof(uint32_t);
	const uint32_t vertexSize = Utils::GetValue<uint32_t>(data, offset);
	offset += sizeof(uint32_t);
	{
		vertices = new uint8_t[vertexCount * vertexSize];
		memcpy(vertices, &Utils::GetValue<uint8_t>(data, offset), vertexCount * vertexSize);
		offset += vertexCount * vertexSize;
	}

	// Vertex Layouts.
	std::vector<VertexLayout> vertexLayouts;
	{
		const uint32_t vertexLayoutCount = Utils::GetValue<uint32_t>(data, offset);
		offset += sizeof(uint32_t);

		for (size_t i = 0; i < vertexLayoutCount; i++)
		{
			VertexLayout& vertexLayout = vertexLayouts.emplace_back();

			// Tag.
			{
				uint32_t tagSize = 0;
				memcpy(&tagSize, &Utils::GetValue<uint32_t>(data, offset), sizeof(uint32_t));
				offset += sizeof(uint32_t);

				vertexLayout.tag.resize(tagSize);
				memcpy(vertexLayout.tag.data(), &Utils::GetValue<uint8_t>(data, offset), tagSize);
				offset += tagSize;
			}

			// Size.
			{
				memcpy(&vertexLayout, &Utils::GetValue<uint32_t>(data, offset), sizeof(uint32_t));
				offset += sizeof(uint32_t);
			}
		}
	}

	std::vector<uint32_t> indices;
	// Indices.
	{
		const uint32_t indicesSize = Utils::GetValue<uint32_t>(data, offset);
		offset += sizeof(uint32_t);

		indices.resize(indicesSize);
		memcpy(indices.data(), &Utils::GetValue<uint8_t>(data, offset), indicesSize * sizeof(uint32_t));
		offset += indicesSize * sizeof(uint32_t);
	}

	delete[] data;

	Logger::Log("Mesh:" + filepath.string() + " has been loaded!", BOLDGREEN);

	Mesh::CreateInfo createInfo{};
	createInfo.filepath = filepath;
	createInfo.name = meshName;
	createInfo.sourceFileInfo = sourceFileInfo;
	createInfo.type = type;
	createInfo.indices = indices;
	createInfo.vertices = vertices;
	createInfo.vertexCount = vertexCount;
	createInfo.vertexSize = vertexSize;
	createInfo.vertexLayouts = vertexLayouts;
	createInfo.boundingBox = boundingBox;
	createInfo.lods = lods;

	return createInfo;
}

void Serializer::SerializeSkeleton(const std::shared_ptr<Skeleton>& skeleton)
{
	if (skeleton->GetFilepath().empty())
	{
		Logger::Error("Failed to save skeleton, filepath is empty!");
	}

	YAML::Emitter out;

	out << YAML::BeginMap;

	out << YAML::Key << "RootBoneIds" << YAML::Value << skeleton->GetRootBoneIds();

	out << YAML::Key << "Bones";

	out << YAML::BeginSeq;

	for (const Skeleton::Bone& bone : skeleton->GetBones())
	{
		out << YAML::BeginMap;

		out << YAML::Key << "Id" << YAML::Value << bone.id;
		out << YAML::Key << "ParentId" << YAML::Value << bone.parentId;
		out << YAML::Key << "Name" << YAML::Value << bone.name;
		out << YAML::Key << "Offset" << YAML::Value << glm::value_ptr(bone.offset);
		out << YAML::Key << "Transform" << YAML::Value << glm::value_ptr(bone.transform);
		out << YAML::Key << "ChildIds" << YAML::Value << bone.childIds;

		out << YAML::EndMap;
	}

	out << YAML::EndSeq;

	out << YAML::EndMap;

	std::ofstream fout(skeleton->GetFilepath());
	fout << out.c_str();
	fout.close();

	Logger::Log("Skeleton:" + skeleton->GetFilepath().string() + " has been saved!", BOLDGREEN);
}

std::shared_ptr<Skeleton> Serializer::DeserializeSkeleton(const std::filesystem::path& filepath)
{
	if (filepath.empty())
	{
		Logger::Error("Failed to load skeleton, filepath is empty!");
		return nullptr;
	}

	if (!std::filesystem::exists(filepath))
	{
		Logger::Error(filepath.string() + ":Failed to load skeleton! The file doesn't exist");
		return nullptr;
	}

	std::ifstream stream(filepath);
	std::stringstream stringStream;

	stringStream << stream.rdbuf();

	stream.close();

	YAML::Node data = YAML::LoadMesh(stringStream.str());
	if (!data)
	{
		Logger::Error(filepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
		return nullptr;
	}

	Skeleton::CreateInfo createInfo{};
	createInfo.name = Utils::GetFilename(filepath);
	createInfo.filepath = filepath;

	if (const auto& rootBoneIdData = data["RootBoneIds"])
	{
		createInfo.rootBoneIds = rootBoneIdData.as<std::vector<uint32_t>>();
	}

	for (const auto& boneData : data["Bones"])
	{
		Skeleton::Bone& bone =  createInfo.bones.emplace_back();
		
		if (const auto& idData = boneData["Id"])
		{
			bone.id = idData.as<uint32_t>();
		}

		if (const auto& parentIdData = boneData["ParentId"])
		{
			bone.parentId = parentIdData.as<uint32_t>();
		}

		if (const auto& nameData = boneData["Name"])
		{
			bone.name = nameData.as<std::string>();
		}

		if (const auto& offsetData = boneData["Offset"])
		{
			bone.offset = offsetData.as<glm::mat4>();
		}

		if (const auto& transformData = boneData["Transform"])
		{
			bone.transform = transformData.as<glm::mat4>();
		}

		if (const auto& childIdsData = boneData["ChildIds"])
		{
			bone.childIds = childIdsData.as<std::vector<uint32_t>>();
		}
	}

	Logger::Log("Skeleton:" + filepath.string() + " has been loaded!", BOLDGREEN);

	return MeshManager::GetInstance().CreateSkeleton(createInfo);
}

void Serializer::SerializeSkeletalAnimation(const std::shared_ptr<SkeletalAnimation>& skeletalAnimation)
{
	if (skeletalAnimation->GetFilepath().empty())
	{
		Logger::Error("Failed to save skeletal animation, filepath is empty!");
	}

	YAML::Emitter out;

	out << YAML::BeginMap;

	out << YAML::Key << "Duration" << YAML::Value << skeletalAnimation->GetDuration();

	out << YAML::Key << "Bones";

	out << YAML::BeginSeq;

	for (const auto& [name, bone] : skeletalAnimation->GetBonesByName())
	{
		out << YAML::BeginMap;

		out << YAML::Key << "Name" << YAML::Value << name;

		// Positions.
		out << YAML::Key << "Positions";

		out << YAML::BeginSeq;

		for (const auto& positionKey : bone.positions)
		{
			out << YAML::BeginMap;

			out << YAML::Key << "Time" << YAML::Value << positionKey.time;
			out << YAML::Key << "Value" << YAML::Value << positionKey.value;

			out << YAML::EndMap;
		}

		out << YAML::EndSeq;
		//

		// Rotations.
		out << YAML::Key << "Rotations";

		out << YAML::BeginSeq;

		for (const auto& rotationKey : bone.rotations)
		{
			out << YAML::BeginMap;

			out << YAML::Key << "Time" << YAML::Value << rotationKey.time;
			out << YAML::Key << "Value" << YAML::Value << rotationKey.value;

			out << YAML::EndMap;
		}

		out << YAML::EndSeq;
		//

		// Scales.
		out << YAML::Key << "Scales";

		out << YAML::BeginSeq;

		for (const auto& scaleKey : bone.scales)
		{
			out << YAML::BeginMap;

			out << YAML::Key << "Time" << YAML::Value << scaleKey.time;
			out << YAML::Key << "Value" << YAML::Value << scaleKey.value;

			out << YAML::EndMap;
		}

		out << YAML::EndSeq;
		//

		out << YAML::EndMap;
	}

	out << YAML::EndSeq;

	out << YAML::EndMap;

	std::ofstream fout(skeletalAnimation->GetFilepath());
	fout << out.c_str();
	fout.close();

	Logger::Log("Skeletal Animation:" + skeletalAnimation->GetFilepath().string() + " has been saved!", BOLDGREEN);
}

std::shared_ptr<SkeletalAnimation> Serializer::DeserializeSkeletalAnimation(const std::filesystem::path& filepath)
{
	if (filepath.empty())
	{
		Logger::Error("Failed to load skeletal animation, filepath is empty!");
		return nullptr;
	}

	if (!std::filesystem::exists(filepath))
	{
		Logger::Error(filepath.string() + ":Failed to load skeletal animation! The file doesn't exist");
		return nullptr;
	}

	std::ifstream stream(filepath);
	std::stringstream stringStream;

	stringStream << stream.rdbuf();

	stream.close();

	YAML::Node data = YAML::LoadMesh(stringStream.str());
	if (!data)
	{
		Logger::Error(filepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
		return nullptr;
	}

	SkeletalAnimation::CreateInfo createInfo{};
	createInfo.name = Utils::GetFilename(filepath);
	createInfo.filepath = filepath;

	if (const auto& durationData = data["Duration"])
	{
		createInfo.duration = durationData.as<double>();
	}

	for (const auto& boneData : data["Bones"])
	{
		std::string name;
		if (const auto& nameData = boneData["Name"])
		{
			name = nameData.as<std::string>();
		}
		else
		{
			Logger::Error(filepath.string() + ": Bone doesn't have any name, the bone will be skipped!");
			continue;
		}

		SkeletalAnimation::Bone& bone = createInfo.bonesByName[name];

		for (const auto& positionKeyData : boneData["Positions"])
		{
			auto& positionKey = bone.positions.emplace_back();
			
			if (const auto& timeData = positionKeyData["Time"])
			{
				positionKey.time = timeData.as<double>();
			}
			
			if (const auto& valueData = positionKeyData["Value"])
			{
				positionKey.value = valueData.as<glm::vec3>();
			}
		}

		for (const auto& rotationKeyData : boneData["Rotations"])
		{
			auto& rotationKey = bone.rotations.emplace_back();

			if (const auto& timeData = rotationKeyData["Time"])
			{
				rotationKey.time = timeData.as<double>();
			}

			if (const auto& valueData = rotationKeyData["Value"])
			{
				rotationKey.value = valueData.as<glm::quat>();
			}
		}

		for (const auto& scaleKeyData : boneData["Scales"])
		{
			auto& scaleKey = bone.scales.emplace_back();

			if (const auto& timeData = scaleKeyData["Time"])
			{
				scaleKey.time = timeData.as<double>();
			}

			if (const auto& valueData = scaleKeyData["Value"])
			{
				scaleKey.value = valueData.as<glm::vec3>();
			}
		}
	}

	Logger::Log("Skeletal Animation:" + filepath.string() + " has been loaded!", BOLDGREEN);

	return MeshManager::GetInstance().CreateSkeletalAnimation(createInfo);
}

void Serializer::SerializeShaderCache(const std::filesystem::path& filepath, const std::string& code)
{
	const std::filesystem::path directory = std::filesystem::path("Shaders") / "Cache";

	if (!std::filesystem::exists(directory))
	{
		std::filesystem::create_directories(directory);
	}

	const UUID uuid = Utils::FindUuid(filepath);
	if (!uuid.IsValid())
	{
		Logger::Error(filepath.string() + ":Failed to save shader cache! No uuid was found for such filepath!");
		return;
	}

	std::filesystem::path cacheFilepath = directory / uuid.ToString();
	cacheFilepath.concat(FileFormats::Spv());
	std::ofstream out(cacheFilepath, std::ostream::binary);

	const size_t lastWriteTime = std::filesystem::last_write_time(filepath).time_since_epoch().count();

	out.write((char*)&lastWriteTime, sizeof(size_t));
	out.write(code.data(), code.size());
	out.close();
}

std::string Serializer::DeserializeShaderCache(const std::filesystem::path& filepath)
{
	if (filepath.empty())
	{
		Logger::Error("Failed to load shader cache! Filepath is empty!");
		return {};
	}

	const UUID uuid = Utils::FindUuid(filepath);
	if (!uuid.IsValid())
	{
		Logger::Error(filepath.string() + ":Failed to load shader cache! No uuid was found for such filepath!");
		return {};
	}

	const std::filesystem::path directory = std::filesystem::path("Shaders") / "Cache";
	std::filesystem::path cacheFilepath = directory / uuid.ToString();
	cacheFilepath.concat(FileFormats::Spv());
	if (std::filesystem::exists(cacheFilepath))
	{
		std::ifstream in(cacheFilepath, std::ifstream::binary);

		in.seekg(0, std::ifstream::end);
		auto size = in.tellg();
		size -= sizeof(size_t);
		in.seekg(0, std::ifstream::beg);

		std::string data;
		data.resize(size);

		size_t lastWriteTime = 0;
		in.read((char*)&lastWriteTime, sizeof(size_t));

		if (lastWriteTime != std::filesystem::last_write_time(filepath).time_since_epoch().count())
		{
			return {};
		}

		in.read(data.data(), size);

		in.close();

		return data;
	}

	return {};
}

void Serializer::SerializeShaderModuleReflection(
	const std::filesystem::path& filepath,
	const ShaderReflection::ReflectShaderModule& reflectShaderModule)
{
	std::function<void(YAML::Emitter&, const std::vector<ShaderReflection::ReflectVariable>&)> serializeVariables;

	serializeVariables = [&serializeVariables](YAML::Emitter& out, const std::vector<ShaderReflection::ReflectVariable>& variables)
	{
		out << YAML::Key << "Variables";

		out << YAML::BeginSeq;

		for (const auto& variable : variables)
		{
			out << YAML::BeginMap;
		
			out << YAML::Key << "Name" << YAML::Value << variable.name;
			out << YAML::Key << "Count" << YAML::Value << variable.count;
			out << YAML::Key << "Offset" << YAML::Value << variable.offset;
			out << YAML::Key << "Size" << YAML::Value << variable.size;
			out << YAML::Key << "Type" << YAML::Value << ShaderReflection::ConvertTypeToString(variable.type);

			serializeVariables(out, variable.variables);

			out << YAML::EndMap;
		}

		out << YAML::EndSeq;
	};

	const std::filesystem::path directory = std::filesystem::path("Shaders") / "Cache";

	if (!std::filesystem::exists(directory))
	{
		std::filesystem::create_directories(directory);
	}

	const UUID uuid = Utils::FindUuid(filepath);
	if (!uuid.IsValid())
	{
		Logger::Error(filepath.string() + ":Failed to save shader reflection! No uuid was found for such filepath!");
		return;
	}

	std::filesystem::path reflectShaderModuleFilepath = directory / uuid.ToString();
	reflectShaderModuleFilepath.concat(FileFormats::Refl());

	const size_t lastWriteTime = std::filesystem::last_write_time(filepath).time_since_epoch().count();

	YAML::Emitter out;

	out << YAML::BeginMap;
	
	out << YAML::Key << "LastWriteTime" << YAML::Value << lastWriteTime;

	out << YAML::Key << "ReflectShaderModule";

	out << YAML::BeginMap;

	out << YAML::Key << "SetLayouts";

	out << YAML::BeginSeq;

	for (const auto& setLayout : reflectShaderModule.setLayouts)
	{
		out << YAML::BeginMap;

		out << YAML::Key << "Set" << YAML::Value << setLayout.set;

		out << YAML::Key << "Bindings";

		out << YAML::BeginSeq;

		for (const auto& binding : setLayout.bindings)
		{
			out << YAML::BeginMap;

			out << YAML::Key << "Name" << YAML::Value << binding.name;
			out << YAML::Key << "Binding" << YAML::Value << binding.binding;
			out << YAML::Key << "Count" << YAML::Value << binding.count;
			out << YAML::Key << "Type" << YAML::Value << (int)binding.type;

			if (binding.buffer)
			{
				out << YAML::Key << "Buffer";

				out << YAML::BeginMap;

				out << YAML::Key << "Name" << YAML::Value << binding.buffer->name;
				out << YAML::Key << "Count" << YAML::Value << binding.buffer->count;
				out << YAML::Key << "Offset" << YAML::Value << binding.buffer->offset;
				out << YAML::Key << "Size" << YAML::Value << binding.buffer->size;
				out << YAML::Key << "Type" << YAML::Value << ShaderReflection::ConvertTypeToString(binding.buffer->type);

				serializeVariables(out, binding.buffer->variables);

				out << YAML::EndMap;
			}

			out << YAML::EndMap;
		}

		out << YAML::EndSeq;

		out << YAML::EndMap;
	}

	out << YAML::EndSeq;

	out << YAML::Key << "AttributeDescriptions";

	out << YAML::BeginSeq;

	for (const auto& attributeDescription : reflectShaderModule.attributeDescriptions)
	{
		out << YAML::BeginMap;

		out << YAML::Key << "Count" << YAML::Value << attributeDescription.count;
		out << YAML::Key << "Format" << YAML::Value << (int)attributeDescription.format;
		out << YAML::Key << "Location" << YAML::Value << attributeDescription.location;
		out << YAML::Key << "Name" << YAML::Value << attributeDescription.name;
		out << YAML::Key << "Size" << YAML::Value << attributeDescription.size;

		out << YAML::EndMap;
	}

	out << YAML::EndSeq;

	out << YAML::EndMap;

	out << YAML::EndMap;

	std::ofstream fout(reflectShaderModuleFilepath);
	fout << out.c_str();
	fout.close();
}

std::optional<ShaderReflection::ReflectShaderModule> Serializer::DeserializeShaderModuleReflection(const std::filesystem::path& filepath)
{
	if (filepath.empty())
	{
		Logger::Error("Failed to load shader reflection! Filepath is empty!");
		return {};
	}

	const UUID uuid = Utils::FindUuid(filepath);
	if (!uuid.IsValid())
	{
		Logger::Error(filepath.string() + ":Failed to load shader reflection! No uuid was found for such filepath!");
		return {};
	}

	const std::filesystem::path directory = std::filesystem::path("Shaders") / "Cache";
	std::filesystem::path reflectShaderModuleFilepath = directory / uuid.ToString();
	reflectShaderModuleFilepath.concat(FileFormats::Refl());

	std::ifstream stream(reflectShaderModuleFilepath);
	std::stringstream stringStream;

	stringStream << stream.rdbuf();

	stream.close();

	YAML::Node data = YAML::LoadMesh(stringStream.str());
	if (!data)
	{
		FATAL_ERROR(reflectShaderModuleFilepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
	}

	size_t lastWriteTime = 0;
	if (const auto& lastWriteTimeData = data["LastWriteTime"])
	{
		lastWriteTime = lastWriteTimeData.as<size_t>();
	}

	if (std::filesystem::exists(reflectShaderModuleFilepath))
	{
		if (lastWriteTime != std::filesystem::last_write_time(filepath).time_since_epoch().count())
		{
			return std::nullopt;
		}
	}
	else
	{
		return std::nullopt;
	}

	std::function<void(const YAML::Node&, std::vector<ShaderReflection::ReflectVariable>&)> deserializeVariables;

	deserializeVariables = [&deserializeVariables](const YAML::Node& node, std::vector<ShaderReflection::ReflectVariable>& variables)
	{
		for (const auto& variableData : node)
		{
			ShaderReflection::ReflectVariable& reflectVariable = variables.emplace_back();

			if (const auto& nameData = variableData["Name"])
			{
				reflectVariable.name = nameData.as<std::string>();
			}

			if (const auto& countData = variableData["Count"])
			{
				reflectVariable.count = countData.as<uint32_t>();
			}

			if (const auto& offsetData = variableData["Offset"])
			{
				reflectVariable.offset = offsetData.as<uint32_t>();
			}

			if (const auto& sizeData = variableData["Size"])
			{
				reflectVariable.size = sizeData.as<uint32_t>();
			}

			if (const auto& typeData = variableData["Type"])
			{
				reflectVariable.type = ShaderReflection::ConvertStringToType(typeData.as<std::string>());
			}

			if (const auto& variablesData = variableData["Variables"])
			{
				deserializeVariables(variablesData, reflectVariable.variables);
			}
		}
	};

	ShaderReflection::ReflectShaderModule reflectShaderModule{};
	if (const auto& reflectShaderModuleData = data["ReflectShaderModule"])
	{
		for (const auto& setLayoutData : reflectShaderModuleData["SetLayouts"])
		{
			ShaderReflection::ReflectDescriptorSetLayout& setLayout = reflectShaderModule.setLayouts.emplace_back();
			
			if (const auto& setData = setLayoutData["Set"])
			{
				setLayout.set = setData.as<uint32_t>();
			}

			for (const auto& bindingData : setLayoutData["Bindings"])
			{
				ShaderReflection::ReflectDescriptorSetBinding& binding = setLayout.bindings.emplace_back();

				if (const auto& nameData = bindingData["Name"])
				{
					binding.name = nameData.as<std::string>();
				}

				if (const auto& locationData = bindingData["Binding"])
				{
					binding.binding = locationData.as<uint32_t>();
				}

				if (const auto& countData = bindingData["Count"])
				{
					binding.count = countData.as<uint32_t>();
				}

				if (const auto& typeData = bindingData["Type"])
				{
					binding.type = (ShaderReflection::Type)typeData.as<int>();
				}

				if (const auto& bufferData = bindingData["Buffer"])
				{
					ShaderReflection::ReflectVariable reflectVariable{};

					if (const auto& nameData = bufferData["Name"])
					{
						reflectVariable.name = nameData.as<std::string>();
					}

					if (const auto& countData = bufferData["Count"])
					{
						reflectVariable.count = countData.as<uint32_t>();
					}

					if (const auto& offsetData = bufferData["Offset"])
					{
						reflectVariable.offset = offsetData.as<uint32_t>();
					}

					if (const auto& sizeData = bufferData["Size"])
					{
						reflectVariable.size = sizeData.as<uint32_t>();
					}

					if (const auto& typeData = bufferData["Type"])
					{
						reflectVariable.type = ShaderReflection::ConvertStringToType(typeData.as<std::string>());
					}

					if (const auto& variablesData = bufferData["Variables"])
					{
						deserializeVariables(variablesData, reflectVariable.variables);
					}

					binding.buffer = reflectVariable;
				}
			}
		}

		for (const auto& attributeDescriptionData : reflectShaderModuleData["AttributeDescriptions"])
		{
			ShaderReflection::AttributeDescription& attributeDescription = reflectShaderModule.attributeDescriptions.emplace_back();

			if (const auto& countData = attributeDescriptionData["Count"])
			{
				attributeDescription.count = countData.as<uint32_t>();
			}

			if (const auto& formatData = attributeDescriptionData["Format"])
			{
				attributeDescription.format = (Format)formatData.as<int>();
			}

			if (const auto& locationData = attributeDescriptionData["Location"])
			{
				attributeDescription.location = locationData.as<uint32_t>();
			}

			if (const auto& nameData = attributeDescriptionData["Name"])
			{
				attributeDescription.name = nameData.as<std::string>();
			}

			if (const auto& sizeData = attributeDescriptionData["Size"])
			{
				attributeDescription.size = sizeData.as<uint32_t>();
			}
		}
	}

	return reflectShaderModule;
}

Serializer::ImportInfo Serializer::GetImportInfo(const std::filesystem::path& filepath)
{
	const std::filesystem::path directory = filepath.parent_path();

	auto data = fastgltf::GltfDataBuffer::FromPath(filepath);
	if (data.error() != fastgltf::Error::None)
	{
		Logger::Error(std::format("Failed to load intermediate {}!", filepath.string()));
		return {};
	}

	fastgltf::Parser parser{};
	auto asset = parser.loadGltf(data.get(), directory, fastgltf::Options::None);

	if (asset.error() != fastgltf::Error::None)
	{
		Logger::Error(std::format("Failed to load intermediate {}!", filepath.string()));
		return {};
	}

	fastgltf::Asset& gltfAsset = asset.get();

	ImportInfo importInfo{};
	importInfo.filepath = filepath;

	const std::string filename = Utils::GetFilename(filepath.string());

	for (size_t i = 0; i < gltfAsset.materials.size(); i++)
	{
		if (gltfAsset.materials[i].name.empty())
		{
			gltfAsset.materials[i].name = std::format("{}Material{}", filename, i);
		}

		importInfo.materials.emplace_back(gltfAsset.materials[i].name);
	}

	for (const auto& mesh : gltfAsset.meshes)
	{
		size_t primitiveIndex = 0;
		for (const auto& primitive : mesh.primitives)
		{
			std::string meshName = mesh.name.c_str();

			if (meshName.empty())
			{
				meshName = std::format("{}Mesh", filename);
			}

			if (mesh.primitives.size() > 1)
			{
				meshName += std::format("{}", primitiveIndex);
			}

			importInfo.meshes.emplace_back(meshName);

			primitiveIndex++;
		}
	}

	for (size_t i = 0; i < gltfAsset.skins.size(); i++)
	{
		if (gltfAsset.skins[i].name.empty())
		{
			gltfAsset.skins[i].name = std::format("{}Skin{}", filename, i);
		}

		importInfo.skeletons.emplace_back(gltfAsset.skins[i].name);
	}

	for (size_t i = 0; i < gltfAsset.animations.size(); i++)
	{
		if (gltfAsset.animations[i].name.empty())
		{
			gltfAsset.animations[i].name = std::format("{}Animation{}", filename, i);
		}

		importInfo.animations.emplace_back(gltfAsset.animations[i].name);
	}

	for (size_t i = 0; i < gltfAsset.scenes.size(); i++)
	{
		if (gltfAsset.scenes[i].name.empty())
		{
			gltfAsset.scenes[i].name = std::format("{}Scene{}", filename, i);
		}

		importInfo.prefabs.emplace_back(gltfAsset.scenes[i].name);
	}

	return importInfo;
}

std::unordered_map<std::shared_ptr<Material>, std::vector<std::shared_ptr<Mesh>>>
Serializer::LoadIntermediate(
	const ImportOptions options,
	std::string& workName,
	float& workStatus)
{
	workName = "Loading " + options.filepath.string();

	std::filesystem::path directory = options.filepath.parent_path();
	const std::filesystem::path texturesDirectory = directory;
	const std::string filename = Utils::GetFilename(options.filepath.string());

	auto data = fastgltf::GltfDataBuffer::FromPath(options.filepath);
	if (data.error() != fastgltf::Error::None)
	{
		Logger::Error(std::format("Failed to load intermediate {}!", options.filepath.string()));
		return {};
	}

	fastgltf::Parser parser{};
	auto asset = parser.loadGltf(data.get(), directory,
		fastgltf::Options::GenerateMeshIndices | fastgltf::Options::LoadExternalBuffers);

	if (asset.error() != fastgltf::Error::None)
	{
		Logger::Error(std::format("Failed to load intermediate {}!", options.filepath.string()));
		return {};
	}

	fastgltf::Asset& gltfAsset = asset.get();

	const float maxWorkStatus = gltfAsset.materials.size() + gltfAsset.meshes.size() + gltfAsset.animations.size();
	float currentWorkStatus = 0.0f;

	if (options.createFolder)
	{
		directory = Utils::EraseFileFormat(options.filepath);
		if (!std::filesystem::exists(directory))
		{
			std::filesystem::create_directory(directory);
		}
	}

	std::unordered_map<size_t, std::shared_ptr<Material>> materialsByIndex;
	if (options.materials)
	{
		workName = "Generating Materials";

		std::vector<std::future<void>> futures;
		std::mutex mutex;
		for (size_t materialIndex = 0; materialIndex < gltfAsset.materials.size(); materialIndex++)
		{
			if (gltfAsset.materials[materialIndex].name.empty())
			{
				gltfAsset.materials[materialIndex].name = std::format("{}Material{}", filename, materialIndex);
			}

			futures.emplace_back(ThreadPool::GetInstance().EnqueueAsyncFuture([
					&gltfMaterial = gltfAsset.materials[materialIndex],
					texturesDirectory,
					directory,
					materialIndex,
					maxWorkStatus,
					&mutex,
					&gltfAsset,
					&materialsByIndex,
					&workStatus,
					&currentWorkStatus]()
			{
				std::shared_ptr<Material> material = GenerateMaterial(gltfAsset, gltfAsset.materials[materialIndex], texturesDirectory, directory);

				std::lock_guard<std::mutex> lock(mutex);
				materialsByIndex[materialIndex] = material;

				workStatus = currentWorkStatus++ / maxWorkStatus;
			}));
		}

		for (auto& future : futures)
		{
			future.get();
		}
	}

	std::vector<std::shared_ptr<Skeleton>> skeletonsByIndex;
	if (options.skeletons)
	{
		workName = "Generating Skeletons";
		for (size_t i = 0; i < gltfAsset.skins.size(); i++)
		{
			auto& skin = gltfAsset.skins[i];
			if (skin.name.empty())
			{
				skin.name = std::format("{}Skeleton{}", filename, i);
			}

			skeletonsByIndex.emplace_back(GenerateSkeleton(gltfAsset, skin, directory));
		}
	}

	std::vector<std::vector<std::shared_ptr<Mesh>>> meshesByIndex;
	std::unordered_map<std::shared_ptr<Mesh>, std::shared_ptr<Material>> materialsByMeshes;
	if (options.meshes.import)
	{
		workName = "Generating Meshes";

		std::vector<std::future<void>> futures;
		std::mutex mutex;
		for (size_t meshIndex = 0; meshIndex < gltfAsset.meshes.size(); meshIndex++)
		{
			uint32_t primitiveIndex = 0;
			auto& primitivesByIndex = meshesByIndex.emplace_back();

			for (const auto& primitive : gltfAsset.meshes[meshIndex].primitives)
			{
				std::string meshName = gltfAsset.meshes[meshIndex].name.c_str();
				if (meshName.empty())
				{
					meshName = std::format("{}Mesh", filename);
				}

				Mesh::CreateInfo::SourceFileInfo sourceFileInfo{};
				sourceFileInfo.filepath = options.filepath;
				sourceFileInfo.meshName = meshName;
				sourceFileInfo.primitiveIndex = primitiveIndex;

				if (gltfAsset.meshes[meshIndex].primitives.size() > 1)
				{
					std::lock_guard<std::mutex> lock(mutex);
					meshName += std::format("{}", primitiveIndex++);
				}

				futures.emplace_back(ThreadPool::GetInstance().EnqueueAsyncFuture([
					&gltfAsset,
					&materialsByMeshes,
					&materialsByIndex,
					&meshesByIndex,
					&mutex,
					&directory,
					&options,
					&workStatus,
					&currentWorkStatus,
					sourceFileInfo,
					meshName,
					maxWorkStatus,
					meshIndex]()
				{
					std::optional<Mesh::CreateInfo> meshCreateInfo = GenerateMesh(
						sourceFileInfo,
						gltfAsset,
						gltfAsset.meshes[meshIndex].primitives[sourceFileInfo.primitiveIndex],
						meshName,
						directory,
						options.meshes);

					std::shared_ptr<Mesh> mesh = nullptr;

					if (meshCreateInfo)
					{
						mesh = MeshManager::GetInstance().CreateMesh(*meshCreateInfo);
						SerializeMesh(mesh->GetFilepath().parent_path(), mesh);
					}

					std::lock_guard<std::mutex> lock(mutex);
					meshesByIndex[meshIndex].emplace_back(mesh);
					materialsByMeshes[mesh] = materialsByIndex[*(gltfAsset.meshes[meshIndex].primitives[sourceFileInfo.primitiveIndex].materialIndex)];
				
					workStatus = currentWorkStatus++ / maxWorkStatus;
				}));
			}
		}

		for (auto& future : futures)
		{
			future.get();
		}
	}

	std::vector<std::shared_ptr<SkeletalAnimation>> animations;
	if (options.animations)
	{
		workName = "Generating Animations";
		for (size_t i = 0; i < gltfAsset.animations.size(); i++)
		{
			fastgltf::Animation& animation = gltfAsset.animations[i];
			if (animation.name.empty())
			{
				animation.name = std::format("{}Animation{}", filename, i);
			}

			animations.emplace_back(GenerateAnimation(gltfAsset, animation, directory));
	
			workStatus = currentWorkStatus++ / maxWorkStatus;
		}
	}

	if (options.prefabs)
	{
		for (size_t i = 0; i < gltfAsset.scenes.size(); i++)
		{
			fastgltf::Scene& scene = gltfAsset.scenes[i];
			if (scene.name.empty())
			{
				scene.name = std::format("{}Scene{}", filename, i);
			}

			for (const auto& nodeIndex : scene.nodeIndices)
			{
				workName = "Generating Prefab " + scene.name;

				std::shared_ptr<Scene> generateGameObjectScene = SceneManager::GetInstance().Create("GenerateGameObject", "GenerateGameObject");
				std::shared_ptr<Entity> root = GenerateEntity(
					gltfAsset,
					gltfAsset.nodes[nodeIndex],
					generateGameObjectScene,
					meshesByIndex,
					skeletonsByIndex,
					materialsByMeshes,
					animations);

				std::filesystem::path prefabFilepath = directory / scene.name;
				prefabFilepath.replace_extension(FileFormats::Prefab());
				SerializePrefab(prefabFilepath, root);

				// TODO: Very slow! Maybe to rewrite!
				for (const auto& [name, scene] : SceneManager::GetInstance().GetScenes())
				{
					if (scene->GetTag() == generateGameObjectScene->GetTag())
					{
						continue;
					}

					auto entities = scene->GetEntities();
					for (auto& entity : entities)
					{
						if (entity->IsPrefab() && entity->GetPrefabFilepathUUID() == root->GetPrefabFilepathUUID())
						{
							const auto prefab = DeserializePrefab(prefabFilepath, scene);
							prefab->GetComponent<Transform>().Copy(entity->GetComponent<Transform>());
							scene->DeleteEntity(entity);
						}
					}
				}

				SceneManager::GetInstance().Delete(generateGameObjectScene);
			}
		}
	}
	
	materialsByMeshes.clear();

	for (auto& [index, material] : materialsByIndex)
	{
		MaterialManager::GetInstance().DeleteMaterial(material);
	}
	materialsByIndex.clear();

	for (auto& primitives : meshesByIndex)
	{
		for (auto& mesh : primitives)
		{
			MeshManager::GetInstance().DeleteMesh(mesh);
		}
	}
	meshesByIndex.clear();

	// TODO: Remove or put here something, now the result is unused.
	return {};
}

std::shared_ptr<Texture> Serializer::LoadGltfTexture(
	const fastgltf::Asset& gltfAsset,
	const fastgltf::Texture& gltfTexture,
	const std::filesystem::path& texturesDirectory,
	const std::filesystem::path& directory,
	const std::string& debugName,
	std::optional<Texture::Meta> meta)
{
	if (!gltfTexture.imageIndex.has_value())
	{
		return nullptr;
	}

	const auto& image = gltfAsset.images[*gltfTexture.imageIndex];

	if (const auto* filepath = std::get_if<fastgltf::sources::URI>(&image.data))
	{
		if (meta)
		{
			meta->filepath = texturesDirectory / filepath->uri.fspath().concat(FileFormats::Meta());
			
			const auto existingMeta = DeserializeTextureMeta(meta->filepath);
			if (existingMeta)
			{
				meta->uuid = existingMeta->uuid;
			}

			if (!meta->uuid.IsValid())
			{
				meta->uuid = UUID();
			}

			SerializeTextureMeta(*meta);
		}
		return AsyncAssetLoader::GetInstance().SyncLoadTexture(texturesDirectory / filepath->uri.fspath());
	}
	if (const auto* view = std::get_if<fastgltf::sources::BufferView>(&image.data))
	{
		Logger::FatalError("Bindless not supported yet!");

		const auto& bufferView = gltfAsset.bufferViews[view->bufferViewIndex];
		const auto& buffer = gltfAsset.buffers[bufferView.bufferIndex];

		if (const auto* array = std::get_if<fastgltf::sources::Array>(&buffer.data))
		{
			Texture::CreateInfo createInfo{};
			createInfo.name = bufferView.name.empty() ? debugName : bufferView.name.c_str();
			createInfo.filepath = directory / createInfo.name;

			int channels = 0;
			stbi_set_flip_vertically_on_load(true);
			createInfo.data = stbi_load_from_memory(
				reinterpret_cast<const stbi_uc*>(array->bytes.data() + bufferView.byteOffset),
				static_cast<int>(bufferView.byteLength),
				&createInfo.size.x,
				&createInfo.size.y,
				&channels,
				STBI_rgb_alpha);

			createInfo.instanceSize = sizeof(uint8_t) * 4;

			if (!createInfo.data)
			{
				Logger::Error(std::format("{}:Failed to load embedded texture!", createInfo.name));
				return nullptr;
			}

			stbi_flip_vertically_on_write(true);
			stbi_write_png(
				createInfo.filepath.string().c_str(),
				createInfo.size.x,
				createInfo.size.y,
				channels,
				createInfo.data,
				createInfo.size.x * createInfo.instanceSize);

			if (!meta)
			{
				meta = Texture::Meta();
			}

			if (!meta->uuid.IsValid())
			{
				meta->uuid = UUID();
			}

			meta->filepath = createInfo.filepath;
			meta->filepath.concat(FileFormats::Meta());

			SerializeTextureMeta(*meta);

			createInfo.aspectMask = Texture::AspectMask::COLOR;
			createInfo.format = meta->srgb ? Format::R8G8B8A8_SRGB : Format::R8G8B8A8_UNORM;
			createInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_DST };

			return TextureManager::GetInstance().Create(createInfo);
		}
	}

	return nullptr;
}

std::optional<Mesh::CreateInfo> Serializer::GenerateMesh(
	const Mesh::CreateInfo::SourceFileInfo& sourceFileInfo,
	const fastgltf::Asset& gltfAsset,
	const fastgltf::Primitive& gltfPrimitive,
	const std::string& name,
	const std::filesystem::path& directory,
	const ImportOptions::MeshOptions& options)
{
	std::string defaultMeshName = name;
	std::string meshName = defaultMeshName;
	meshName.erase(std::remove(meshName.begin(), meshName.end(), ':'), meshName.end());
	meshName.erase(std::remove(meshName.begin(), meshName.end(), '|'), meshName.end());

	std::filesystem::path meshFilepath = directory / (meshName + FileFormats::Mesh());

	//int containIndex = 0;
	//while (MeshManager::GetInstance().GetMesh(meshFilepath))
	//{
	//	meshName = defaultMeshName + "_" + std::to_string(containIndex);
	//	meshFilepath.replace_filename(meshName + FileFormats::Mesh());
	//	containIndex++;
	//}

	const auto& primitive = gltfPrimitive;

	if (primitive.type != fastgltf::PrimitiveType::Triangles)
	{
		Logger::Error(std::format("{}: Primitive type is not supported!", meshName));
		return std::nullopt;
	}

	auto positionAttribute = primitive.findAttribute("POSITION");
	auto uvAttribute = primitive.findAttribute("TEXCOORD_0");
	auto normalAttribute = primitive.findAttribute("NORMAL");
	auto colorAttribute = primitive.findAttribute("COLOR_0");
	auto tangentAttribute = primitive.findAttribute("TANGENT");
	auto jointAttribute = primitive.findAttribute("JOINTS_0");
	auto weightAttribute = primitive.findAttribute("WEIGHTS_0");

	const auto& positionAccessor = positionAttribute != primitive.attributes.end()
		? &gltfAsset.accessors[positionAttribute->accessorIndex] : nullptr;
	const auto& uvAccessor = uvAttribute != primitive.attributes.end()
		? &gltfAsset.accessors[uvAttribute->accessorIndex] : nullptr;
	const auto& normalAccessor = normalAttribute != primitive.attributes.end()
		? &gltfAsset.accessors[normalAttribute->accessorIndex] : nullptr;
	const auto& tangentAccessor = tangentAttribute != primitive.attributes.end()
		? &gltfAsset.accessors[tangentAttribute->accessorIndex] : nullptr;
	const auto& colorAccessor = colorAttribute != primitive.attributes.end()
		? &gltfAsset.accessors[colorAttribute->accessorIndex] : nullptr;
	const auto& jointAccessor = jointAttribute != primitive.attributes.end()
		? &gltfAsset.accessors[jointAttribute->accessorIndex] : nullptr;
	const auto& weightAccessor = weightAttribute != primitive.attributes.end()
		? &gltfAsset.accessors[weightAttribute->accessorIndex] : nullptr;

	if (positionAttribute == primitive.attributes.end())
	{
		Logger::Error(std::format("{}: Mesh doesn't have POSITION attribute!", name));
		return std::nullopt;
	}

	const size_t vertexCount = positionAccessor->count;
	const bool skinned = options.skinned && jointAccessor && weightAccessor;

	void* vertices = skinned ? (void*)new VertexDefaultSkinned[vertexCount] : (void*)new VertexDefault[vertexCount];
	std::vector<uint32_t> indices;

	fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
		gltfAsset,
		*positionAccessor,
		[&](fastgltf::math::fvec3 position, std::size_t index)
		{
			void* vertex = skinned ? (void*)&static_cast<VertexDefaultSkinned*>(vertices)[index] : (void*)&static_cast<VertexDefault*>(vertices)[index];
			
			static_cast<VertexDefault*>(vertex)->position = { position.x(), position.y(), position.z() };
			static_cast<VertexDefault*>(vertex)->uv = { 0.0f, 0.0f };
			static_cast<VertexDefault*>(vertex)->normal = { 0.0f, 0.0f, 0.0f };
			static_cast<VertexDefault*>(vertex)->tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
			static_cast<VertexDefault*>(vertex)->color = 0xffffffff;

			if (skinned)
			{
				static_cast<VertexDefaultSkinned*>(vertex)->boneIds = { 0, 0, 0, 0 };
				static_cast<VertexDefaultSkinned*>(vertex)->weights = { 0.0f, 0.0f, 0.0f, 0.0f };
			}
		});

	if (uvAccessor)
	{
		fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
			gltfAsset,
			*uvAccessor,
			[&](fastgltf::math::fvec2 uv, std::size_t index)
			{
				void* vertex = skinned ? (void*)&static_cast<VertexDefaultSkinned*>(vertices)[index] : (void*)&static_cast<VertexDefault*>(vertices)[index];
				static_cast<VertexDefault*>(vertex)->uv =
				{
					options.flipUV.x ? 1.0f - uv.x() : uv.x(),
					options.flipUV.y ? 1.0f - uv.y() : uv.y()
				};
			});
	}

	if (normalAccessor)
	{
		fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
			gltfAsset,
			*normalAccessor,
			[&](fastgltf::math::fvec3 normal, std::size_t index)
			{
				void* vertex = skinned ? (void*)&static_cast<VertexDefaultSkinned*>(vertices)[index] : (void*)&static_cast<VertexDefault*>(vertices)[index];
				static_cast<VertexDefault*>(vertex)->normal = { normal.x(), normal.y(), normal.z() };
			});
	}

	if (colorAccessor)
	{
		if (skinned)
		{
			ProcessColors(gltfAsset, colorAccessor, vertices, sizeof(VertexDefaultSkinned), offsetof(VertexDefaultSkinned, color));
		}
		else
		{
			ProcessColors(gltfAsset, colorAccessor, vertices, sizeof(VertexDefault), offsetof(VertexDefault, color));
		}
	}

	if (primitive.indicesAccessor)
	{
		const auto& indexAccessor = gltfAsset.accessors[*primitive.indicesAccessor];
		if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedInt)
		{
			indices.resize(indexAccessor.count);
			fastgltf::iterateAccessorWithIndex<uint32_t>(
				gltfAsset,
				indexAccessor,
				[&](uint32_t idx, std::size_t index)
				{
					indices[index] = idx;
				});
		}
		else if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort)
		{
			indices.resize(indexAccessor.count);
			fastgltf::iterateAccessorWithIndex<uint16_t>(
				gltfAsset,
				indexAccessor,
				[&](uint16_t idx, std::size_t index)
				{
					indices[index] = idx;
				});
		}
		else if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedByte)
		{
			indices.resize(indexAccessor.count);
			fastgltf::iterateAccessorWithIndex<uint8_t>(
				gltfAsset,
				indexAccessor,
				[&](uint8_t idx, std::size_t index)
				{
					indices[index] = idx;
				});
		}
		else
		{
			Logger::Error(std::format("{}: Index type is not supported!", meshName));
		}
	}
	else
	{
		indices.resize(vertexCount);
		for (size_t i = 0; i < vertexCount; ++i)
		{
			indices[i] = static_cast<uint32_t>(i);
		}
	}

	if (tangentAccessor)
	{
		fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
			gltfAsset,
			*tangentAccessor,
			[&](fastgltf::math::fvec4 tangent, std::size_t index)
			{
				void* vertex = skinned ? (void*)&static_cast<VertexDefaultSkinned*>(vertices)[index] : (void*)&static_cast<VertexDefault*>(vertices)[index];
				static_cast<VertexDefault*>(vertex)->tangent = { tangent.x(), tangent.y(), tangent.z(), tangent.w() };
			});
	}
	else
	{
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			const uint32_t i0 = indices[i];
			const uint32_t i1 = indices[i + 1];
			const uint32_t i2 = indices[i + 2];

			void* v0 = skinned ? (void*)&static_cast<VertexDefaultSkinned*>(vertices)[i0] : (void*)&static_cast<VertexDefault*>(vertices)[i0];
			void* v1 = skinned ? (void*)&static_cast<VertexDefaultSkinned*>(vertices)[i1] : (void*)&static_cast<VertexDefault*>(vertices)[i1];
			void* v2 = skinned ? (void*)&static_cast<VertexDefaultSkinned*>(vertices)[i2] : (void*)&static_cast<VertexDefault*>(vertices)[i2];

			const glm::vec3 pos0 = static_cast<VertexDefault*>(v0)->position;
			const glm::vec3 pos1 = static_cast<VertexDefault*>(v1)->position;
			const glm::vec3 pos2 = static_cast<VertexDefault*>(v2)->position;

			const glm::vec2 uv0 = static_cast<VertexDefault*>(v0)->uv;
			const glm::vec2 uv1 = static_cast<VertexDefault*>(v1)->uv;
			const glm::vec2 uv2 = static_cast<VertexDefault*>(v2)->uv;

			const glm::vec3 edge1 = pos1 - pos0;
			const glm::vec3 edge2 = pos2 - pos0;

			const glm::vec2 deltaUV1 = uv1 - uv0;
			const glm::vec2 deltaUV2 = uv2 - uv0;

			float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

			glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
			tangent = glm::normalize(tangent);

			glm::vec3 bitangent = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * f;
			bitangent = glm::normalize(bitangent);

			const float handedness = (glm::dot(glm::cross(static_cast<VertexDefault*>(v0)->normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;

			static_cast<VertexDefault*>(v0)->tangent = glm::vec4(tangent, handedness);
			static_cast<VertexDefault*>(v1)->tangent = glm::vec4(tangent, handedness);
			static_cast<VertexDefault*>(v2)->tangent = glm::vec4(tangent, handedness);
		}
	}

	if (skinned && jointAccessor)
	{
		if (jointAccessor->componentType == fastgltf::ComponentType::UnsignedByte)
		{
			fastgltf::iterateAccessorWithIndex<fastgltf::math::u8vec4>(gltfAsset, *jointAccessor,
			[&](fastgltf::math::u8vec4 value, size_t index)
			{
				static_cast<VertexDefaultSkinned*>(vertices)[index].boneIds = { value.x(), value.y(), value.z(), value.w() };
			});
		}
		else if (jointAccessor->componentType == fastgltf::ComponentType::UnsignedShort)
		{
			fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec4>(gltfAsset, *jointAccessor,
			[&](fastgltf::math::u16vec4 value, size_t index)
			{
				static_cast<VertexDefaultSkinned*>(vertices)[index].boneIds = { value.x(), value.y(), value.z(), value.w() };
			});
		}
	}

	if (skinned && weightAccessor)
	{
		fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltfAsset, *weightAccessor,
		[&](fastgltf::math::fvec4 value, size_t index)
		{
			static_cast<VertexDefaultSkinned*>(vertices)[index].weights = { value.x(), value.y(), value.z(), value.w() };
			for (size_t i = 0; i < 4; i++)
			{
				if (value[i] == 0.0f)
				{
					static_cast<VertexDefaultSkinned*>(vertices)[index].boneIds[i] = -1;
				}
			}
		});
	}

	Mesh::Type meshType = skinned ? Mesh::Type::SKINNED : Mesh::Type::STATIC;
	const size_t vertexSize = meshType == Mesh::Type::STATIC ? sizeof(VertexDefault) : sizeof(VertexDefaultSkinned);
	const size_t lodCount = options.lodCount;

	auto generateLodTargets = [](size_t originalIndexCount, int lodCount, float startRatio = 1.0f, float endRatio = 0.2f)
	{
		std::vector<size_t> targets;
		targets.reserve(lodCount);

		if (lodCount == 1)
		{
			targets.push_back(originalIndexCount);
			return targets;
		}

		for (int i = 0; i < lodCount; ++i)
		{
			const float t = static_cast<float>(i) / (lodCount - 1);
			const float ratio = startRatio * std::pow(endRatio / startRatio, t);
			size_t target = static_cast<size_t>(originalIndexCount * ratio);

			target = std::max<size_t>(target, 3);

			if (i > 0 && target >= targets.back())
			{
				target = std::max<size_t>(targets.back() / 2, 3);
			}

			target = std::min(target, originalIndexCount);
			target = target - (target % 3);
			targets.push_back(target);
		}

		return targets;
	};


	auto generateLODDistanceThresholds = [](
		size_t lodCount,
		float minDistance,
		float maxDistance,
		float curvePower = 2.0f)
	{
		std::vector<float> thresholds;

		if (lodCount <= 1)
		{
			thresholds.push_back(minDistance);
			return thresholds;
		}

		thresholds.resize(lodCount);

		thresholds[0] = 0.0f;

		for (size_t i = 1; i < lodCount; ++i)
		{
			float t = static_cast<float>(i + 1) / lodCount;
			float ratio = std::pow(t, curvePower);
			thresholds[i] = minDistance + (maxDistance - minDistance) * ratio;
		}

		return thresholds;
	};

	std::vector<size_t> lodsTargetIndexCount = generateLodTargets(indices.size(), lodCount, 1.0f, options.minIndexCountFactor);
	std::vector<std::vector<uint32_t>> lodIndices(lodCount);
	std::vector<Mesh::Lod> lods(lodCount);
	std::vector<float> distanceThresholds = generateLODDistanceThresholds(lodCount, options.distanceMinMax.x, options.distanceMinMax.y);
	size_t finalIndexCount = 0;
	for (size_t i = 0; i < lodCount; i++)
	{
		lodIndices[i].resize(indices.size());
		size_t indexCount = meshopt_simplify(
			lodIndices[i].data(),
			indices.data(),
			indices.size(),
			(float*)vertices,
			vertexCount,
			vertexSize,
			lodsTargetIndexCount[i],
			options.targetError,
			0,
			nullptr);

		lodIndices[i].resize(indexCount);
		lods[i].indexCount = indexCount;
		lods[i].indexOffset = i == 0 ? 0 : lods[i - 1].indexOffset + lods[i - 1].indexCount;
		lods[i].distanceThreshold = distanceThresholds[i];
		finalIndexCount += indexCount;
	}

	indices.resize(finalIndexCount);
	for (size_t i = 0; i < lodCount; i++)
	{
		memcpy(&indices[lods[i].indexOffset], lodIndices[i].data(), lods[i].indexCount * sizeof(uint32_t));
	}

	Mesh::CreateInfo createInfo{};
	if (meshType == Mesh::Type::STATIC)
	{
		createInfo.filepath = std::move(meshFilepath);
		createInfo.name = std::move(meshName);
		createInfo.sourceFileInfo = sourceFileInfo;
		createInfo.type = meshType;
		createInfo.indices = std::move(indices);
		createInfo.vertices = vertices;
		createInfo.vertexCount = vertexCount;
		createInfo.vertexSize = vertexSize;
		createInfo.lods = lods;
		createInfo.vertexLayouts =
		{
			VertexLayout(sizeof(VertexPosition), "Position"),
			VertexLayout(sizeof(VertexNormal), "Normal"),
			VertexLayout(sizeof(VertexColor), "Color"),
		};
	}
	else if (meshType == Mesh::Type::SKINNED)
	{
		createInfo.filepath = std::move(meshFilepath);
		createInfo.name = std::move(meshName);
		createInfo.sourceFileInfo = sourceFileInfo;
		createInfo.type = meshType;
		createInfo.indices = std::move(indices);
		createInfo.vertices = vertices;
		createInfo.vertexCount = vertexCount;
		createInfo.vertexSize = vertexSize;
		createInfo.lods = lods;
		createInfo.vertexLayouts =
		{
			VertexLayout(sizeof(VertexPosition), "Position"),
			VertexLayout(sizeof(VertexNormal), "Normal"),
			VertexLayout(sizeof(VertexColor), "Color"),
			VertexLayout(sizeof(VertexSkinned), "Bones")
		};
	}
	else
	{
		Logger::Error(std::format("Failed to generate mesh {} at {}, unsupported mesh type!",
			meshName, sourceFileInfo.filepath.string()));
		return std::nullopt;
	}
	
	return createInfo;
}

//std::optional<Mesh::CreateInfo> Serializer::ReimportMesh(
//	const std::filesystem::path& filepath,
//	const std::string& name)
//{
//	auto data = fastgltf::GltfDataBuffer::FromPath(filepath);
//	if (data.error() != fastgltf::Error::None)
//	{
//		Logger::Error(std::format("Failed to load intermediate {}!", filepath.string()));
//		return {};
//	}
//
//	fastgltf::Parser parser{};
//	const std::filesystem::path directory = filepath.parent_path();
//	auto asset = parser.loadGltf(data.get(), directory,
//		fastgltf::Options::GenerateMeshIndices | fastgltf::Options::LoadExternalBuffers);
//
//	if (asset.error() != fastgltf::Error::None)
//	{
//		Logger::Error(std::format("Failed to load intermediate {}!", filepath.string()));
//		return {};
//	}
//
//	fastgltf::Asset& gltfAsset = asset.get();
//
//	for (size_t meshIndex = 0; meshIndex < gltfAsset.meshes.size(); meshIndex++)
//	{
//		uint32_t primitiveIndex = 0;
//		for (const auto& primitive : gltfAsset.meshes[meshIndex].primitives)
//		{
//			std::string meshName = gltfAsset.meshes[meshIndex].name.c_str();
//
//			if (gltfAsset.meshes[meshIndex].primitives.size() > 1)
//			{
//				meshName += std::format("{}", primitiveIndex++);
//			}
//
//			std::optional<Mesh::CreateInfo> meshCreateInfo;
//			if (!skeletonsByIndex.empty())
//			{
//				meshCreateInfo = GenerateMeshSkinned(filepath, gltfAsset, primitive, meshName, directory, flipUVY);
//			}
//			else
//			{
//				meshCreateInfo = GenerateMesh(filepath, gltfAsset, primitive, meshName, directory, flipUVY);
//			}
//
//			std::shared_ptr<Mesh> mesh = nullptr;
//
//			if (meshCreateInfo)
//			{
//				std::shared_ptr<Mesh> mesh = MeshManager::GetInstance().CreateMesh(*meshCreateInfo);
//				SerializeMesh(mesh->GetFilepath().parent_path(), mesh);
//			}
//
//			primitivesByIndex.emplace_back(mesh);
//		}
//	}
//
//	return std::optional<Mesh::CreateInfo>();
//}

void Serializer::ProcessColors(
	const fastgltf::Asset& gltfAsset,
	const fastgltf::Accessor* colorAccessor,
	void* vertices,
	const size_t vertexSize,
	const size_t colorOffset)
{
	if (colorAccessor->type == fastgltf::AccessorType::Vec3)
	{
		if (colorAccessor->componentType == fastgltf::ComponentType::Float)
		{
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
				gltfAsset,
				*colorAccessor,
			[&](fastgltf::math::fvec3 color, std::size_t index)
			{
				uint32_t& packedColor = *(uint32_t*)(((uint8_t*)vertices) + index * vertexSize + colorOffset);
				packedColor = glm::packUnorm4x8({ color.x(), color.y(), color.z(), 1.0f });
			});
		}
		else if (colorAccessor->componentType == fastgltf::ComponentType::UnsignedByte)
		{
			fastgltf::iterateAccessorWithIndex<fastgltf::math::u8vec3>(
				gltfAsset,
				*colorAccessor,
			[&](fastgltf::math::u8vec3 color, std::size_t index)
			{
				uint8_t* packedColor = ((uint8_t*)vertices) + index * vertexSize + colorOffset;
				packedColor[0] = color.x();
				packedColor[1] = color.y();
				packedColor[2] = color.z();
				packedColor[3] = 255;
			});
		}
		else if (colorAccessor->componentType == fastgltf::ComponentType::UnsignedShort)
		{
			fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec3>(
				gltfAsset,
				*colorAccessor,
			[&](fastgltf::math::u16vec3 color, std::size_t index)
			{
				uint8_t* packedColor = ((uint8_t*)vertices) + index * vertexSize + colorOffset;
				packedColor[0] = color.x() / 255;
				packedColor[1] = color.y() / 255;
				packedColor[2] = color.z() / 255;
				packedColor[3] = 255;
			});
		}
	}
	else if (colorAccessor->type == fastgltf::AccessorType::Vec4)
	{
		if (colorAccessor->componentType == fastgltf::ComponentType::Float)
		{
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
				gltfAsset,
				*colorAccessor,
			[&](fastgltf::math::fvec4 color, std::size_t index)
			{
				uint32_t& packedColor = *(uint32_t*)(((uint8_t*)vertices) + index * vertexSize + colorOffset);
				packedColor = glm::packUnorm4x8({ color.x(), color.y(), color.z(), color.w() });
			});
		}
		else if (colorAccessor->componentType == fastgltf::ComponentType::UnsignedByte)
		{
			fastgltf::iterateAccessorWithIndex<fastgltf::math::u8vec4>(
				gltfAsset,
				*colorAccessor,
			[&](fastgltf::math::u8vec4 color, std::size_t index)
			{
				uint8_t* packedColor = ((uint8_t*)vertices) + index * vertexSize + colorOffset;
				packedColor[0] = color.x();
				packedColor[1] = color.y();
				packedColor[2] = color.z();
				packedColor[3] = color.w();
			});
		}
		else if (colorAccessor->componentType == fastgltf::ComponentType::UnsignedShort)
		{
			fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec4>(
				gltfAsset,
				*colorAccessor,
			[&](fastgltf::math::u16vec4 color, std::size_t index)
			{
				uint8_t* packedColor = ((uint8_t*)vertices) + index * vertexSize + colorOffset;
				packedColor[0] = color.x() / 255;
				packedColor[1] = color.y() / 255;
				packedColor[2] = color.z() / 255;
				packedColor[3] = color.w() / 255;
			});
		}
	}
}

std::shared_ptr<Skeleton> Serializer::GenerateSkeleton(
	const fastgltf::Asset& gltfAsset,
	const fastgltf::Skin& gltfSkin,
	const std::filesystem::path& directory)
{
	Skeleton::CreateInfo createInfo{};
	createInfo.name = gltfSkin.name;
	createInfo.filepath = (directory / createInfo.name).concat(FileFormats::Skeleton());

	std::vector<glm::mat4> inverseBindMatrices;
	if (gltfSkin.inverseBindMatrices)
	{
		const auto& ibmAccessor = gltfAsset.accessors[*gltfSkin.inverseBindMatrices];
		inverseBindMatrices.resize(ibmAccessor.count);

		fastgltf::iterateAccessorWithIndex<fastgltf::math::fmat4x4>(gltfAsset, ibmAccessor,
		[&](fastgltf::math::fmat4x4 matrix, size_t index)
		{
			inverseBindMatrices[index] = glm::make_mat4(matrix.data());
		});
	}

	for (size_t i = 0; i < gltfSkin.joints.size(); i++)
	{
		const auto& node = gltfAsset.nodes[gltfSkin.joints[i]];
		Skeleton::Bone& bone = createInfo.bones.emplace_back();
		bone.name = node.name;
		bone.id = i;
		bone.transform = glm::make_mat4(fastgltf::getTransformMatrix(node).data());
		bone.offset = (i < inverseBindMatrices.size()) ? inverseBindMatrices[i] : glm::mat4(1.0f);
	}

	for (size_t i = 0; i < createInfo.bones.size(); i++)
	{
		const auto& node = gltfAsset.nodes[gltfSkin.joints[i]];

		for (const auto childIndex : node.children)
		{
			const auto joint = std::find(gltfSkin.joints.begin(),
				gltfSkin.joints.end(),
				childIndex);
			if (joint != gltfSkin.joints.end())
			{
				const uint32_t childBoneIndex = static_cast<uint32_t>(joint - gltfSkin.joints.begin());
				createInfo.bones[i].childIds.emplace_back(childBoneIndex);
				createInfo.bones[childBoneIndex].parentId = static_cast<uint32_t>(i);
			}
		}
	}

	if (gltfSkin.skeleton)
	{
		createInfo.rootBoneIds.emplace_back(*gltfSkin.skeleton);
	}
	else
	{
		for (const auto& bone : createInfo.bones)
		{
			if (bone.parentId == -1)
			{
				createInfo.rootBoneIds.emplace_back(bone.id);
			}
		}
	}

	const std::shared_ptr<Skeleton> skeleton = MeshManager::GetInstance().CreateSkeleton(createInfo);
	SerializeSkeleton(skeleton);

	GenerateFileUUID(skeleton->GetFilepath());

	return skeleton;
}

std::shared_ptr<SkeletalAnimation> Serializer::GenerateAnimation(
	const fastgltf::Asset& gltfAsset,
	const fastgltf::Animation& animation,
	const std::filesystem::path& directory)
{
	std::string name = animation.name.c_str();
	name.erase(std::remove(name.begin(), name.end(), ':'), name.end());
	name.erase(std::remove(name.begin(), name.end(), '|'), name.end());

	SkeletalAnimation::CreateInfo createInfo{};
	createInfo.name = name;
	createInfo.filepath = (directory / createInfo.name).concat(FileFormats::Anim());

	for (const auto& channel : animation.channels)
	{
		if (!channel.nodeIndex)
		{
			Logger::Warning(std::format("{}: Animation channel doesn't have node index!", name));
			continue;
		}

		const fastgltf::Node& node = gltfAsset.nodes[*channel.nodeIndex];
		SkeletalAnimation::Bone& bone = createInfo.bonesByName[node.name.c_str()];

		const auto& sampler = animation.samplers[channel.samplerIndex];

		const auto& timeAccessor = gltfAsset.accessors[sampler.inputAccessor];
		const auto& valueAccessor = gltfAsset.accessors[sampler.outputAccessor];

		switch (channel.path)
		{
		case fastgltf::AnimationPath::Translation:
		{
			assert(valueAccessor.count == timeAccessor.count);
			bone.positions.resize(valueAccessor.count);

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltfAsset, valueAccessor,
			[&](fastgltf::math::fvec3 value, size_t index)
			{
				bone.positions[index].value = glm::vec3(value.x(), value.y(), value.z());
			});

			fastgltf::iterateAccessorWithIndex<float>(gltfAsset, timeAccessor,
			[&](float timestamp, size_t index)
			{
				bone.positions[index].time = timestamp;
				createInfo.duration = glm::max<float>(createInfo.duration, timestamp);
			});

			break;
		}
		case fastgltf::AnimationPath::Rotation:
		{
			assert(valueAccessor.count == timeAccessor.count);
			bone.rotations.resize(valueAccessor.count);

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltfAsset, valueAccessor,
			[&](fastgltf::math::fvec4 value, size_t index)
			{
				bone.rotations[index].value = glm::quat(value.w(), value.x(), value.y(), value.z());
			});

			fastgltf::iterateAccessorWithIndex<float>(gltfAsset, timeAccessor,
			[&](float timestamp, size_t index)
			{
				bone.rotations[index].time = timestamp;
				createInfo.duration = glm::max<float>(createInfo.duration, timestamp);
			});

			break;
		}
		case fastgltf::AnimationPath::Scale:
		{
			assert(valueAccessor.count == timeAccessor.count);
			bone.scales.resize(valueAccessor.count);

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltfAsset, valueAccessor,
			[&](fastgltf::math::fvec3 value, size_t index)
			{
				bone.scales[index].value = glm::vec3(value.x(), value.y(), value.z());
			});

			fastgltf::iterateAccessorWithIndex<float>(gltfAsset, timeAccessor,
			[&](float timestamp, size_t index)
			{
				bone.scales[index].time = timestamp;
				createInfo.duration = glm::max<float>(createInfo.duration, timestamp);
			});

			break;
		}
		}
	}

	const std::shared_ptr<SkeletalAnimation> skeletalAnimation = MeshManager::GetInstance().CreateSkeletalAnimation(createInfo);
	SerializeSkeletalAnimation(skeletalAnimation);

	GenerateFileUUID(skeletalAnimation->GetFilepath());

	return skeletalAnimation;
}

std::shared_ptr<Material> Serializer::GenerateMaterial(
	const fastgltf::Asset& gltfAsset,
	const fastgltf::Material& gltfMaterial,
	const std::filesystem::path& texturesDirectory,
	const std::filesystem::path& directory)
{
	const std::string materialName = gltfMaterial.name.c_str();
	if (materialName == "DefaultMaterial")
	{
		return AsyncAssetLoader::GetInstance().SyncLoadMaterial(
			std::filesystem::path("Materials") / "MeshBase.mat");
	}

	std::filesystem::path materialFilepath = directory / materialName;
	materialFilepath.concat(FileFormats::Mat());

	const std::shared_ptr<Material> meshBaseMaterial = AsyncAssetLoader::GetInstance().SyncLoadMaterial(
		std::filesystem::path("Materials") / "MeshBase.mat");
	const std::shared_ptr<Material> meshBaseDoubleSidedMaterial = AsyncAssetLoader::GetInstance().SyncLoadMaterial(
		std::filesystem::path("Materials") / "MeshBaseDoubleSided.mat");

	bool doubleSided = gltfMaterial.doubleSided;

	const std::shared_ptr<Material> material = MaterialManager::GetInstance().Clone(
		materialName,
		materialFilepath,
		doubleSided ? meshBaseDoubleSidedMaterial : meshBaseMaterial);

	material->WriteToBuffer("GBufferMaterial", "material.alphaCutoff", gltfMaterial.alphaCutoff);

	switch (gltfMaterial.alphaMode)
	{
	case fastgltf::AlphaMode::Opaque:
	{
		constexpr int useAlphaCutoff = false;
		material->WriteToBuffer("GBufferMaterial", "material.useAlphaCutoff", useAlphaCutoff);
		break;
	}
	case fastgltf::AlphaMode::Mask:
	{
		constexpr int useAlphaCutoff = true;
		material->WriteToBuffer("GBufferMaterial", "material.useAlphaCutoff", useAlphaCutoff);
		break;
	}
	case fastgltf::AlphaMode::Blend:
	{
		material->SetOption("Transparency", true);
		break;
	}
	}

	const std::shared_ptr<UniformWriter> uniformWriter = material->GetUniformWriter(GBuffer);

	const glm::vec4 baseColor =
	{
		gltfMaterial.pbrData.baseColorFactor.x(),
		gltfMaterial.pbrData.baseColorFactor.y(),
		gltfMaterial.pbrData.baseColorFactor.z(),
		gltfMaterial.pbrData.baseColorFactor.w(),
	};
	material->WriteToBuffer("GBufferMaterial", "material.albedoColor", baseColor);
	
	const glm::vec4 emissiveColor =
	{
		gltfMaterial.emissiveFactor.x(),
		gltfMaterial.emissiveFactor.y(),
		gltfMaterial.emissiveFactor.z(),
		1.0f,
	};
	material->WriteToBuffer("GBufferMaterial", "material.emissiveColor", emissiveColor);

	material->WriteToBuffer("GBufferMaterial", "material.metallicFactor", gltfMaterial.pbrData.metallicFactor);
	material->WriteToBuffer("GBufferMaterial", "material.roughnessFactor", gltfMaterial.pbrData.roughnessFactor);
	material->WriteToBuffer("GBufferMaterial", "material.emissiveFactor", gltfMaterial.emissiveStrength);

	{
		const int useParallaxOcclusion = 0;
		const int minParallaxLayers = 0;
		const int maxParallaxLayers = 0;
		const float parallaxHeightScale = 0.0f;
		material->WriteToBuffer("GBufferMaterial", "material.useParallaxOcclusion", useParallaxOcclusion);
		material->WriteToBuffer("GBufferMaterial", "material.minParallaxLayers", minParallaxLayers);
		material->WriteToBuffer("GBufferMaterial", "material.maxParallaxLayers", maxParallaxLayers);
		material->WriteToBuffer("GBufferMaterial", "material.parallaxHeightScale", parallaxHeightScale);

		const int heightTextureIndex = TextureManager::GetInstance().GetWhite()->GetBindlessIndex();
		material->WriteToBuffer("GBufferMaterial", "material.heightTexture", heightTextureIndex);
	}

	float ao = 1.0f;
	material->WriteToBuffer("GBufferMaterial", "material.aoFactor", ao);

	if (gltfMaterial.pbrData.baseColorTexture.has_value())
	{
		Texture::Meta meta{};
		meta.uuid = UUID();
		meta.srgb = true;
		if (const std::shared_ptr<Texture> albedoTexture = LoadGltfTexture(
			gltfAsset,
			gltfAsset.textures[gltfMaterial.pbrData.baseColorTexture->textureIndex],
			texturesDirectory,
			directory,
			materialName + "_BaseColor" + FileFormats::Png(),
			meta))
		{
			const int albedoTextureIndex = albedoTexture->GetBindlessIndex();
			material->WriteToBuffer("GBufferMaterial", "material.albedoTexture", albedoTextureIndex);
		}
	}
	else
	{
		const int albedoTextureIndex = TextureManager::GetInstance().GetWhite()->GetBindlessIndex();
		material->WriteToBuffer("GBufferMaterial", "material.albedoTexture", albedoTextureIndex);
	}

	if (gltfMaterial.normalTexture.has_value())
	{
		if (const std::shared_ptr<Texture> normalTexture = LoadGltfTexture(
			gltfAsset,
			gltfAsset.textures[gltfMaterial.normalTexture->textureIndex],
			texturesDirectory,
			directory,
			materialName + "_Normal" + FileFormats::Png()))
		{
			const int normalTextureIndex = normalTexture->GetBindlessIndex();
			material->WriteToBuffer("GBufferMaterial", "material.normalTexture", normalTextureIndex);

			constexpr int useNormalMap = 1;
			material->WriteToBuffer("GBufferMaterial", "material.useNormalMap", useNormalMap);
		}
	}
	else
	{
		const int normalTextureIndex = TextureManager::GetInstance().GetWhite()->GetBindlessIndex();
		material->WriteToBuffer("GBufferMaterial", "material.normalTexture", normalTextureIndex);
	}

	if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value())
	{
		if (const std::shared_ptr<Texture> metallicRoughnessTexture = LoadGltfTexture(
			gltfAsset,
			gltfAsset.textures[gltfMaterial.pbrData.metallicRoughnessTexture->textureIndex],
			texturesDirectory,
			directory,
			materialName + "_MetallicRoughness" + FileFormats::Png()))
		{
			const int metallicRoughnessTextureIndex = metallicRoughnessTexture->GetBindlessIndex();
			material->WriteToBuffer("GBufferMaterial", "material.metallicRoughnessTexture", metallicRoughnessTextureIndex);
		}
	}
	else
	{
		const int metallicRoughnessTextureIndex = TextureManager::GetInstance().GetWhite()->GetBindlessIndex();
		material->WriteToBuffer("GBufferMaterial", "material.metallicRoughnessTexture", metallicRoughnessTextureIndex);
	}

	if (gltfMaterial.occlusionTexture.has_value())
	{
		if (const std::shared_ptr<Texture> aoTexture = LoadGltfTexture(
			gltfAsset,
			gltfAsset.textures[gltfMaterial.occlusionTexture->textureIndex],
			texturesDirectory,
			directory,
			materialName + "_Occlusion" + FileFormats::Png()))
		{
			const int aoTextureIndex = aoTexture->GetBindlessIndex();
			material->WriteToBuffer("GBufferMaterial", "material.aoTexture", aoTextureIndex);
		}
	}
	else
	{
		const int aoTextureIndex = TextureManager::GetInstance().GetWhite()->GetBindlessIndex();
		material->WriteToBuffer("GBufferMaterial", "material.aoTexture", aoTextureIndex);
	}

	if (gltfMaterial.emissiveTexture.has_value())
	{
		if (const std::shared_ptr<Texture> emissiveTexture = LoadGltfTexture(
			gltfAsset,
			gltfAsset.textures[gltfMaterial.emissiveTexture->textureIndex],
			texturesDirectory,
			directory,
			materialName + "_Emissive" + FileFormats::Png()))
		{
			const int emissiveTextureIndex = emissiveTexture->GetBindlessIndex();
			material->WriteToBuffer("GBufferMaterial", "material.emissiveTexture", emissiveTextureIndex);
		}
	}
	else
	{
		const int emissiveTextureIndex = TextureManager::GetInstance().GetWhite()->GetBindlessIndex();
		material->WriteToBuffer("GBufferMaterial", "material.emissiveTexture", emissiveTextureIndex);
	}

	material->GetBuffer("GBufferMaterial")->Flush();
	uniformWriter->Flush();

	Material::Save(material);

	return material;
}

std::shared_ptr<Entity> Serializer::GenerateEntity(
	const fastgltf::Asset& gltfAsset,
	const fastgltf::Node& gltfNode,
	const std::shared_ptr<Scene>& scene,
	const std::vector<std::vector<std::shared_ptr<Mesh>>>& meshesByIndex,
	const std::vector<std::shared_ptr<Skeleton>>& skeletonsByIndex,
	const std::unordered_map<std::shared_ptr<Mesh>, std::shared_ptr<Material>>& materialsByMeshes,
	const std::vector<std::shared_ptr<SkeletalAnimation>>& animations)
{
	const std::string nodeName = gltfNode.name.empty() ? "Unnamed" : gltfNode.name.c_str();

	std::vector<std::shared_ptr<Entity>> nodes;
	if (gltfNode.meshIndex && !meshesByIndex.empty())
	{
		for (const std::shared_ptr<Mesh> mesh : meshesByIndex[*gltfNode.meshIndex])
		{
			std::shared_ptr<Entity> node = scene->CreateEntity(nodeName);
			Transform& nodeTransform = node->AddComponent<Transform>(node);

			nodes.emplace_back(node);

			Renderer3D& r3d = node->AddComponent<Renderer3D>();
			r3d.mesh = mesh;

			const std::shared_ptr<Material> material = materialsByMeshes.find(mesh)->second;
			if (material)
			{
				r3d.material = material;
			}
			else
			{
				if (r3d.mesh->GetType() == Mesh::Type::STATIC)
				{
					r3d.material = AsyncAssetLoader::GetInstance().SyncLoadMaterial(
						std::filesystem::path("Materials") / "MeshBase.mat");
				}
				else if (r3d.mesh->GetType() == Mesh::Type::SKINNED)
				{
					r3d.material = AsyncAssetLoader::GetInstance().SyncLoadMaterial(
						std::filesystem::path("Materials") / "MeshBaseSkinned.mat");
				}
			}

			if (r3d.mesh->GetType() == Mesh::Type::SKINNED)
			{
				r3d.material->SetBaseMaterial(MaterialManager::GetInstance().LoadBaseMaterial(
					std::filesystem::path("Materials") / "MeshBaseSkinned.basemat"));
				Material::Save(r3d.material, false);

				if (gltfNode.skinIndex)
				{
					std::shared_ptr<Entity> topEntity = scene->GetEntities().front();
					if (!topEntity->HasComponent<SkeletalAnimator>())
					{
						SkeletalAnimator& skeletalAnimator = topEntity->AddComponent<SkeletalAnimator>();
						skeletalAnimator.SetSkeleton(skeletonsByIndex[*gltfNode.skinIndex]);
						skeletalAnimator.SetSpeed(1.0f);

						if (!animations.empty())
						{
							skeletalAnimator.SetSkeletalAnimation(animations[0]);
						}
					}

					r3d.skeletalAnimatorEntityUUID = topEntity->GetUUID();
				}
			}
		}
	}

	if (nodes.empty())
	{
		std::shared_ptr<Entity> node = scene->CreateEntity(nodeName);
		Transform& nodeTransform = node->AddComponent<Transform>(node);

		nodes.emplace_back(node);
	}

	std::shared_ptr<Entity> rootNode = nodes.front();

	for (size_t childIndex = 0; childIndex < gltfNode.children.size(); childIndex++)
	{
		const auto child = gltfNode.children[childIndex];
		rootNode->AddChild(GenerateEntity(gltfAsset, gltfAsset.nodes[child], scene, meshesByIndex, skeletonsByIndex, materialsByMeshes, animations));
	}

	for (const auto& node : nodes)
	{
		if (rootNode != node)
		{
			rootNode->AddChild(node);
		}

		const auto gltfTransformMat4 = fastgltf::getTransformMatrix(gltfNode);
		const glm::mat4 transformMat4 = glm::make_mat4(gltfTransformMat4.data());

		glm::vec3 position;
		glm::vec3 rotation;
		glm::vec3 scale;
		Utils::DecomposeTransform(transformMat4, position, rotation, scale);

		Transform& nodeTransform = node->GetComponent<Transform>();
		nodeTransform.Translate(position);
		nodeTransform.Rotate(rotation);
		nodeTransform.Scale(scale);
	}
	
	return rootNode;
}

void Serializer::SerializeEntity(YAML::Emitter& out, const std::shared_ptr<Entity>& entity, bool rootEntity, bool isSerializingPrefab)
{
	if (!entity)
	{
		return;
	}

	if (!(isSerializingPrefab && rootEntity) && entity->IsPrefab())
	{
		out << YAML::BeginMap;

		out << YAML::Key << "PrefabFilepath" << YAML::Value << entity->GetPrefabFilepathUUID();
		out << YAML::Key << "UUID" << YAML::Value << entity->GetUUID();
		SerializeTransform(out, entity);

		out << YAML::EndMap;

		return;
	}

	out << YAML::BeginMap;

	if (!isSerializingPrefab)
	{
		out << YAML::Key << "UUID" << YAML::Value << entity->GetUUID();
	}

	out << YAML::Key << "Name" << YAML::Value << entity->GetName();
	out << YAML::Key << "IsEnabled" << YAML::Value << entity->IsEnabled();

	SerializeTransform(out, entity);
	SerializeCamera(out, entity);
	SerializeRenderer3D(out, entity);
	SerializePointLight(out, entity);
	SerializeSpotLight(out, entity);
	SerializeDecal(out, entity);
	SerializeDirectionalLight(out, entity);
	SerializeSkeletalAnimator(out, entity);
	SerializeEntityAnimator(out, entity);
	SerializeCanvas(out, entity);
	SerializeRigidBody(out, entity);
	SerializeUserComponents(out, entity);

	// Childs.
	out << YAML::Key << "Childs";
	out << YAML::BeginSeq;

	for (const std::weak_ptr<Entity> weakChild : entity->GetChilds())
	{
		if (const std::shared_ptr<Entity> child = weakChild.lock())
		{
			SerializeEntity(out, child, false, isSerializingPrefab);
		}
	}

	out << YAML::EndSeq;
	//

	out << YAML::EndMap;
}

std::shared_ptr<Entity> Serializer::DeserializeEntity(
	const YAML::Node& in,
	const std::shared_ptr<Scene>& scene)
{
	if (const auto& prefabFilepathData = in["PrefabFilepath"])
	{
		const UUID prefabUUID = prefabFilepathData.as<UUID>();
		const std::filesystem::path prefabFilepath = Utils::FindFilepath(prefabUUID);
		if (!std::filesystem::exists(prefabFilepath))
		{
			Logger::Error(prefabUUID.ToString() + ":Prefab doesn't exist!");
			return nullptr;
		}

		const std::shared_ptr<Entity> prefab = DeserializePrefab(prefabFilepath, scene);
		DeserializeTransform(in, prefab);

		if (const auto& uuidData = in["UUID"])
		{
			prefab->SetUUID(uuidData.as<UUID>());
		}

		return prefab;
	}

	UUID uuid;
	if (const auto& uuidData = in["UUID"])
	{
		uuid = uuidData.as<UUID>();
	}

	std::string name;
	if (const auto& nameData = in["Name"])
	{
		name = nameData.as<std::string>();
	}

	std::shared_ptr<Entity> entity = scene->CreateEntity(name, uuid.IsValid() ? uuid : UUID());

	if (const auto& isEnabledData = in["IsEnabled"])
	{
		entity->SetEnabled(isEnabledData.as<bool>());
	}

	DeserializeTransform(in, entity);
	DeserializeCamera(in, entity);
	DeserializeRenderer3D(in, entity);
	DeserializePointLight(in, entity);
	DeserializeSpotLight(in, entity);
	DeserializeDecal(in, entity);
	DeserializeDirectionalLight(in, entity);
	DeserializeSkeletalAnimator(in, entity);
	DeserializeEntityAnimator(in, entity);
	DeserializeCanvas(in, entity);
	DeserializeRigidBody(in, entity);
	DeserializeUserComponents(in, entity);

	for (const auto& childData : in["Childs"])
	{
		if (const std::shared_ptr<Entity> child = DeserializeEntity(childData, scene))
		{
			entity->AddChild(child, false);
		}
	}

	return entity;
}

void Serializer::SerializePrefab(const std::filesystem::path& filepath, const std::shared_ptr<Entity>& entity)
{
	if (filepath.empty() || filepath == none || Utils::GetFileFormat(filepath) != FileFormats::Prefab())
	{
		Logger::Error(filepath.string() + ":Failed to serialize prefab! Filepath is incorrect!");
		return;
	}

	YAML::Emitter out;

	SerializeEntity(out, entity, true, true);

	std::ofstream fout(filepath);
	fout << out.c_str();
	fout.close();

	entity->SetPrefabFilepathUUID(GenerateFileUUID(filepath));
}

std::shared_ptr<Entity> Serializer::DeserializePrefab(const std::filesystem::path& filepath, const std::shared_ptr<Scene>& scene)
{
	if (filepath.empty() || filepath == none || Utils::GetFileFormat(filepath) != FileFormats::Prefab())
	{
		Logger::Error(filepath.string() + ":Failed to load prefab! Filepath is incorrect!");
		return nullptr;
	}

	std::ifstream stream(filepath);
	std::stringstream stringStream;

	stringStream << stream.rdbuf();

	stream.close();

	YAML::Node data = YAML::LoadMesh(stringStream.str());
	if (!data)
	{
		FATAL_ERROR(filepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
	}

	std::shared_ptr<Entity> entity = DeserializeEntity(data, scene);
	if (entity)
	{
		entity->SetPrefabFilepathUUID(Utils::FindUuid(filepath));

		Logger::Log("Prefab:" + filepath.string() + " has been loaded!", BOLDGREEN);	
	}

	return entity;
}

void Serializer::UpdatePrefab(
	const std::filesystem::path& filepath,
	const std::vector<std::shared_ptr<Entity>> entities)
{

}

void Serializer::SerializeTransform(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<Transform>())
	{
		return;
	}

	const Transform& transform = entity->GetComponent<Transform>();

	out << YAML::Key << "Transform";

	out << YAML::BeginMap;

	out << YAML::Key << "Position" << YAML::Value << transform.GetPosition(Transform::System::LOCAL);
	out << YAML::Key << "Rotation" << YAML::Value << transform.GetRotation(Transform::System::LOCAL);
	out << YAML::Key << "Scale" << YAML::Value << transform.GetScale(Transform::System::LOCAL);
	out << YAML::Key << "FollowOwner" << YAML::Value << transform.GetFollorOwner();

	out << YAML::EndMap;
}

void Serializer::DeserializeTransform(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& transformData = in["Transform"])
	{
		if (!entity->HasComponent<Transform>())
		{
			entity->AddComponent<Transform>(entity);
		}

		Transform& transform = entity->GetComponent<Transform>();

		if (const auto& positionData = transformData["Position"])
		{
			transform.Translate(positionData.as<glm::vec3>());
		}

		if (const auto& rotationData = transformData["Rotation"])
		{
			transform.Rotate(rotationData.as<glm::vec3>());
		}

		if (const auto& scaleData = transformData["Scale"])
		{
			transform.Scale(scaleData.as<glm::vec3>());
		}

		if (const auto& follorOwnerData = transformData["FollowOwner"])
		{
			transform.SetFollowOwner(follorOwnerData.as<bool>());
		}
	}
}

void Serializer::SerializeRenderer3D(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<Renderer3D>())
	{
		return;
	}

	const Renderer3D& r3d = entity->GetComponent<Renderer3D>();

	out << YAML::Key << "Renderer3D";

	out << YAML::BeginMap;

	if (r3d.skeletalAnimatorEntityUUID.IsValid())
	{
		out << YAML::Key << "SkeletalAnimatorEntity" << YAML::Value << entity->GetScene()->FindEntityByUUID(r3d.skeletalAnimatorEntityUUID)->GetName();
	}

	if (r3d.mesh)
	{
		UUID uuid = Utils::FindUuid(r3d.mesh->GetFilepath());
		if (uuid.IsValid())
		{
			out << YAML::Key << "Mesh" << YAML::Value << Utils::FindUuid(r3d.mesh->GetFilepath());
		}
	}

	if (r3d.material)
	{
		UUID uuid = Utils::FindUuid(r3d.material->GetFilepath());
		if (uuid.IsValid())
		{
			out << YAML::Key << "Material" << YAML::Value << uuid;
		}
	}

	out << YAML::Key << "RenderingOrder" << YAML::Value << r3d.renderingOrder;
	out << YAML::Key << "IsEnabled" << YAML::Value << r3d.isEnabled;
	out << YAML::Key << "CastShadows" << YAML::Value << r3d.castShadows;
	out << YAML::Key << "ObjectVisibilityMask" << YAML::Value << (uint32_t)r3d.objectVisibilityMask;
	out << YAML::Key << "ShadowVisibilityMask" << YAML::Value << (uint32_t)r3d.shadowVisibilityMask;

	out << YAML::EndMap;
}

void Serializer::DeserializeRenderer3D(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& renderer3DData = in["Renderer3D"])
	{
		if (!entity->HasComponent<Renderer3D>())
		{
			entity->AddComponent<Renderer3D>();
		}

		Renderer3D& r3d = entity->GetComponent<Renderer3D>();

		if (const auto& renderingOrderData = renderer3DData["RenderingOrder"])
		{
			r3d.renderingOrder = glm::clamp(renderingOrderData.as<int>(), 0, 10);
		}

		if (const auto& isEnabledData = renderer3DData["IsEnabled"])
		{
			r3d.isEnabled = isEnabledData.as<bool>();
		}

		if (const auto& skeletalAnimatorEntityData = renderer3DData["SkeletalAnimatorEntity"])
		{
			auto callback = [&r3d, wEntity = std::weak_ptr(entity), skeletalAnimatorEntityName = skeletalAnimatorEntityData.as<std::string>()]()
			{
				if (const std::shared_ptr<Entity> entity = wEntity.lock())
				{
					if (const std::shared_ptr<Entity> skeletalAnimatorEntity = entity->GetTopEntity()->FindEntityInHierarchy(skeletalAnimatorEntityName))
					{
						r3d.skeletalAnimatorEntityUUID = skeletalAnimatorEntity->GetUUID();
					}
				}
			};

			std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, nullptr);
			EventSystem::GetInstance().SendEvent(event);
		}

		if (const auto& castShadowsData = renderer3DData["CastShadows"])
		{
			r3d.castShadows = castShadowsData.as<bool>();
		}

		if (const auto& objectVisibilityMaskData = renderer3DData["ObjectVisibilityMask"])
		{
			r3d.objectVisibilityMask = objectVisibilityMaskData.as<uint32_t>();
		}

		if (const auto& shadowVisibilityMaskData = renderer3DData["ShadowVisibilityMask"])
		{
			r3d.shadowVisibilityMask = shadowVisibilityMaskData.as<uint32_t>();
		}

		if (const auto& meshData = renderer3DData["Mesh"])
		{
			const UUID uuid = meshData.as<UUID>();
			AsyncAssetLoader::GetInstance().AsyncLoadMesh(Utils::FindFilepath(uuid), [wEntity = std::weak_ptr(entity)](std::weak_ptr<Mesh> mesh)
			{
				std::shared_ptr<Mesh> sharedMesh = mesh.lock();
				if (!sharedMesh)
				{
					return;
				}

				if (std::shared_ptr<Entity> entity = wEntity.lock())
				{
					if (entity->HasComponent<Renderer3D>())
					{
						entity->GetComponent<Renderer3D>().mesh = sharedMesh;
					}
				}
				else
				{
					auto callback = [mesh]()
					{
						std::shared_ptr<Mesh> sharedMesh = mesh.lock();
						MeshManager::GetInstance().DeleteMesh(sharedMesh);
					};

					std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, nullptr);
					EventSystem::GetInstance().SendEvent(event);
					
				}
			});
		}

		if (const auto& materialData = renderer3DData["Material"])
		{
			const UUID uuid = materialData.as<UUID>();

			AsyncAssetLoader::GetInstance().AsyncLoadMaterial(Utils::FindFilepath(uuid), [wEntity = std::weak_ptr(entity)](std::weak_ptr<Material> material)
			{
				std::shared_ptr<Material> sharedMaterial = material.lock();
				if (!material.lock())
				{
					return;
				}

				if (std::shared_ptr<Entity> entity = wEntity.lock())
				{
					if (entity->HasComponent<Renderer3D>())
					{
						entity->GetComponent<Renderer3D>().material = sharedMaterial;
					}
				}
				else
				{
					auto callback = [material]()
					{
						std::shared_ptr<Material> sharedMaterial = material.lock();
						MaterialManager::GetInstance().DeleteMaterial(sharedMaterial);
					};

					std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, nullptr);
					EventSystem::GetInstance().SendEvent(event);
				}
			});
		}
	}
}

void Serializer::SerializePointLight(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<PointLight>())
	{
		return;
	}

	const PointLight& pointLight = entity->GetComponent<PointLight>();

	out << YAML::Key << "PointLight";

	out << YAML::BeginMap;

	out << YAML::Key << "Color" << YAML::Value << pointLight.color;
	out << YAML::Key << "Intensity" << YAML::Value << pointLight.intensity;
	out << YAML::Key << "Radius" << YAML::Value << pointLight.radius;
	out << YAML::Key << "Bias" << YAML::Value << pointLight.bias;
	out << YAML::Key << "DrawBoundingSphere" << YAML::Value << pointLight.drawBoundingSphere;
	out << YAML::Key << "CastShadows" << YAML::Value << pointLight.castShadows;
	out << YAML::Key << "CastSSS" << YAML::Value << pointLight.castSSS;

	out << YAML::EndMap;
}

void Serializer::DeserializePointLight(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& pointLightData = in["PointLight"])
	{
		if (!entity->HasComponent<PointLight>())
		{
			entity->AddComponent<PointLight>();
		}

		PointLight& pointLight = entity->GetComponent<PointLight>();

		if (const auto& colorData = pointLightData["Color"])
		{
			pointLight.color = colorData.as<glm::vec3>();
		}

		if (const auto& intensityData = pointLightData["Intensity"])
		{
			pointLight.intensity = intensityData.as<float>();
		}

		if (const auto& radiusData = pointLightData["Radius"])
		{
			pointLight.radius = radiusData.as<float>();
		}

		if (const auto& biasData = pointLightData["Bias"])
		{
			pointLight.bias = biasData.as<float>();
		}

		if (const auto& drawBoundingSphereData = pointLightData["DrawBoundingSphere"])
		{
			pointLight.drawBoundingSphere = drawBoundingSphereData.as<bool>();
		}

		if (const auto& castShadowsData = pointLightData["CastShadows"])
		{
			pointLight.castShadows = castShadowsData.as<bool>();
		}

		if (const auto& castSSSData = pointLightData["CastSSS"])
		{
			pointLight.castSSS = castSSSData.as<bool>();
		}
	}
}

void Serializer::SerializeSpotLight(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<SpotLight>())
	{
		return;
	}

	const SpotLight& spotLight = entity->GetComponent<SpotLight>();

	out << YAML::Key << "SpotLight";

	out << YAML::BeginMap;

	out << YAML::Key << "Color" << YAML::Value << spotLight.color;
	out << YAML::Key << "Intensity" << YAML::Value << spotLight.intensity;
	out << YAML::Key << "Radius" << YAML::Value << spotLight.radius;
	out << YAML::Key << "Bias" << YAML::Value << spotLight.bias;
	out << YAML::Key << "InnerCutOff" << YAML::Value << spotLight.innerCutOff;
	out << YAML::Key << "OuterCutOff" << YAML::Value << spotLight.outerCutOff;
	out << YAML::Key << "DrawBoundingSphere" << YAML::Value << spotLight.drawBoundingSphere;
	out << YAML::Key << "CastShadows" << YAML::Value << spotLight.castShadows;
	out << YAML::Key << "CastSSS" << YAML::Value << spotLight.castSSS;

	out << YAML::EndMap;
}

void Serializer::DeserializeSpotLight(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& spotLightData = in["SpotLight"])
	{
		if (!entity->HasComponent<SpotLight>())
		{
			entity->AddComponent<SpotLight>();
		}

		SpotLight& spotLight = entity->GetComponent<SpotLight>();

		if (const auto& colorData = spotLightData["Color"])
		{
			spotLight.color = colorData.as<glm::vec3>();
		}

		if (const auto& intensityData = spotLightData["Intensity"])
		{
			spotLight.intensity = intensityData.as<float>();
		}

		if (const auto& radiusData = spotLightData["Radius"])
		{
			spotLight.radius = radiusData.as<float>();
		}

		if (const auto& biasData = spotLightData["Bias"])
		{
			spotLight.bias = biasData.as<float>();
		}

		if (const auto& innerCutOffData = spotLightData["InnerCutOff"])
		{
			spotLight.innerCutOff = innerCutOffData.as<float>();
		}

		if (const auto& outerCutOffData = spotLightData["OuterCutOff"])
		{
			spotLight.outerCutOff = outerCutOffData.as<float>();
		}

		if (const auto& drawBoundingSphereData = spotLightData["DrawBoundingSphere"])
		{
			spotLight.drawBoundingSphere = drawBoundingSphereData.as<bool>();
		}

		if (const auto& castShadowsData = spotLightData["CastShadows"])
		{
			spotLight.castShadows = castShadowsData.as<bool>();
		}

		if (const auto& castSSSData = spotLightData["CastSSS"])
		{
			spotLight.castSSS = castSSSData.as<bool>();
		}
	}
}

void Serializer::SerializeDirectionalLight(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<DirectionalLight>())
	{
		return;
	}

	const DirectionalLight& directionalLight = entity->GetComponent<DirectionalLight>();

	out << YAML::Key << "DirectionalLight";

	out << YAML::BeginMap;

	out << YAML::Key << "Color" << YAML::Value << directionalLight.color;
	out << YAML::Key << "Intensity" << YAML::Value << directionalLight.intensity;
	out << YAML::Key << "Ambient" << YAML::Value << directionalLight.ambient;

	out << YAML::EndMap;
}

void Serializer::DeserializeDirectionalLight(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& directionalLightData = in["DirectionalLight"])
	{
		if (!entity->HasComponent<DirectionalLight>())
		{
			entity->AddComponent<DirectionalLight>();
		}

		DirectionalLight& directionalLight = entity->GetComponent<DirectionalLight>();

		if (const auto& colorData = directionalLightData["Color"])
		{
			directionalLight.color = colorData.as<glm::vec3>();
		}

		if (const auto& intensityData = directionalLightData["Intensity"])
		{
			directionalLight.intensity = intensityData.as<float>();
		}

		if (const auto& ambientData = directionalLightData["Ambient"])
		{
			directionalLight.ambient = ambientData.as<float>();
		}
	}
}

void Serializer::SerializeSkeletalAnimator(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<SkeletalAnimator>())
	{
		return;
	}

	const SkeletalAnimator& skeletalAnimator = entity->GetComponent<SkeletalAnimator>();

	out << YAML::Key << "SkeletalAnimator";

	out << YAML::BeginMap;

	if (skeletalAnimator.GetSkeleton())
	{
		out << YAML::Key << "Skeleton" << YAML::Value << Utils::FindUuid(skeletalAnimator.GetSkeleton()->GetFilepath());
	}

	if (skeletalAnimator.GetSkeletalAnimation())
	{
		out << YAML::Key << "SkeletalAnimation" << YAML::Value << Utils::FindUuid(skeletalAnimator.GetSkeletalAnimation()->GetFilepath());
	}

	out << YAML::Key << "Speed" << YAML::Value << skeletalAnimator.GetSpeed();
	out << YAML::Key << "CurrentTime" << YAML::Value << skeletalAnimator.GetCurrentTime();
	out << YAML::Key << "ApplySkeletonTransform" << YAML::Value << skeletalAnimator.GetApplySkeletonTransform();

	out << YAML::EndMap;
}

void Serializer::DeserializeSkeletalAnimator(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& skeletalAnimatorData = in["SkeletalAnimator"])
	{
		if (!entity->HasComponent<SkeletalAnimator>())
		{
			entity->AddComponent<SkeletalAnimator>();
		}

		SkeletalAnimator& skeletalAnimator = entity->GetComponent<SkeletalAnimator>();

		if (const auto& skeletonData = skeletalAnimatorData["Skeleton"])
		{
			skeletalAnimator.SetSkeleton(MeshManager::GetInstance().LoadSkeleton(DeserializeFilepath(skeletonData.as<std::string>())));
		}

		if (const auto& skeletalAnimationData = skeletalAnimatorData["SkeletalAnimation"])
		{
			skeletalAnimator.SetSkeletalAnimation(MeshManager::GetInstance().LoadSkeletalAnimation(DeserializeFilepath(skeletalAnimationData.as<std::string>())));
		}

		if (const auto& speedData = skeletalAnimatorData["Speed"])
		{
			skeletalAnimator.SetSpeed(speedData.as<float>());
		}

		if (const auto& currentTimeData = skeletalAnimatorData["CurrentTime"])
		{
			skeletalAnimator.SetCurrentTime(currentTimeData.as<float>());
		}

		if (const auto& applySkeletonTransformData = skeletalAnimatorData["ApplySkeletonTransform"])
		{
			skeletalAnimator.SetApplySkeletonTransform(applySkeletonTransformData.as<bool>());
		}
	}
}

void Serializer::SerializeEntityAnimator(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<EntityAnimator>())
	{
		return;
	}

	const EntityAnimator& entityAnimator = entity->GetComponent<EntityAnimator>();

	out << YAML::Key << "EntityAnimator";

	out << YAML::BeginMap;

	if (entityAnimator.animationTrack)
	{
		out << YAML::Key << "AnimationTrack" << YAML::Value << Utils::FindUuid(entityAnimator.animationTrack->GetFilepath());
	}

	out << YAML::Key << "Time" << YAML::Value << entityAnimator.time;
	out << YAML::Key << "Speed" << YAML::Value << entityAnimator.speed;
	out << YAML::Key << "IsPlaying" << YAML::Value << entityAnimator.isPlaying;
	out << YAML::Key << "IsLoop" << YAML::Value << entityAnimator.isLoop;

	out << YAML::EndMap;
}

void Serializer::DeserializeEntityAnimator(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& entityAnimatorData = in["EntityAnimator"])
	{
		if (!entity->HasComponent<EntityAnimator>())
		{
			entity->AddComponent<EntityAnimator>();
		}

		EntityAnimator& entityAnimator = entity->GetComponent<EntityAnimator>();

		if (const auto& animationTrackData = entityAnimatorData["AnimationTrack"])
		{
			entityAnimator.animationTrack = DeserializeAnimationTrack(DeserializeFilepath(animationTrackData.as<std::string>()));
		}

		if (const auto& speedData = entityAnimatorData["Speed"])
		{
			entityAnimator.speed = speedData.as<float>();
		}

		if (const auto& timeData = entityAnimatorData["Time"])
		{
			entityAnimator.time = timeData.as<float>();
		}

		if (const auto& isPlayingData = entityAnimatorData["IsPlaying"])
		{
			entityAnimator.isPlaying = isPlayingData.as<bool>();
		}

		if (const auto& isLoopData = entityAnimatorData["IsLoop"])
		{
			entityAnimator.isLoop = isLoopData.as<bool>();
		}
	}
}

void Serializer::SerializeCamera(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<Camera>())
	{
		return;
	}

	const Camera& camera = entity->GetComponent<Camera>();

	out << YAML::Key << "Camera";

	out << YAML::BeginMap;

	out << YAML::Key << "Fov" << YAML::Value << camera.GetFov();
	out << YAML::Key << "Type" << YAML::Value << static_cast<int>(camera.GetType());
	out << YAML::Key << "ZNear" << YAML::Value << camera.GetZNear();
	out << YAML::Key << "ZFar" << YAML::Value << camera.GetZFar();
	out << YAML::Key << "PassName" << YAML::Value << camera.GetPassName();
	out << YAML::Key << "RenderTargetIndex" << YAML::Value << camera.GetRenderTargetIndex();
	out << YAML::Key << "ObjectVisibilityMask" << YAML::Value << (uint32_t)camera.GetObjectVisibilityMask();
	out << YAML::Key << "ShadowVisibilityMask" << YAML::Value << (uint32_t)camera.GetShadowVisibilityMask();

	// TODO: Maybe do it more optimal.
	// TODO: Also take window into account, for now it is just viewports, but viewports can have the same names across different windows.
	for (const auto& [windowName, window] : WindowManager::GetInstance().GetWindows())
	{
		for (const auto& [viewportName, viewport] : window->GetViewportManager().GetViewports())
		{
			if (const std::shared_ptr<Entity> viewportCamera = viewport->GetCamera().lock())
			{
				if (viewportCamera->GetUUID() == entity->GetUUID())
				{
					out << YAML::Key << "Viewport" << YAML::Value << viewportName;
				}
			}
		}
	}

	out << YAML::EndMap;
}

void Serializer::DeserializeCamera(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& cameraData = in["Camera"])
	{
		if (!entity->HasComponent<Camera>())
		{
			entity->AddComponent<Camera>(entity);
		}

		Camera& camera = entity->GetComponent<Camera>();

		if (const auto& fovData = cameraData["Fov"])
		{
			camera.SetFov(fovData.as<float>());
		}

		if (const auto& typeData = cameraData["Type"])
		{
			camera.SetType(static_cast<Camera::Type>(typeData.as<int>()));
		}

		if (const auto& zNearData = cameraData["ZNear"])
		{
			camera.SetZNear(zNearData.as<float>());
		}

		if (const auto& zFarData = cameraData["ZFar"])
		{
			camera.SetZFar(zFarData.as<float>());
		}

		if (const auto& passNameData = cameraData["PassName"])
		{
			camera.SetPassName(passNameData.as<std::string>());
		}

		if (const auto& renderTargetIndexData = cameraData["RenderTargetIndex"])
		{
			camera.SetRenderTargetIndex(renderTargetIndexData.as<int>());
		}

		if (const auto& objectVisibilityMaskData = cameraData["ObjectVisibilityMask"])
		{
			camera.SetObjectVisibilityMask(objectVisibilityMaskData.as<uint32_t>());
		}

		if (const auto& shadowVisibilityMaskData = cameraData["ShadowVisibilityMask"])
		{
			camera.SetShadowVisibilityMask(shadowVisibilityMaskData.as<uint32_t>());
		}

		if (const auto& viewportData = cameraData["Viewport"])
		{
			// TODO: Take window name into account
			for (const auto& [name, window] : WindowManager::GetInstance().GetWindows())
			{
				const std::shared_ptr<Viewport> viewport = window->GetViewportManager().GetViewport(viewportData.as<std::string>());
				if (viewport)
				{
					viewport->SetCamera(entity);
					break;
				}
			}
		}
	}
}

void Serializer::SerializeCanvas(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<Canvas>())
	{
		return;
	}

	const Canvas& canvas = entity->GetComponent<Canvas>();

	out << YAML::Key << "Canvas";

	out << YAML::BeginMap;

	out << YAML::Key << "DrawInMainViewport" << YAML::Value << canvas.drawInMainViewport;
	out << YAML::Key << "Size" << YAML::Value << canvas.size;

	std::vector<std::string> scriptNames;
	for (const auto& script : canvas.scripts)
	{
		scriptNames.emplace_back(script.name);
	}
	out << YAML::Key << "ScriptNames" << YAML::Value << scriptNames;

	out << YAML::EndMap;
}

void Serializer::DeserializeCanvas(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& canvasData = in["Canvas"])
	{
		if (!entity->HasComponent<Canvas>())
		{
			entity->AddComponent<Canvas>();
		}

		if (!entity->HasComponent<Renderer3D>())
		{
			entity->AddComponent<Renderer3D>();
		}

		auto& r3d = entity->GetComponent<Renderer3D>();
		const std::shared_ptr<Material> defaultMaterial = MaterialManager::GetInstance().LoadMaterial(std::filesystem::path("Materials") / "UIBase.mat");
		const std::string name = std::to_string(UUID::Generate());
		std::filesystem::path filepath = defaultMaterial->GetFilepath().parent_path() / name;
		filepath.replace_extension(FileFormats::Mat());

		r3d.material = Material::Clone(name, filepath, defaultMaterial);

		Canvas& canvas = entity->GetComponent<Canvas>();

		if (const auto& drawInMainViewportData = canvasData["DrawInMainViewport"])
		{
			canvas.drawInMainViewport = drawInMainViewportData.as<bool>();
		}

		if (const auto& sizeData = canvasData["Size"])
		{
			canvas.size = sizeData.as<glm::ivec2>();
		}

		for (const auto& scriptNamesData : canvasData["ScriptNames"])
		{
			const std::string scriptName = scriptNamesData.as<std::string>();
			if (ClayManager::GetInstance().scriptsByName.contains(scriptName))
			{
				Canvas::Script& script = canvas.scripts.emplace_back();
				script.name = scriptName;
				script.callback = ClayManager::GetInstance().scriptsByName.at(script.name);
			}
		}
	}
}

void Serializer::SerializeRigidBody(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<RigidBody>())
	{
		return;
	}

	const RigidBody& rigidBody = entity->GetComponent<RigidBody>();

	out << YAML::Key << "RigidBody";

	out << YAML::BeginMap;

	out << YAML::Key << "IsStatic" << YAML::Value << rigidBody.isStatic;
	out << YAML::Key << "Mass" << YAML::Value << rigidBody.mass;
	out << YAML::Key << "Type" << YAML::Value << (int)rigidBody.type;
	
	switch (rigidBody.type)
	{
	case RigidBody::Type::Box:
	{
		out << YAML::Key << "HalfExtents" << YAML::Value << rigidBody.shape.box.halfExtents;
		break;
	}
	case RigidBody::Type::Sphere:
	{
		out << YAML::Key << "Radius" << YAML::Value << rigidBody.shape.sphere.radius;
		break;
	}
	case RigidBody::Type::Cylinder:
	{
		out << YAML::Key << "HalfHeight" << YAML::Value << rigidBody.shape.cylinder.halfHeight;
		out << YAML::Key << "Radius" << YAML::Value << rigidBody.shape.cylinder.radius;
		break;
	}
	}

	auto& joltPhysicsSystem = entity->GetScene()->GetPhysicsSystem()->GetInstance();

	out << YAML::Key << "AngularVelocity" << YAML::Value << JoltVec3ToGlmVec3(joltPhysicsSystem.GetBodyInterface().GetAngularVelocity(rigidBody.id));
	out << YAML::Key << "LinearVelocity" << YAML::Value << JoltVec3ToGlmVec3(joltPhysicsSystem.GetBodyInterface().GetLinearVelocity(rigidBody.id));
	out << YAML::Key << "Friction" << YAML::Value << joltPhysicsSystem.GetBodyInterface().GetFriction(rigidBody.id);
	out << YAML::Key << "Restitution" << YAML::Value << joltPhysicsSystem.GetBodyInterface().GetRestitution(rigidBody.id);
	
	JPH::BodyLockWrite lock(joltPhysicsSystem.GetBodyLockInterface(), rigidBody.id);
	if (lock.Succeeded())
	{
		JPH::Body& body = lock.GetBody();
		out << YAML::Key << "AllowSleeping" << YAML::Value << body.GetAllowSleeping();
	}
	lock.ReleaseLock();

	out << YAML::EndMap;
}

void Serializer::DeserializeRigidBody(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& rigidBodyData = in["RigidBody"])
	{
		if (!entity->HasComponent<RigidBody>())
		{
			entity->AddComponent<RigidBody>();
		}

		auto& rigidBody = entity->GetComponent<RigidBody>();
		
		if (const auto& isStaticData = rigidBodyData["IsStatic"])
		{
			rigidBody.isStatic = isStaticData.as<bool>();
		}

		if (const auto& massData = rigidBodyData["Mass"])
		{
			rigidBody.mass = massData.as<float>();
		}

		if (const auto& typeData = rigidBodyData["Type"])
		{
			rigidBody.type = (RigidBody::Type)typeData.as<int>();
		}

		switch (rigidBody.type)
		{
		case RigidBody::Type::Box:
		{
			if (const auto& halfExtentsData = rigidBodyData["HalfExtents"])
			{
				rigidBody.shape.box.halfExtents = halfExtentsData.as<glm::vec3>();
			}
			break;
		}
		case RigidBody::Type::Sphere:
		{
			if (const auto& radiusData = rigidBodyData["Radius"])
			{
				rigidBody.shape.sphere.radius = radiusData.as<float>();
			}
			break;
		}
		case RigidBody::Type::Cylinder:
		{
			if (const auto& halfHeightData = rigidBodyData["HalfHeight"])
			{
				rigidBody.shape.cylinder.halfHeight = halfHeightData.as<float>();
			}

			if (const auto& radiusData = rigidBodyData["Radius"])
			{
				rigidBody.shape.cylinder.radius = radiusData.as<float>();
			}
			break;
		}
		}

		const auto physicsSystem = entity->GetScene()->GetPhysicsSystem();
		auto& joltPhysicsSystem = physicsSystem->GetInstance();

		physicsSystem->UpdateBodies(entity->GetScene());

		if (const auto& angularVelocityData = rigidBodyData["AngularVelocity"])
		{
			joltPhysicsSystem.GetBodyInterface().SetAngularVelocity(rigidBody.id, GlmVec3ToJoltVec3(angularVelocityData.as<glm::vec3>()));
		}

		if (const auto& linearVelocityData = rigidBodyData["LinearVelocity"])
		{
			joltPhysicsSystem.GetBodyInterface().SetLinearVelocity(rigidBody.id, GlmVec3ToJoltVec3(linearVelocityData.as<glm::vec3>()));
		}

		if (const auto& frictionData = rigidBodyData["Friction"])
		{
			joltPhysicsSystem.GetBodyInterface().SetFriction(rigidBody.id, frictionData.as<float>());
		}

		if (const auto& restitutionData = rigidBodyData["Restitution"])
		{
			joltPhysicsSystem.GetBodyInterface().SetRestitution(rigidBody.id, restitutionData.as<float>());
		}

		JPH::BodyLockWrite lock(joltPhysicsSystem.GetBodyLockInterface(), rigidBody.id);
		if (lock.Succeeded())
		{
			JPH::Body& body = lock.GetBody();

			if (const auto& allowSleepingData = rigidBodyData["AllowSleeping"])
			{
				body.SetAllowSleeping(allowSleepingData.as<bool>());
			}
		}
		lock.ReleaseLock();
	}
}

void Serializer::SerializeDecal(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<Decal>())
	{
		return;
	}

	const Decal& decal = entity->GetComponent<Decal>();

	out << YAML::Key << "Decal";

	out << YAML::BeginMap;

	if (decal.material)
	{
		UUID uuid = Utils::FindUuid(decal.material->GetFilepath());
		if (uuid.IsValid())
		{
			out << YAML::Key << "Material" << YAML::Value << uuid;
		}
	}

	out << YAML::Key << "ObjectVisibilityMask" << YAML::Value << (uint32_t)decal.objectVisibilityMask;

	out << YAML::EndMap;
}

void Serializer::DeserializeDecal(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	if (const auto& decalData = in["Decal"])
	{
		if (!entity->HasComponent<Decal>())
		{
			entity->AddComponent<Decal>();
		}

		Decal& decal = entity->GetComponent<Decal>();

		if (const auto& materialData = decalData["Material"])
		{
			const UUID uuid = materialData.as<UUID>();

			AsyncAssetLoader::GetInstance().AsyncLoadMaterial(Utils::FindFilepath(uuid), [wEntity = std::weak_ptr(entity)](std::weak_ptr<Material> material)
			{
				std::shared_ptr<Material> sharedMaterial = material.lock();
				if (!material.lock())
				{
					return;
				}

				if (std::shared_ptr<Entity> entity = wEntity.lock())
				{
					if (entity->HasComponent<Decal>())
					{
						entity->GetComponent<Decal>().material = sharedMaterial;
					}
				}
				else
				{
					auto callback = [material]()
					{
						std::shared_ptr<Material> sharedMaterial = material.lock();
						MaterialManager::GetInstance().DeleteMaterial(sharedMaterial);
					};

					std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, nullptr);
					EventSystem::GetInstance().SendEvent(event);
				}
			});
		}

		if (const auto& objectVisibilityMaskData = decalData["ObjectVisibilityMask"])
		{
			decal.objectVisibilityMask = objectVisibilityMaskData.as<uint32_t>();
		}
	}
}

void Serializer::SerializeUserComponents(YAML::Emitter& out, const std::shared_ptr<Entity>& entity)
{
	std::function<void(void*, const ReflectionSystem::RegisteredClass&, size_t)> serialize = [&out, &serialize]
		(void* instance, const ReflectionSystem::RegisteredClass& registeredClass, size_t offset)
	{
		for (auto& [name, prop] : registeredClass.m_PropertiesByName)
		{
#define SERIALIZE_PROPERTY(_type) \
if (prop.IsValue<_type>()) \
{ \
	const _type& value = prop.GetValue<_type>(instance, offset); \
	out << YAML::Key << "Type" << prop.m_Type; \
	out << YAML::Key << "Value" << value; \
}

			out << YAML::Key << name;
			out << YAML::BeginMap;

			SERIALIZE_PROPERTY(bool)
			else SERIALIZE_PROPERTY(float)
			else SERIALIZE_PROPERTY(int)
			else SERIALIZE_PROPERTY(double)
			else SERIALIZE_PROPERTY(std::string)
			else SERIALIZE_PROPERTY(glm::vec2)
			else SERIALIZE_PROPERTY(glm::vec3)
			else SERIALIZE_PROPERTY(glm::vec4)
			else SERIALIZE_PROPERTY(glm::ivec2)
			else SERIALIZE_PROPERTY(glm::ivec3)
			else SERIALIZE_PROPERTY(glm::ivec4)

			out << YAML::EndMap;
		}

		for (const auto& parent : registeredClass.m_Parents)
		{
			ReflectionSystem& reflectionSystem = ReflectionSystem::GetInstance();
			auto classByType = reflectionSystem.m_ClassesByType.find(parent.first);
			if (classByType != reflectionSystem.m_ClassesByType.end())
			{
				serialize(instance, classByType->second, parent.second);
			}
		}
	};

	ReflectionSystem& reflectionSystem = ReflectionSystem::GetInstance();

	for (auto [id, storage] : entity->GetRegistry().storage())
	{
		if (storage.contains(entity->GetHandle()))
		{
			auto classByType = reflectionSystem.m_ClassesByType.find(id);
			if (classByType == reflectionSystem.m_ClassesByType.end())
			{
				continue;
			}

			const auto& registeredClass = classByType->second;
			void* component = storage.value(entity->GetHandle());

			out << YAML::Key << registeredClass.m_TypeInfo.name().data();

			out << YAML::BeginMap;

			serialize(component, classByType->second, 0);

			if (registeredClass.m_SerializeCallback)
			{
				registeredClass.m_SerializeCallback(&out, component, entity.get());
			}

			out << YAML::EndMap;
		}
	}
}

void Serializer::DeserializeUserComponents(const YAML::Node& in, const std::shared_ptr<Entity>& entity)
{
	std::function<void(const YAML::Node&, void*, ReflectionSystem::RegisteredClass&, size_t)> deserialize = [&deserialize]
	(const YAML::Node& in, void* instance, ReflectionSystem::RegisteredClass& registeredClass, size_t offset)
	{
		ReflectionSystem& reflectionSystem = ReflectionSystem::GetInstance();

		for (auto& [name, prop] : registeredClass.m_PropertiesByName)
		{
			if (auto& propData = in[name])
			{
				const std::string type = propData["Type"].as<std::string>();

#define DESERIALIZE_PROPERTY(_type)             \
if (GetTypeName<_type>() == type)               \
{                                               \
	_type value = propData["Value"].as<_type>();\
	prop.SetValue(instance, value, offset);     \
}                                               \

				DESERIALIZE_PROPERTY(bool)
				else DESERIALIZE_PROPERTY(float)
				else DESERIALIZE_PROPERTY(int)
				else DESERIALIZE_PROPERTY(double)
				else DESERIALIZE_PROPERTY(std::string)
				else DESERIALIZE_PROPERTY(glm::vec2)
				else DESERIALIZE_PROPERTY(glm::vec3)
				else DESERIALIZE_PROPERTY(glm::vec4)
				else DESERIALIZE_PROPERTY(glm::ivec2)
				else DESERIALIZE_PROPERTY(glm::ivec3)
				else DESERIALIZE_PROPERTY(glm::ivec4)
			}
		}

		for (const auto& parent : registeredClass.m_Parents)
		{
			ReflectionSystem& reflectionSystem = ReflectionSystem::GetInstance();
			auto classByType = reflectionSystem.m_ClassesByType.find(parent.first);
			if (classByType != reflectionSystem.m_ClassesByType.end())
			{
				deserialize(in, instance, classByType->second, parent.second);
			}
		}
	};

	ReflectionSystem& reflectionSystem = ReflectionSystem::GetInstance();
	for (auto& [id, registeredClass] : reflectionSystem.m_ClassesByType)
	{
		if (const auto& userComponentData = in[registeredClass.m_TypeInfo.name().data()])
		{
			void* component = registeredClass.m_CreateCallback(entity->GetRegistry(), entity->GetHandle());
			deserialize(userComponentData, component, registeredClass, 0);

			if (registeredClass.m_DeserializeCallback)
			{
				registeredClass.m_DeserializeCallback((void*)&userComponentData, component, entity.get());
			}
		}
	}
}

void Serializer::SerializeScene(const std::filesystem::path& filepath, const std::shared_ptr<Scene>& scene)
{
	if (!scene)
	{
		Logger::Error("Failed to save a scene, the scene is invalid!");
		return;
	}

	YAML::Emitter out;

	out << YAML::BeginMap;

	// Settings.
	out << YAML::Key << "Settings";
	out << YAML::Value << YAML::BeginMap;

	out << YAML::Key << "DrawBoundingBoxes" << YAML::Value << scene->GetSettings().drawBoundingBoxes;
	out << YAML::Key << "DrawPhysicsShapes" << YAML::Value << scene->GetSettings().drawPhysicsShapes;

	// Wind Settings.
	out << YAML::Key << "Wind";
	out << YAML::Value << YAML::BeginMap;

	const Scene::WindSettings& windSettings = scene->GetWindSettings();
	out << YAML::Key << "Direction" << YAML::Value << windSettings.direction;
	out << YAML::Key << "Frequency" << YAML::Value << windSettings.frequency;
	out << YAML::Key << "Strength" << YAML::Value << windSettings.strength;

	out << YAML::EndMap;
	//

	out << YAML::EndMap;
	//

	// Graphics Settings.
	const UUID graphicsSettingsUuid = Utils::FindUuid(scene->GetGraphicsSettings().GetFilepath());
	if (graphicsSettingsUuid.IsValid())
	{
		out << YAML::Key << "GraphicsSettings" << YAML::Value << graphicsSettingsUuid;
	}
	//

	// Entities.
	out << YAML::Key << "Scene";
	out << YAML::Value << YAML::BeginSeq;

	for (const auto& entity : scene->GetEntities())
	{
		//if (entity->IsPrefab())
		//{
		//	SerializePrefab(Utils::FindFilepath(entity->GetPrefabFilepathUUID()), entity);
		//}

		if (!entity->HasParent())
		{
			SerializeEntity(out, entity, true, false);
		}
	}

	out << YAML::EndSeq;
	//

	out << YAML::EndMap;

	std::ofstream fout(filepath);
	fout << out.c_str();
	fout.close();

	Logger::Log("Scene:" + filepath.string() + " has been saved!", BOLDGREEN);
}

std::shared_ptr<Scene> Serializer::DeserializeScene(const std::filesystem::path& filepath)
{
	if (filepath.empty() || filepath == none || Utils::GetFileFormat(filepath) != FileFormats::Scene())
	{
		Logger::Error(filepath.string() + ":Failed to load scene! Filepath is incorrect!");
		return nullptr;
	}

	std::ifstream stream(filepath);
	std::stringstream stringStream;

	stringStream << stream.rdbuf();

	stream.close();

	YAML::Node data = YAML::LoadMesh(stringStream.str());
	if (!data)
	{
		FATAL_ERROR(filepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
	}

	std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create(
		Utils::GetFilename(filepath),
		"Main");
	scene->SetFilepath(filepath);

	for (const auto& entityData : data["Scene"])
	{
		DeserializeEntity(entityData, scene);
	}

	if (const auto& settingsData = data["Settings"])
	{
		if (const auto& drawBoundingBoxesData = settingsData["DrawBoundingBoxes"])
		{
			scene->GetSettings().drawBoundingBoxes = drawBoundingBoxesData.as<bool>();
		}

		if (const auto& drawPhysicsShapesData = settingsData["DrawPhysicsShapes"])
		{
			scene->GetSettings().drawPhysicsShapes = drawPhysicsShapesData.as<bool>();
		}

		if (const auto& windSettingsData = settingsData["Wind"])
		{
			Scene::WindSettings windSettings{};
			if (const auto& directionData = windSettingsData["Direction"])
			{
				windSettings.direction = directionData.as<glm::vec3>();
			}

			if (const auto& frequencyData = windSettingsData["Frequency"])
			{
				windSettings.frequency = frequencyData.as<float>();
			}

			if (const auto& strengthData = windSettingsData["Strength"])
			{
				windSettings.strength = strengthData.as<float>();
			}
			scene->SetWindSettings(windSettings);
		}
	}

	if (const auto& graphicsSettingsUUIDData = data["GraphicsSettings"])
	{
		const std::filesystem::path& graphicsSettingsFilepath = Utils::FindFilepath(graphicsSettingsUUIDData.as<UUID>());
		if (!graphicsSettingsFilepath.empty())
		{
			scene->SetGraphicsSettings(DeserializeGraphicsSettings(graphicsSettingsFilepath));
		}
	}

	AsyncAssetLoader::GetInstance().WaitIdle();

	Logger::Log("Scene:" + filepath.string() + " has been loaded!", BOLDGREEN);

	return scene;
}

void Serializer::SerializeGraphicsSettings(const GraphicsSettings& graphicsSettings)
{
	YAML::Emitter out;

	out << YAML::BeginMap;

	// SSAO.
	out << YAML::Key << "SSAO";
	out << YAML::Value << YAML::BeginMap;

	out << YAML::Key << "IsEnabled" << YAML::Value << graphicsSettings.ssao.isEnabled;
	out << YAML::Key << "AoScale" << YAML::Value << graphicsSettings.ssao.aoScale;
	out << YAML::Key << "Bias" << YAML::Value << graphicsSettings.ssao.bias;
	out << YAML::Key << "KernelSize" << YAML::Value << graphicsSettings.ssao.kernelSize;
	out << YAML::Key << "NoiseSize" << YAML::Value << graphicsSettings.ssao.noiseSize;
	out << YAML::Key << "Radius" << YAML::Value << graphicsSettings.ssao.radius;
	out << YAML::Key << "ResolutionScale" << YAML::Value << graphicsSettings.ssao.resolutionScale;
	out << YAML::Key << "ResolutionBlurScale" << YAML::Value << graphicsSettings.ssao.resolutionBlurScale;

	out << YAML::EndMap;
	//

	// CSM.
	out << YAML::Key << "CSM";
	out << YAML::Value << YAML::BeginMap;

	out << YAML::Key << "IsEnabled" << YAML::Value << graphicsSettings.shadows.csm.isEnabled;
	out << YAML::Key << "Quality" << YAML::Value << graphicsSettings.shadows.csm.quality;
	out << YAML::Key << "CascadeCount" << YAML::Value << graphicsSettings.shadows.csm.cascadeCount;
	out << YAML::Key << "SplitFactor" << YAML::Value << graphicsSettings.shadows.csm.splitFactor;
	out << YAML::Key << "MaxDistance" << YAML::Value << graphicsSettings.shadows.csm.maxDistance;
	out << YAML::Key << "FogFactor" << YAML::Value << graphicsSettings.shadows.csm.fogFactor;
	out << YAML::Key << "Filter" << YAML::Value << (int)graphicsSettings.shadows.csm.filter;
	out << YAML::Key << "PcfRange" << YAML::Value << graphicsSettings.shadows.csm.pcfRange;
	out << YAML::Key << "Biases" << YAML::Value << graphicsSettings.shadows.csm.biases;
	out << YAML::Key << "StabilizeCascades" << YAML::Value << graphicsSettings.shadows.csm.stabilizeCascades;

	out << YAML::EndMap;
	//

	// SSS.
	out << YAML::Key << "SSS";
	out << YAML::Value << YAML::BeginMap;

	out << YAML::Key << "IsEnabled" << YAML::Value << graphicsSettings.shadows.sss.isEnabled;
	out << YAML::Key << "ResolutionScale" << YAML::Value << graphicsSettings.shadows.sss.resolutionScale;
	out << YAML::Key << "ResolutionBlurScale" << YAML::Value << graphicsSettings.shadows.sss.resolutionBlurScale;
	out << YAML::Key << "MaxRayDistance" << YAML::Value << graphicsSettings.shadows.sss.maxRayDistance;
	out << YAML::Key << "MaxDistance" << YAML::Value << graphicsSettings.shadows.sss.maxDistance;
	out << YAML::Key << "MaxSteps" << YAML::Value << graphicsSettings.shadows.sss.maxSteps;
	out << YAML::Key << "MinThickness" << YAML::Value << graphicsSettings.shadows.sss.minThickness;
	out << YAML::Key << "MaxThickness" << YAML::Value << graphicsSettings.shadows.sss.maxThickness;

	out << YAML::EndMap;
	//

	// PointLightShadows.
	out << YAML::Key << "PointLightShadows";
	out << YAML::Value << YAML::BeginMap;

	out << YAML::Key << "IsEnabled" << YAML::Value << graphicsSettings.shadows.pointLightShadows.isEnabled;
	out << YAML::Key << "AtlasQuality" << YAML::Value << graphicsSettings.shadows.pointLightShadows.atlasQuality;
	out << YAML::Key << "FaceQuality" << YAML::Value << graphicsSettings.shadows.pointLightShadows.faceQuality;

	out << YAML::EndMap;
	//

	// SpotLightShadows.
	out << YAML::Key << "SpotLightShadows";
	out << YAML::Value << YAML::BeginMap;

	out << YAML::Key << "IsEnabled" << YAML::Value << graphicsSettings.shadows.spotLightShadows.isEnabled;
	out << YAML::Key << "AtlasQuality" << YAML::Value << graphicsSettings.shadows.spotLightShadows.atlasQuality;
	out << YAML::Key << "FaceQuality" << YAML::Value << graphicsSettings.shadows.spotLightShadows.faceQuality;

	out << YAML::EndMap;
	//

	// Bloom.
	out << YAML::Key << "Bloom";
	out << YAML::Value << YAML::BeginMap;

	out << YAML::Key << "IsEnabled" << YAML::Value << graphicsSettings.bloom.isEnabled;
	out << YAML::Key << "MipCount" << YAML::Value << graphicsSettings.bloom.mipCount;
	out << YAML::Key << "BrightnessThreshold" << YAML::Value << graphicsSettings.bloom.brightnessThreshold;
	out << YAML::Key << "Intensity" << YAML::Value << graphicsSettings.bloom.intensity;
	out << YAML::Key << "ResolutionScale" << YAML::Value << graphicsSettings.bloom.resolutionScale;

	out << YAML::EndMap;
	//

	// SSR.
	out << YAML::Key << "SSR";
	out << YAML::Value << YAML::BeginMap;

	out << YAML::Key << "IsEnabled" << YAML::Value << graphicsSettings.ssr.isEnabled;
	out << YAML::Key << "MaxDistance" << YAML::Value << graphicsSettings.ssr.maxDistance;
	out << YAML::Key << "Resolution" << YAML::Value << graphicsSettings.ssr.resolution;
	out << YAML::Key << "ResolutionBlurScale" << YAML::Value << graphicsSettings.ssr.resolutionBlurScale;
	out << YAML::Key << "ResolutionScale" << YAML::Value << graphicsSettings.ssr.resolutionScale;
	out << YAML::Key << "StepCount" << YAML::Value << graphicsSettings.ssr.stepCount;
	out << YAML::Key << "Thickness" << YAML::Value << graphicsSettings.ssr.thickness;
	out << YAML::Key << "BlurRange" << YAML::Value << graphicsSettings.ssr.blurRange;
	out << YAML::Key << "BlurOffset" << YAML::Value << graphicsSettings.ssr.blurOffset;
	out << YAML::Key << "MipMultiplier" << YAML::Value << graphicsSettings.ssr.mipMultiplier;
	out << YAML::Key << "UseSkyBoxFallback" << YAML::Value << graphicsSettings.ssr.useSkyBoxFallback;
	out << YAML::Key << "Blur" << YAML::Value << (int)graphicsSettings.ssr.blur;

	out << YAML::EndMap;
	//

	// Post Process.
	out << YAML::Key << "PostProcess";
	out << YAML::Value << YAML::BeginMap;

	out << YAML::Key << "Gamma" << YAML::Value << graphicsSettings.postProcess.gamma;
	out << YAML::Key << "ToneMapper" << YAML::Value << (int)graphicsSettings.postProcess.toneMapper;
	out << YAML::Key << "FXAA" << YAML::Value << graphicsSettings.postProcess.fxaa;

	out << YAML::EndMap;
	//

	out << YAML::EndMap;

	std::ofstream fout(graphicsSettings.GetFilepath());
	fout << out.c_str();
	fout.close();
}

GraphicsSettings Serializer::DeserializeGraphicsSettings(const std::filesystem::path& filepath)
{
	GraphicsSettings graphicsSettings(Utils::GetFilename(filepath), filepath);

	if (filepath.empty() || filepath == none || Utils::GetFileFormat(filepath) != FileFormats::GraphicsSettings())
	{
		Logger::Error(filepath.string() + ":Failed to load scene! Filepath is incorrect!");
		return graphicsSettings;
	}

	std::ifstream stream(filepath);
	std::stringstream stringStream;

	stringStream << stream.rdbuf();

	stream.close();

	YAML::Node data = YAML::LoadMesh(stringStream.str());
	if (!data)
	{
		FATAL_ERROR(filepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
	}

	if (const auto& ssaoData = data["SSAO"])
	{
		if (const auto& isEnabledData = ssaoData["IsEnabled"])
		{
			graphicsSettings.ssao.isEnabled = isEnabledData.as<bool>();
		}

		if (const auto& aoScaleData = ssaoData["AoScale"])
		{
			graphicsSettings.ssao.aoScale = aoScaleData.as<float>();
		}

		if (const auto& biasData = ssaoData["Bias"])
		{
			graphicsSettings.ssao.bias = biasData.as<float>();
		}

		if (const auto& kernelSizeData = ssaoData["KernelSize"])
		{
			graphicsSettings.ssao.kernelSize = kernelSizeData.as<int>();
		}

		if (const auto& noiseSizeData = ssaoData["NoiseSize"])
		{
			graphicsSettings.ssao.noiseSize = noiseSizeData.as<int>();
		}

		if (const auto& radiusData = ssaoData["Radius"])
		{
			graphicsSettings.ssao.radius = radiusData.as<float>();
		}

		if (const auto& resolutionScaleData = ssaoData["ResolutionScale"])
		{
			graphicsSettings.ssao.resolutionScale = glm::clamp(resolutionScaleData.as<int>(), 0, 3);
		}

		if (const auto& resolutionBlurScaleData = ssaoData["ResolutionBlurScale"])
		{
			graphicsSettings.ssao.resolutionBlurScale = glm::clamp(resolutionBlurScaleData.as<int>(), 0, 3);
		}
	}

	if (const auto& csmData = data["CSM"])
	{
		if (const auto& isEnabledData = csmData["IsEnabled"])
		{
			graphicsSettings.shadows.csm.isEnabled = isEnabledData.as<bool>();
		}

		if (const auto& qualityData = csmData["Quality"])
		{
			constexpr int maxQualityIndex = 2;
			graphicsSettings.shadows.csm.quality = std::min(qualityData.as<int>(), maxQualityIndex);
		}

		if (const auto& cascadeCountData = csmData["CascadeCount"])
		{
			graphicsSettings.shadows.csm.cascadeCount = cascadeCountData.as<int>();
		}

		if (const auto& splitFactorData = csmData["SplitFactor"])
		{
			graphicsSettings.shadows.csm.splitFactor = splitFactorData.as<float>();
		}

		if (const auto& maxDistanceData = csmData["MaxDistance"])
		{
			graphicsSettings.shadows.csm.maxDistance = maxDistanceData.as<float>();
		}

		if (const auto& biasesData = csmData["Biases"])
		{
			graphicsSettings.shadows.csm.biases = biasesData.as<std::vector<float>>();
		}

		if (const auto& fogFactorData = csmData["FogFactor"])
		{
			graphicsSettings.shadows.csm.fogFactor = fogFactorData.as<float>();
		}

		if (const auto& filterData = csmData["Filter"])
		{
			graphicsSettings.shadows.csm.filter = (GraphicsSettings::Shadows::CSM::Filter)filterData.as<int>();
		}

		if (const auto& pcfRangeData = csmData["PcfRange"])
		{
			graphicsSettings.shadows.csm.pcfRange = pcfRangeData.as<int>();
		}

		if (const auto& stabilizeCascadesData = csmData["StabilizeCascades"])
		{
			graphicsSettings.shadows.csm.stabilizeCascades = stabilizeCascadesData.as<bool>();
		}
	}

	if (const auto& sssData = data["SSS"])
	{
		if (const auto& isEnabledData = sssData["IsEnabled"])
		{
			graphicsSettings.shadows.sss.isEnabled = isEnabledData.as<bool>();
		}

		if (const auto& resolutionScaleData = sssData["ResolutionScale"])
		{
			graphicsSettings.shadows.sss.resolutionScale = glm::clamp(resolutionScaleData.as<int>(), 0, 3);
		}

		if (const auto& resolutionBlurScaleData = sssData["ResolutionBlurScale"])
		{
			graphicsSettings.shadows.sss.resolutionBlurScale = glm::clamp(resolutionBlurScaleData.as<int>(), 0, 3);
		}

		if (const auto& maxRayDistanceData = sssData["MaxRayDistance"])
		{
			graphicsSettings.shadows.sss.maxRayDistance = maxRayDistanceData.as<float>();
		}

		if (const auto& maxDistanceData = sssData["MaxDistance"])
		{
			graphicsSettings.shadows.sss.maxDistance = maxDistanceData.as<float>();
		}

		if (const auto& maxStepsData = sssData["MaxSteps"])
		{
			graphicsSettings.shadows.sss.maxSteps = maxStepsData.as<int>();
		}

		if (const auto& minThicknessData = sssData["MinThickness"])
		{
			graphicsSettings.shadows.sss.minThickness = minThicknessData.as<float>();
		}

		if (const auto& maxThicknessData = sssData["MaxThickness"])
		{
			graphicsSettings.shadows.sss.maxThickness = maxThicknessData.as<float>();
		}
	}

	if (const auto& pointLightShadowsData = data["PointLightShadows"])
	{
		if (const auto& isEnabledData = pointLightShadowsData["IsEnabled"])
		{
			graphicsSettings.shadows.pointLightShadows.isEnabled = isEnabledData.as<bool>();
		}

		if (const auto& atlasQualityData = pointLightShadowsData["AtlasQuality"])
		{
			graphicsSettings.shadows.pointLightShadows.atlasQuality = glm::clamp(atlasQualityData.as<int>(), 0, 3);
		}

		if (const auto& faceQualityData = pointLightShadowsData["FaceQuality"])
		{
			graphicsSettings.shadows.pointLightShadows.faceQuality = glm::clamp(faceQualityData.as<int>(), 0, 3);
		}
	}

	if (const auto& spotLightShadowsData = data["SpotLightShadows"])
	{
		if (const auto& isEnabledData = spotLightShadowsData["IsEnabled"])
		{
			graphicsSettings.shadows.spotLightShadows.isEnabled = isEnabledData.as<bool>();
		}

		if (const auto& atlasQualityData = spotLightShadowsData["AtlasQuality"])
		{
			graphicsSettings.shadows.spotLightShadows.atlasQuality = glm::clamp(atlasQualityData.as<int>(), 0, 3);
		}

		if (const auto& faceQualityData = spotLightShadowsData["FaceQuality"])
		{
			graphicsSettings.shadows.spotLightShadows.faceQuality = glm::clamp(faceQualityData.as<int>(), 0, 3);
		}
	}

	if (const auto& bloomData = data["Bloom"])
	{
		if (const auto& isEnabledData = bloomData["IsEnabled"])
		{
			graphicsSettings.bloom.isEnabled = isEnabledData.as<bool>();
		}

		if (const auto& mipCountData = bloomData["MipCount"])
		{
			graphicsSettings.bloom.mipCount = mipCountData.as<int>();
		}

		if (const auto& brightnessThresholdData = bloomData["BrightnessThreshold"])
		{
			graphicsSettings.bloom.brightnessThreshold = brightnessThresholdData.as<float>();
		}

		if (const auto& intensityData = bloomData["Intensity"])
		{
			graphicsSettings.bloom.intensity = intensityData.as<float>();
		}

		if (const auto& resolutionScaleData = bloomData["ResolutionScale"])
		{
			graphicsSettings.bloom.resolutionScale = glm::clamp(resolutionScaleData.as<int>(), 0, 3);
		}
	}

	if (const auto& ssrData = data["SSR"])
	{
		if (const auto& isEnabledData = ssrData["IsEnabled"])
		{
			graphicsSettings.ssr.isEnabled = isEnabledData.as<bool>();
		}

		if (const auto& maxDistanceData = ssrData["MaxDistance"])
		{
			graphicsSettings.ssr.maxDistance = maxDistanceData.as<float>();
		}

		if (const auto& resolutionData = ssrData["Resolution"])
		{
			graphicsSettings.ssr.resolution = resolutionData.as<float>();
		}

		if (const auto& resolutionBlurScaleData = ssrData["ResolutionBlurScale"])
		{
			graphicsSettings.ssr.resolutionBlurScale = glm::clamp(resolutionBlurScaleData.as<int>(), 0, 4);
		}

		if (const auto& resolutionScaleData = ssrData["ResolutionScale"])
		{
			graphicsSettings.ssr.resolutionScale = glm::clamp(resolutionScaleData.as<int>(), 0, 3);
		}

		if (const auto& stepCountData = ssrData["StepCount"])
		{
			graphicsSettings.ssr.stepCount = stepCountData.as<int>();
		}

		if (const auto& thicknessData = ssrData["Thickness"])
		{
			graphicsSettings.ssr.thickness = thicknessData.as<float>();
		}

		if (const auto& blurRangeData = ssrData["BlurRange"])
		{
			graphicsSettings.ssr.blurRange = blurRangeData.as<int>();
		}

		if (const auto& blurOffsetData = ssrData["BlurOffset"])
		{
			graphicsSettings.ssr.blurOffset = blurOffsetData.as<int>();
		}

		if (const auto& mipMultiplierData = ssrData["MipMultiplier"])
		{
			graphicsSettings.ssr.mipMultiplier = mipMultiplierData.as<float>();
		}

		if (const auto& useSkyBoxFallbackData = ssrData["UseSkyBoxFallback"])
		{
			graphicsSettings.ssr.useSkyBoxFallback = useSkyBoxFallbackData.as<bool>();
		}

		if (const auto& blurData = ssrData["Blur"])
		{
			graphicsSettings.ssr.blur = (GraphicsSettings::SSR::Blur)blurData.as<int>();
		}
	}

	if (const auto& postProcessData = data["PostProcess"])
	{
		if (const auto& gammaData = postProcessData["Gamma"])
		{
			graphicsSettings.postProcess.gamma = gammaData.as<float>();
		}

		if (const auto& toneMapperData = postProcessData["ToneMapper"])
		{
			graphicsSettings.postProcess.toneMapper = (GraphicsSettings::PostProcess::ToneMapper)toneMapperData.as<int>();
		}

		if (const auto& fxaaData = postProcessData["FXAA"])
		{
			graphicsSettings.postProcess.fxaa = fxaaData.as<bool>();
		}
	}

	return graphicsSettings;
}

void Serializer::SerializeTextureMeta(const Texture::Meta& meta)
{
	if (meta.filepath.empty())
	{
		Logger::Error("Failed to save texture meta, filepath is empty!");
	}

	YAML::Emitter out;

	out << YAML::BeginMap;

	out << YAML::Key << "UUID" << YAML::Value << meta.uuid;
	out << YAML::Key << "SRGB" << YAML::Value << meta.srgb;
	out << YAML::Key << "CreateMipMaps" << YAML::Value << meta.createMipMaps;

	out << YAML::EndMap;

	std::ofstream fout(meta.filepath);
	fout << out.c_str();
	fout.close();
}

std::optional<Texture::Meta> Serializer::DeserializeTextureMeta(const std::filesystem::path& filepath)
{
	if (filepath.empty())
	{
		Logger::Error("Failed to load texture meta, filepath is empty!");
		return std::nullopt;
	}

	if (!std::filesystem::exists(filepath))
	{
		Logger::Error(filepath.string() + ":Failed to load texture meta! The file doesn't exist");
		return std::nullopt;
	}

	std::ifstream stream(filepath);
	std::stringstream stringStream;

	stringStream << stream.rdbuf();

	stream.close();

	YAML::Node data = YAML::LoadMesh(stringStream.str());
	if (!data)
	{
		Logger::Error(filepath.string() + ":Failed to load yaml file! The file doesn't contain data or doesn't exist!");
		return std::nullopt;
	}

	Texture::Meta meta{};
	if (YAML::Node uuidData = data["UUID"])
	{
		meta.uuid = uuidData.as<UUID>();
	}

	if (YAML::Node srgbData = data["SRGB"])
	{
		meta.srgb = srgbData.as<bool>();
	}

	if (YAML::Node createMipMapsData = data["CreateMipMaps"])
	{
		meta.createMipMaps = createMipMapsData.as<bool>();
	}

	meta.filepath = filepath;

	return meta;
}

std::filesystem::path Serializer::DeserializeFilepath(const std::string& uuidOrFilepath)
{
	if (std::filesystem::exists(uuidOrFilepath))
	{
		return uuidOrFilepath;
	}

	std::filesystem::path filepath = Utils::FindFilepath(UUID::FromString(uuidOrFilepath));
	if (filepath.empty())
	{
		Logger::Error(uuidOrFilepath + ":Failed to deserialize filepath!");
	}

	return filepath;
}

void Serializer::SerializeThumbnailMeta(const std::filesystem::path& filepath, const size_t lastWriteTime)
{
	if (filepath.empty())
	{
		Logger::Error("Failed to save thumbnail meta, filepath is empty!");
	}

	std::ofstream out(filepath, std::ifstream::binary);
	out.write((char*)&lastWriteTime, static_cast<std::streamsize>(sizeof(lastWriteTime)));
	out.close();
}

size_t Serializer::DeserializeThumbnailMeta(const std::filesystem::path& filepath)
{
	if (filepath.empty())
	{
		Logger::Error("Failed to load thumbnail meta, filepath is empty!");
		return 0;
	}

	if (!std::filesystem::exists(filepath))
	{
		Logger::Error(filepath.string() + ":Failed to load thumbnail meta! The file doesn't exist");
		return 0;
	}

	std::ifstream in(filepath, std::ifstream::binary);

	in.seekg(0, std::ifstream::end);
	const int size = in.tellg();
	in.seekg(0, std::ifstream::beg);

	struct ThumbnailMeta
	{
		size_t lastWriteTime = 0;
	};

	ThumbnailMeta meta{};

	in.read((char*)&meta, size);

	in.close();

	return meta.lastWriteTime;
}

void Serializer::SerializeAnimationTrack(
	const std::filesystem::path& filepath,
	const EntityAnimator::AnimationTrack& animationTrack)
{
	if (filepath.empty())
	{
		Logger::Error("Failed to save animation track, filepath is empty!");
		return;
	}

	std::ofstream out(filepath, std::ios::binary);

	size_t keyframeCount = animationTrack.keyframes.size();
	out.write(reinterpret_cast<const char*>(&keyframeCount), sizeof(size_t));

	for (const auto& keyframe : animationTrack.keyframes)
	{
		out.write(reinterpret_cast<const char*>(&keyframe.time), sizeof(float));
		out.write(reinterpret_cast<const char*>(&keyframe.translation), sizeof(glm::vec3));
		out.write(reinterpret_cast<const char*>(&keyframe.rotation), sizeof(glm::vec3));
		out.write(reinterpret_cast<const char*>(&keyframe.scale), sizeof(glm::vec3));
		out.write(reinterpret_cast<const char*>(&keyframe.interpType), sizeof(EntityAnimator::Keyframe::InterpolationType));
	}

	out.close();
}

std::optional<EntityAnimator::AnimationTrack> Serializer::DeserializeAnimationTrack(const std::filesystem::path& filepath)
{
	if (filepath.empty())
	{
		Logger::Error("Failed to load animation track, filepath is empty!");
		return std::nullopt;
	}

	std::ifstream in(filepath, std::ios::binary);

	EntityAnimator::AnimationTrack animationTrack(Utils::GetFilename(filepath), filepath);

	size_t keyframeCount = 0;
	in.read(reinterpret_cast<char*>(&keyframeCount), sizeof(size_t));

	animationTrack.keyframes.resize(keyframeCount);
	for (size_t i = 0; i < keyframeCount; i++)
	{
		auto& keyframe = animationTrack.keyframes[i];

		in.read(reinterpret_cast<char*>(&keyframe.time), sizeof(float));
		in.read(reinterpret_cast<char*>(&keyframe.translation), sizeof(glm::vec3));
		in.read(reinterpret_cast<char*>(&keyframe.rotation), sizeof(glm::vec3));
		in.read(reinterpret_cast<char*>(&keyframe.scale), sizeof(glm::vec3));
		in.read(reinterpret_cast<char*>(&keyframe.interpType), sizeof(EntityAnimator::Keyframe::InterpolationType));
	}

	in.close();

	return animationTrack;
}

void Serializer::ParseUniformValues(
	const YAML::detail::iterator_value& data,
	Pipeline::UniformInfo& uniformsInfo)
{
	auto parseTextureName = [&uniformsInfo](const std::string& name)
	{
		if (name.empty())
		{
			return TextureManager::GetInstance().GetWhite()->GetName();
		}
		else if (std::filesystem::exists(name))
		{
			return name;
		}
		else
		{
			if (TextureManager::GetInstance().GetTexture(name))
			{
				return name;
			}
			else
			{
				const auto filepathByUUID = Utils::FindFilepath(UUID::FromString(name));
				if (!filepathByUUID.empty())
				{
					return filepathByUUID.string();	
				}
				else
				{
					return TextureManager::GetInstance().GetPink()->GetName();
				}
			}
		}
	};

	for (const auto& uniformData : data["Uniforms"])
	{
		std::string uniformName;
		if (const auto& nameData = uniformData["Name"])
		{
			uniformName = nameData.as<std::string>();
		}

		if (uniformData["Values"])
		{
			Pipeline::UniformBufferInfo uniformBufferInfo{};

			for (const auto& valueData : uniformData["Values"])
			{
				std::string valueName;
				if (const auto& nameData = valueData["Name"])
				{
					valueName = nameData.as<std::string>();
				}

				ShaderReflection::ReflectVariable::Type valueType;
				if (const auto& typeData = valueData["Type"])
				{
					valueType = ShaderReflection::ConvertStringToType(typeData.as<std::string>());
				}
				else
				{
					Logger::Warning("Failed to load type of " + valueName);
					continue;
				}

				if (const auto& bufferValueData = valueData["Value"])
				{
					if (valueType == ShaderReflection::ReflectVariable::Type::INT)
					{
						uniformBufferInfo.intValuesByName.emplace(valueName, bufferValueData.as<int>());
					}
					else if (valueType == ShaderReflection::ReflectVariable::Type::FLOAT)
					{
						uniformBufferInfo.floatValuesByName.emplace(valueName, bufferValueData.as<float>());
					}
					else if (valueType == ShaderReflection::ReflectVariable::Type::VEC2)
					{
						uniformBufferInfo.vec2ValuesByName.emplace(valueName, bufferValueData.as<glm::vec2>());
					}
					else if (valueType == ShaderReflection::ReflectVariable::Type::VEC3)
					{
						uniformBufferInfo.vec3ValuesByName.emplace(valueName, bufferValueData.as<glm::vec3>());
					}
					else if (valueType == ShaderReflection::ReflectVariable::Type::VEC4)
					{
						uniformBufferInfo.vec4ValuesByName.emplace(valueName, bufferValueData.as<glm::vec4>());
					}
					else if (valueType == ShaderReflection::ReflectVariable::Type::TEXTURE)
					{
						uniformBufferInfo.texturesByName.emplace(valueName, parseTextureName(bufferValueData.as<std::string>()));
					}
				}
			}
			uniformsInfo.uniformBuffersByName.emplace(uniformName, uniformBufferInfo);
		}

		if (const auto& nameData = uniformData["Value"])
		{
			uniformsInfo.texturesByName.emplace(uniformName, parseTextureName(nameData.as<std::string>()));
		}

		if (const auto& nameData = uniformData["TextureAttachment"])
		{
			std::string textureAttachmentName = nameData.as<std::string>();
			size_t openBracketIndex = textureAttachmentName.find_first_of('[');
			if (openBracketIndex != std::string::npos)
			{
				size_t closeBracketIndex = textureAttachmentName.find_last_of(']');
				if (closeBracketIndex != std::string::npos)
				{
					Pipeline::TextureAttachmentInfo textureAttachmentInfo{};

					const std::string attachmentIndexString = textureAttachmentName.substr(openBracketIndex + 1, closeBracketIndex - openBracketIndex - 1);
					textureAttachmentInfo.name = textureAttachmentName.substr(0, openBracketIndex);
					textureAttachmentInfo.attachmentIndex = std::stoul(attachmentIndexString);

					uniformsInfo.textureAttachmentsByName[uniformName] = textureAttachmentInfo;
				}
				else
				{
					Logger::Warning(uniformName + ": no close bracket for render target!");
				}
			}
			else
			{
				// If no attachment index then the texture should be a storage texture.
				Pipeline::TextureAttachmentInfo textureAttachmentInfo{};
				textureAttachmentInfo.name = textureAttachmentName;
				uniformsInfo.textureAttachmentsByName[uniformName] = textureAttachmentInfo;
			}
		}

		if (const auto& nameData = uniformData["TextureAttachmentDefault"])
		{
			uniformsInfo.textureAttachmentsByName[uniformName].defaultName = nameData.as<std::string>();
		}
	}
}
