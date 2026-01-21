#pragma once

#include "Core.h"
#include "Entity.h"
#include "GraphicsSettings.h"

#include "../Configs/EngineConfig.h"
#include "../Graphics/SkeletalAnimation.h"
#include "../Graphics/Material.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/Pipeline.h"
#include "../Graphics/Skeleton.h"
#include "../Components/EntityAnimator.h"

#include "yaml-cpp/yaml.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

namespace Pengine
{

	class PENGINE_API Serializer
	{
	public:
		static EngineConfig DeserializeEngineConfig(const std::filesystem::path& filepath);

		static void GenerateFilesUUID(const std::filesystem::path& directory);

		static UUID GenerateFileUUID(const std::filesystem::path& filepath);

		//static void SerializeUUID();

		//static void LoadFilesUUID();

		static void DeserializeDescriptorSets(
			const YAML::detail::iterator_value& pipelineData,
			const std::string& passName,
			std::map<Pipeline::DescriptorSetIndexType, std::map<std::string, uint32_t>>& descriptorSetIndicesByType);

		static void DeserializeShaderFilepaths(
			const YAML::detail::iterator_value& pipelineData,
			std::map<ShaderModule::Type, std::filesystem::path>& shaderFilepathsByType);

		static void SerializeTexture(const std::filesystem::path& filepath, std::shared_ptr<Texture> texture, bool* isLoaded);

		static GraphicsPipeline::CreateGraphicsInfo DeserializeGraphicsPipeline(const YAML::detail::iterator_value& pipelineData);

		static ComputePipeline::CreateComputeInfo DeserializeComputePipeline(const YAML::detail::iterator_value& pipelineData);

		static BaseMaterial::CreateInfo LoadBaseMaterial(const std::filesystem::path& filepath);

		static Material::CreateInfo LoadMaterial(const std::filesystem::path& filepath);

		static void SerializeMaterial(const std::shared_ptr<Material>& material, bool useLog = true);

		static void SerializeMesh(const std::filesystem::path& directory, const std::shared_ptr<Mesh>& mesh);

		static Mesh::CreateInfo DeserializeMesh(const std::filesystem::path& filepath);

		static void SerializeSkeleton(const std::shared_ptr<Skeleton>& skeleton);

		static std::shared_ptr<Skeleton> DeserializeSkeleton(const std::filesystem::path& filepath);

		static void SerializeSkeletalAnimation(const std::shared_ptr<SkeletalAnimation>& skeletalAnimation);

		static std::shared_ptr<SkeletalAnimation> DeserializeSkeletalAnimation(const std::filesystem::path& filepath);

		static void SerializeShaderCache(const std::filesystem::path& filepath, const std::string& code);

		static std::string DeserializeShaderCache(const std::filesystem::path& filepath);

		static void SerializeShaderModuleReflection(const std::filesystem::path& filepath, const ShaderReflection::ReflectShaderModule& reflectShaderModule);

		static std::optional<ShaderReflection::ReflectShaderModule> DeserializeShaderModuleReflection(const std::filesystem::path& filepath);

		struct ImportInfo
		{
			std::filesystem::path filepath;
			std::vector<std::string> meshes;
			std::vector<std::string> materials;
			std::vector<std::string> animations;
			std::vector<std::string> skeletons;
			std::vector<std::string> prefabs;
		};

		static Serializer::ImportInfo GetImportInfo(const std::filesystem::path& filepath);

		struct ImportOptions
		{
			std::filesystem::path filepath;

			struct MeshOptions
			{
				bool import = true;
				bool skinned = false;
				int lodCount = 3;
				float targetError = 1e-2f;
				float minIndexCountFactor = 0.2f;
				glm::vec2 distanceMinMax = { 10.0f, 50.0f };
				glm::bvec2 flipUV = { false, false };
			} meshes;
			
			bool skeletons = true;
			bool materials = true;
			bool animations = true;
			bool prefabs = true;
			bool createFolder = false;
		};

		static std::unordered_map<std::shared_ptr<Material>, std::vector<std::shared_ptr<Mesh>>> LoadIntermediate(
			const ImportOptions options,
			std::string& workName,
			float& workStatus);

		static std::shared_ptr<Texture> LoadGltfTexture(
			const fastgltf::Asset& gltfAsset,
			const fastgltf::Texture& gltfTexture,
			const std::filesystem::path& texturesDirectory,
			const std::filesystem::path& directory,
			const std::string& debugName,
			std::optional<Texture::Meta> meta = std::nullopt);

		static std::optional<Mesh::CreateInfo> GenerateMesh(
			const Mesh::CreateInfo::SourceFileInfo& sourceFileInfo,
			const fastgltf::Asset& gltfAsset,
			const fastgltf::Primitive& gltfPrimitive,
			const std::string& name,
			const std::filesystem::path& directory,
			const ImportOptions::MeshOptions& options);

		//static std::optional<Mesh::CreateInfo> ReimportMesh(
		//	Mesh::CreateInfo);

		static void ProcessColors(
			const fastgltf::Asset& gltfAsset,
			const fastgltf::Accessor* colorAccessor,
			void* vertices,
			const size_t vertexSize,
			const size_t colorOffset);

		static std::shared_ptr<Skeleton> GenerateSkeleton(
			const fastgltf::Asset& gltfAsset,
			const fastgltf::Skin& gltfSkin,
			const std::filesystem::path& directory);

		static std::shared_ptr<SkeletalAnimation> GenerateAnimation(
			const fastgltf::Asset& gltfAsset,
			const fastgltf::Animation& animation,
			const std::filesystem::path& directory);

		static std::shared_ptr<Material> GenerateMaterial(
			const fastgltf::Asset& gltfAsset,
			const fastgltf::Material& gltfMaterial,
			const std::filesystem::path& texturesDirectory,
			const std::filesystem::path& directory);

		static std::shared_ptr<Entity> GenerateEntity(
			const fastgltf::Asset& gltfAsset,
			const fastgltf::Node& gltfNode,
			const std::shared_ptr<Scene>& scene,
			const std::vector<std::vector<std::shared_ptr<Mesh>>>& meshesByIndex,
			const std::vector<std::shared_ptr<Skeleton>>& skeletonsByIndex,
			const std::unordered_map<std::shared_ptr<Mesh>, std::shared_ptr<Material>>& materialsByMeshes,
			const std::vector<std::shared_ptr<SkeletalAnimation>>& animations);

		static void SerializeEntity(YAML::Emitter& out, const std::shared_ptr<Entity>& entity, bool rootEntity = false, bool isSerializingPrefab = false);

		static std::shared_ptr<Entity> DeserializeEntity(const YAML::Node& in, const std::shared_ptr<Scene>& scene);

		static void SerializePrefab(const std::filesystem::path& filepath, const std::shared_ptr<Entity>& entity);

		static std::shared_ptr<Entity> DeserializePrefab(const std::filesystem::path& filepath, const std::shared_ptr<Scene>& scene);

		static void UpdatePrefab(const std::filesystem::path& filepath, const std::vector<std::shared_ptr<Entity>> entities);

		static void SerializeTransform(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeTransform(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeRenderer3D(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeRenderer3D(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializePointLight(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializePointLight(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeSpotLight(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeSpotLight(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeDirectionalLight(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeDirectionalLight(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeSkeletalAnimator(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeSkeletalAnimator(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeEntityAnimator(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeEntityAnimator(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeCamera(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeCamera(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeCanvas(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeCanvas(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeRigidBody(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeRigidBody(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeDecal(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeDecal(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeUserComponents(YAML::Emitter& out, const std::shared_ptr<Entity>& entity);

		static void DeserializeUserComponents(const YAML::Node& in, const std::shared_ptr<Entity>& entity);

		static void SerializeScene(const std::filesystem::path& filepath, const std::shared_ptr<Scene>& scene);

		static std::shared_ptr<Scene> DeserializeScene(const std::filesystem::path& filepath);

		static void SerializeGraphicsSettings(const GraphicsSettings& graphicsSettings);

		static GraphicsSettings DeserializeGraphicsSettings(const std::filesystem::path& filepath);

		static void SerializeTextureMeta(const Texture::Meta& meta);

		static std::optional<Texture::Meta> DeserializeTextureMeta(const std::filesystem::path& filepath);

		static std::filesystem::path DeserializeFilepath(const std::string& uuidOrFilepath);

		static void SerializeThumbnailMeta(const std::filesystem::path& filepath, const size_t lastWriteTime);

		static size_t DeserializeThumbnailMeta(const std::filesystem::path& filepath);

		static void SerializeAnimationTrack(const std::filesystem::path& filepath, const EntityAnimator::AnimationTrack& animationTrack);

		static std::optional<EntityAnimator::AnimationTrack> DeserializeAnimationTrack(const std::filesystem::path& filepath);

	private:
		static void ParseUniformValues(
			const YAML::detail::iterator_value& data,
			Pipeline::UniformInfo& uniformsInfo);
	};

}
