#include "../RenderPassManager.h"

#include "../Logger.h"
#include "../BindlessUniformWriter.h"
#include "../MaterialManager.h"
#include "../MeshManager.h"
#include "../SceneManager.h"
#include "../TextureManager.h"
#include "../Time.h"
#include "../FrustumCulling.h"
#include "../Raycast.h"
#include "../UIRenderer.h"
#include "../Profiler.h"
#include "../Timer.h"

#include "../../Components/Canvas.h"
#include "../../Components/Camera.h"
#include "../../Components/Decal.h"
#include "../../Components/DirectionalLight.h"
#include "../../Components/PointLight.h"
#include "../../Components/SpotLight.h"
#include "../../Components/Renderer3D.h"
#include "../../Components/SkeletalAnimator.h"
#include "../../Components/Transform.h"
#include "../../Graphics/Device.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/RenderView.h"
#include "../../Graphics/GraphicsPipeline.h"
#include "../../EventSystem/EventSystem.h"
#include "../../EventSystem/NextFrameEvent.h"

#include "../ViewportManager.h"
#include "../Viewport.h"

using namespace Pengine;

void RenderPassManager::CreateSSS()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = SSS;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SSS);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "SSS.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::Shadows::SSS& sssSettings = renderInfo.scene->GetGraphicsSettings().shadows.sss;
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[sssSettings.resolutionScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t);
		createInfo.filepath = passName;
		createInfo.name = passName;
		createInfo.format = Format::R8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		createInfo.isMultiBuffered = false;

		std::shared_ptr<Texture> sssTexture = renderInfo.renderView->GetStorageImage(passName);

		if (!sssSettings.isEnabled)
		{
			renderInfo.renderView->DeleteUniformWriter(passName);
			renderInfo.renderView->DeleteStorageImage(passName);

			return;
		}
		else
		{
			if (!renderInfo.renderView->GetStorageImage(passName))
			{
				sssTexture = Texture::Create(createInfo);
				renderInfo.renderView->SetStorageImage(passName, sssTexture);
				GetOrCreateUniformWriter(
					renderInfo.renderView,
					pipeline,
					Pipeline::DescriptorSetIndexType::RENDERER,
					passName)->WriteTextureToAllFrames("outColor", sssTexture);
			}
		}

		if (currentViewportSize != sssTexture->GetSize())
		{
			const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
				renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
			const std::shared_ptr<Texture> sssTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage(passName, sssTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", sssTexture);
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

			glm::uvec2 groupCount = currentViewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateSSSBlur()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = SSSBlur;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SSSBlur);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "SSSBlur.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::Shadows::SSS& sssSettings = renderInfo.scene->GetGraphicsSettings().shadows.sss;
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[sssSettings.resolutionBlurScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t);
		createInfo.filepath = passName;
		createInfo.name = passName;
		createInfo.format = Format::R8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		createInfo.isMultiBuffered = true;

		std::shared_ptr<Texture> sssBlurTexture = renderInfo.renderView->GetStorageImage(passName);
		if (!sssSettings.isEnabled)
		{
			renderInfo.renderView->DeleteUniformWriter(passName);
			renderInfo.renderView->DeleteStorageImage(passName);

			return;
		}
		else
		{
			if (!renderInfo.renderView->GetStorageImage(passName))
			{
				sssBlurTexture = Texture::Create(createInfo);
				renderInfo.renderView->SetStorageImage(passName, sssBlurTexture);
				GetOrCreateUniformWriter(
					renderInfo.renderView,
					pipeline,
					Pipeline::DescriptorSetIndexType::RENDERER,
					passName)->WriteTextureToAllFrames("outColor", sssBlurTexture);
			}
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
		if (currentViewportSize != sssBlurTexture->GetSize())
		{
			const std::shared_ptr<Texture> ssaoBlurTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage(passName, ssaoBlurTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", ssaoBlurTexture);
		}

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

			glm::uvec2 groupCount = currentViewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);

		}

		renderInfo.renderer->EndCommandLabel(renderInfo.frame);
	};

	CreateComputePass(createInfo);
}

