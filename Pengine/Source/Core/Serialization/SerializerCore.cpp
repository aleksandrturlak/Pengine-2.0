#include "../Serializer.h"

#include "../Logger.h"
#include "../FileFormatNames.h"
#include "../GraphicsSettings.h"
#include "../TextureManager.h"

#include "../../Utils/Utils.h"
#include "../../Configs/EngineConfig.h"
#include "../../Components/EntityAnimator.h"
#include "../../Graphics/ShaderReflection.h"

using namespace Pengine;

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

void Serializer::SerializeAnimationTrack(const std::filesystem::path& filepath, const EntityAnimator::AnimationTrack& animationTrack)
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