#include "../Serializer.h"

#include "../Logger.h"
#include "../FileFormatNames.h"
#include "../TextureManager.h"
#include "../MaterialManager.h"
#include "../MeshManager.h"
#include "../Profiler.h"
#include "../ThreadPool.h"

#include "../../Utils/Utils.h"
#include "../../Graphics/Material.h"
#include "../../Graphics/Mesh.h"
#include "../../Graphics/Texture.h"
#include "../../Graphics/Skeleton.h"
#include "../../Graphics/SkeletalAnimation.h"
#include "../../Graphics/ShaderReflection.h"

#include <stbi/stb_image_write.h>

using namespace Pengine;

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