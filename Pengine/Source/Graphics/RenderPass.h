#pragma once

#include "../Core/Core.h"

#include "Pass.h"
#include "Texture.h"
#include "Format.h"

namespace Pengine
{
	const std::string ZPrePass = "ZPrePass";
	const std::string ComputeIndirectDrawGBuffer = "ComputeIndirectDrawGBuffer";
	const std::string GBuffer = "GBuffer";
	const std::string Decals = "Decals";
	const std::string Deferred = "Deferred";
	const std::string DefaultReflection = "DefaultReflection";
	const std::string Atmosphere = "Atmosphere";
	const std::string Transparent = "Transparent";
	const std::string SSAO = "SSAO";
	const std::string SSAOBlur = "SSAOBlur";
	const std::string SSS = "SSS";
	const std::string SSSBlur = "SSSBlur";
	const std::string CSM = "CSM";
	const std::string PointLightShadows = "PointLightShadows";
	const std::string SpotLightShadows = "SpotLightShadows";
	const std::string Bloom = "Bloom";
	const std::string ToneMapping = "ToneMapping";
	const std::string AntiAliasingAndCompose = "AntiAliasingAndCompose";
	const std::string SSR = "SSR";
	const std::string SSRBlur = "SSRBlur";
	const std::string UI = "UI";

	class FrameBuffer;
	class Window;
	class Camera;
	class Renderer;
	class RenderView;
	class Scene;
	class Entity;

	class PENGINE_API RenderPass : public Pass
	{
	public:
		struct ClearDepth
		{
			float clearDepth;
			uint32_t clearStencil;
		};

		enum class Load
		{
			LOAD = 0,
			CLEAR = 1,
			DONT_CARE = 2,
		};

		enum class Store
		{
			STORE = 0,
			DONT_CARE = 1,
			NONE = 1000301000,
		};

		struct AttachmentDescription
		{
			Texture::CreateInfo textureCreateInfo;
			Texture::Layout layout;
			Load load = Load::CLEAR;
			Store store = Store::STORE;
			std::function<std::shared_ptr<Texture>(RenderView*)> getFrameBufferCallback;
		};

		struct Scissors
		{
			glm::ivec2 offset;
			glm::uvec2 size;
		};

		struct Viewport
		{
			glm::vec2 position;
			glm::vec2 size;
			glm::vec2 minMaxDepth;
		};

		struct SubmitInfo
		{
			std::shared_ptr<RenderPass> renderPass;
			std::shared_ptr<FrameBuffer> frameBuffer;
			std::optional<Scissors> scissors;
			std::optional<Viewport> viewport;
			void* frame;
		};

		struct CreateInfo
		{
			Type type;
			std::string name;
			std::vector<glm::vec4> clearColors;
			std::vector<ClearDepth> clearDepths;
			std::vector<AttachmentDescription> attachmentDescriptions;
			std::function<void(const RenderCallbackInfo&)> executeCallback;
			std::function<void(Pass*)> createCallback;
			bool resizeWithViewport = false;
			bool createFrameBuffer = true;
			glm::vec2 resizeViewportScale = { 1.0f, 1.0f };
		};

		static std::shared_ptr<RenderPass> Create(const CreateInfo& createInfo);

		explicit RenderPass(const CreateInfo& createInfo);
		virtual ~RenderPass() override = default;
		RenderPass(const RenderPass&) = delete;
		RenderPass(RenderPass&&) = delete;
		RenderPass& operator=(const RenderPass&) = delete;
		RenderPass& operator=(RenderPass&&) = delete;

		[[nodiscard]] std::vector<glm::vec4> GetClearColors() const { return m_ClearColors; }

		[[nodiscard]] std::vector<ClearDepth> GetClearDepth() const { return m_ClearDepths; }

		virtual void Execute(const RenderCallbackInfo& renderInfo) const override;

		[[nodiscard]] std::vector<AttachmentDescription>& GetAttachmentDescriptions() { return m_AttachmentDescriptions; }

		[[nodiscard]] const bool GetResizeWithViewport() const { return m_ResizeWithViewport; }

		[[nodiscard]] const glm::vec2& GetResizeViewportScale() const { return m_ResizeViewportScale; }

		void SetResizeViewportScale(const glm::vec2& resizeViewportScale) { m_ResizeViewportScale = resizeViewportScale; }

	private:
		std::vector<AttachmentDescription> m_AttachmentDescriptions; 
		std::vector<glm::vec4> m_ClearColors;
		std::vector<ClearDepth> m_ClearDepths;
		bool m_ResizeWithViewport = false;
		bool m_CreateFrameBuffer = true;
		glm::vec2 m_ResizeViewportScale = { 1.0f, 1.0f };

		friend class RenderView;
	};

}
