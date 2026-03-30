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

void RenderPassManager::CreateSSR()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = SSR;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SSR);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "SSR.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::RayTracing::Reflections rtSettings = renderInfo.scene->GetGraphicsSettings().rayTracing.reflections;
		const GraphicsSettings::SSR& ssrSettings = renderInfo.scene->GetGraphicsSettings().ssr;
		const std::string ssrBufferName = passName;
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[ssrSettings.resolutionScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t) * 4;
		createInfo.filepath = "Reflections";
		createInfo.name = "Reflections";
		createInfo.format = Format::R8G8B8A8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		createInfo.isMultiBuffered = false;

		if (!ssrSettings.isEnabled && !rtSettings.isRayTraced)
		{
			renderInfo.renderView->DeleteUniformWriter(passName);
			renderInfo.renderView->DeleteStorageImage("Reflections");
			renderInfo.renderView->DeleteBuffer(ssrBufferName);

			return;
		}

		if (!ssrSettings.isEnabled || rtSettings.isRayTraced)
		{
			return;
		}

		std::shared_ptr<Texture> reflectionsTexture = renderInfo.renderView->GetStorageImage("Reflections");
		if (!reflectionsTexture)
		{
			reflectionsTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage("Reflections", reflectionsTexture);
		}

		GetOrCreateUniformWriter(
			renderInfo.renderView,
			pipeline,
			Pipeline::DescriptorSetIndexType::RENDERER,
			passName)->WriteTextureToFrame("outColor", reflectionsTexture);

		if (currentViewportSize != reflectionsTexture->GetSize())
		{
			const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
				renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
			const std::shared_ptr<Texture> reflectionsTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage("Reflections", reflectionsTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", reflectionsTexture);
		}
		
		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
		
		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);
		if (const auto frameBuffer = renderInfo.scene->GetRenderView()->GetFrameBuffer(Atmosphere))
		{
			renderUniformWriter->WriteTextureToFrame("skyboxTexture", frameBuffer->GetAttachment(0));
		}

		const glm::vec2 viewportScale = glm::vec2(resolutionScales[ssrSettings.resolutionScale]);
		const int useSkyBoxFallback = ssrSettings.useSkyBoxFallback;
		const std::shared_ptr<Buffer> ssrBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			ssrBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "viewportScale", viewportScale);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "maxDistance", ssrSettings.maxDistance);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "resolution", ssrSettings.resolution);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "stepCount", ssrSettings.stepCount);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "thickness", ssrSettings.thickness);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "useSkyBoxFallback", useSkyBoxFallback);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

			renderInfo.renderer->BeginCommandLabel(passName, { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

			glm::uvec2 groupCount = currentViewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->PipelineBarrier(
				BarrierBatch{}
					.Stages(PipelineStage::ComputeShader, PipelineStage::ComputeShader)
					.Image(reflectionsTexture, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead),
					renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateBlurReflections()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = BlurReflections;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(BlurReflections);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "BlurReflections.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::RayTracing::Reflections rtSettings = renderInfo.scene->GetGraphicsSettings().rayTracing.reflections;
		const GraphicsSettings::SSR& ssrSettings = renderInfo.scene->GetGraphicsSettings().ssr;
		const std::string ssrBufferName = passName;
		constexpr float resolutionScales[] = { 0.125f, 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[ssrSettings.resolutionBlurScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t) * 4;
		createInfo.filepath = "BlurredReflections";
		createInfo.name = "BlurredReflections";
		createInfo.format = Format::R8G8B8A8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::TRANSFER_DST };
		createInfo.isMultiBuffered = false;

		std::shared_ptr<Texture> blurredReflectionsTexture = renderInfo.renderView->GetStorageImage("BlurredReflections");

		if (ssrSettings.isEnabled || rtSettings.isRayTraced)
		{
			if (!blurredReflectionsTexture)
			{
				blurredReflectionsTexture = Texture::Create(createInfo);
				renderInfo.renderView->SetStorageImage("BlurredReflections", blurredReflectionsTexture);
				GetOrCreateUniformWriter(
					renderInfo.renderView,
					pipeline,
					Pipeline::DescriptorSetIndexType::RENDERER,
					passName)->WriteTextureToAllFrames("outColor", blurredReflectionsTexture);
			}
		}
		else
		{
			renderInfo.renderView->DeleteUniformWriter(passName);
			renderInfo.renderView->DeleteStorageImage("BlurredReflections");
			renderInfo.renderView->DeleteBuffer(ssrBufferName);

			return;
		}

		if (currentViewportSize != blurredReflectionsTexture->GetSize())
		{
			const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
				renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
			const std::shared_ptr<Texture> blurredReflectionsTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage("BlurredReflections", blurredReflectionsTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", blurredReflectionsTexture);
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

		const int blur = (int)ssrSettings.blur;

		const std::shared_ptr<Buffer> ssrBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			ssrBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "blur", blur);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "blurRange", ssrSettings.blurRange);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "blurOffset", ssrSettings.blurOffset);

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

			renderInfo.renderer->PipelineBarrier(
				BarrierBatch{}
					.Stages(PipelineStage::ComputeShader, PipelineStage::FragmentShader)
					.Image(blurredReflectionsTexture, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead),
					renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateRayTracedReflections()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = RayTracedReflection;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(RayTracedReflection);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "RayTracedReflection.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::RayTracing::Reflections rtSettings = renderInfo.scene->GetGraphicsSettings().rayTracing.reflections;
		const GraphicsSettings::SSR& ssrSettings = renderInfo.scene->GetGraphicsSettings().ssr;
		const std::string reflectionsBufferName = passName;
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[ssrSettings.resolutionScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t) * 4;
		createInfo.filepath = "Reflections";
		createInfo.name = "Reflections";
		createInfo.format = Format::R8G8B8A8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		createInfo.isMultiBuffered = false;

		std::shared_ptr<Texture> reflectionsTexture = renderInfo.renderView->GetStorageImage("Reflections");

		if (!rtSettings.isRayTraced)
		{
			return;
		}

		if (!reflectionsTexture)
		{
			reflectionsTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage("Reflections", reflectionsTexture);
		}

		GetOrCreateUniformWriter(
			renderInfo.renderView,
			pipeline,
			Pipeline::DescriptorSetIndexType::RENDERER,
			passName)->WriteTextureToFrame("outColor", reflectionsTexture);

		if (currentViewportSize != reflectionsTexture->GetSize())
		{
			const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
				renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
			const std::shared_ptr<Texture> reflectionsTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage("Reflections", reflectionsTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", reflectionsTexture);
		}
		
		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
		
		if (renderInfo.scene->GetTLAS())
		{
			renderUniformWriter->WriteAccelerationStructureToFrame("topLevelAS", renderInfo.scene->GetTLAS());
		}

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);
		
		if (const auto frameBuffer = renderInfo.scene->GetRenderView()->GetFrameBuffer(Atmosphere))
		{
			renderUniformWriter->WriteTextureToFrame("skyboxTexture", frameBuffer->GetAttachment(0));
		}

		const std::shared_ptr<Buffer> ddgiBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			"DDGIUniform",
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		renderUniformWriter->WriteBufferToFrame("DDGIUniform", ddgiBuffer);

		const glm::vec2 viewportScale = glm::vec2(resolutionScales[ssrSettings.resolutionScale]);
		const std::shared_ptr<Buffer> reflectionsBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			reflectionsBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		baseMaterial->WriteToBuffer(reflectionsBuffer, reflectionsBufferName, "viewportScale", viewportScale);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

			renderInfo.renderer->BeginCommandLabel(passName, { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

			glm::uvec2 groupCount = currentViewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->PipelineBarrier(
				BarrierBatch{}
					.Stages(PipelineStage::ComputeShader, PipelineStage::ComputeShader)
					.Image(reflectionsTexture, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead),
					renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}
