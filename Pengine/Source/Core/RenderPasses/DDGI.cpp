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
#include "../DDGIRenderer.h"

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

static std::shared_ptr<Texture> EnsureTexture(
    const std::shared_ptr<RenderView>& renderView,
    const std::string& name,
    const glm::ivec2& size,
    Format format,
    uint32_t instanceSize)
{
    auto existing = renderView->GetStorageImage(name);
    if (existing && existing->GetSize() == size)
        return existing;

    Texture::CreateInfo createInfo{};
    createInfo.aspectMask    = Texture::AspectMask::COLOR;
    createInfo.instanceSize  = instanceSize;
    createInfo.filepath      = name;
    createInfo.name          = name;
    createInfo.format        = format;
    createInfo.size          = size;
    createInfo.usage         = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
    createInfo.isMultiBuffered = false;
    const auto texture = Texture::Create(createInfo);
    renderView->SetStorageImage(name, texture);
    return texture;
}

void RenderPassManager::CreateDDGIProbeOffset()
{
    ComputePass::CreateInfo createInfo{};
    createInfo.type = Pass::Type::COMPUTE;
    createInfo.name = DDGIProbeOffset;

    createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
    {
        PROFILER_SCOPE(DDGIProbeOffset);

        const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
            std::filesystem::path("Materials") / "DDGIProbeOffset.basemat");
        const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
        if (!pipeline)
            return;

        const GraphicsSettings::DDGI& ddgiSettings = renderInfo.scene->GetGraphicsSettings().ddgi;

        DDGIRenderer* ddgi = (DDGIRenderer*)renderInfo.renderView->GetCustomData("DDGIRenderer");
        if (!ddgi)
        {
            ddgi = new DDGIRenderer();
            renderInfo.renderView->SetCustomData("DDGIRenderer", ddgi);
        }

        ddgi->m_IsEnabled      = ddgiSettings.isEnabled;
        ddgi->m_GridDimensions = glm::uvec3(ddgiSettings.gridX, ddgiSettings.gridY, ddgiSettings.gridZ);
        ddgi->m_ProbeSpacing   = ddgiSettings.probeSpacing;
        ddgi->m_RaysPerProbe   = static_cast<uint32_t>(ddgiSettings.raysPerProbe);
        ddgi->m_FollowCamera   = ddgiSettings.followCamera;
        ddgi->m_FixedOrigin    = ddgiSettings.fixedOrigin;

        if (!ddgi->m_IsEnabled)
            return;

        if (!renderInfo.scene->GetTLAS())
            return;

        // Detect grid movement and reset hysteresis so the blend pass does a
        // direct copy instead of blending with stale atlas data.
        {
            const glm::vec3 newOrigin = ddgi->ComputeGridOrigin(
                renderInfo.camera
                    ? renderInfo.camera->GetComponent<Transform>().GetPosition()
                    : glm::vec3(0.0f));
            if (newOrigin != ddgi->m_LastGridOrigin)
            {
                ddgi->m_LastGridOrigin = newOrigin;
                ddgi->m_FrameIndex     = 0;
            }
        }

        const glm::ivec2 offsetSize = ddgi->GetProbeOffsetAtlasSize();
        ddgi->m_ProbeOffsetAtlas = EnsureTexture(
            renderInfo.renderView, "DDGIProbeOffsets", offsetSize,
            Format::R16G16B16A16_SFLOAT, sizeof(uint16_t) * 4);

        const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
            renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

        const glm::vec3 cameraPosition = renderInfo.camera
            ? renderInfo.camera->GetComponent<Transform>().GetPosition()
            : glm::vec3(0.0f);

        WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

        renderUniformWriter->WriteTextureToFrame("probeOffsets", ddgi->m_ProbeOffsetAtlas);
        renderUniformWriter->WriteAccelerationStructureToFrame("topLevelAS", renderInfo.scene->GetTLAS());

        const DDGIRenderer::DDGIData shaderData = ddgi->BuildShaderData(cameraPosition);

        const std::shared_ptr<Buffer> ddgiBuffer = GetOrCreateBuffer(
            renderInfo.renderView,
            renderUniformWriter,
            "DDGIOffsetUniform",
            {},
            { Buffer::Usage::UNIFORM_BUFFER },
            MemoryType::CPU,
            true);

        baseMaterial->WriteToBuffer(ddgiBuffer, "DDGIOffsetUniform", "ddgi", shaderData);

        std::vector<NativeHandle> uniformWriterNativeHandles;
        std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
        GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);

        if (FlushUniformWriters(uniformWriters))
        {
            renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

            const uint32_t totalProbes = ddgi->m_GridDimensions.x
                                       * ddgi->m_GridDimensions.y
                                       * ddgi->m_GridDimensions.z;
            renderInfo.renderer->Compute(
                pipeline,
                { (totalProbes + 63u) / 64u, 1u, 1u },
                uniformWriterNativeHandles,
                renderInfo.frame);

            renderInfo.renderer->PipelineBarrier(
                BarrierBatch{}
                    .Stages(PipelineStage::ComputeShader, PipelineStage::ComputeShader)
                    .Image(ddgi->m_ProbeOffsetAtlas, ImageLayout::General, ImageLayout::General,
                           Access::ShaderWrite, Access::ShaderRead),
                renderInfo.frame);

            renderInfo.renderer->EndCommandLabel(renderInfo.frame);
        }
    };

    CreateComputePass(createInfo);
}

void RenderPassManager::CreateDDGIProbeUpdate()
{
    ComputePass::CreateInfo createInfo{};
    createInfo.type = Pass::Type::COMPUTE;
    createInfo.name = DDGIProbeUpdate;

    createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
    {
        PROFILER_SCOPE(DDGIProbeUpdate);

        const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
            std::filesystem::path("Materials") / "DDGIProbeUpdate.basemat");
        const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
        if (!pipeline)
            return;

        const GraphicsSettings::DDGI& ddgiSettings = renderInfo.scene->GetGraphicsSettings().ddgi;

        DDGIRenderer* ddgi = (DDGIRenderer*)renderInfo.renderView->GetCustomData("DDGIRenderer");
        if (!ddgi)
        {
            ddgi = new DDGIRenderer();
            renderInfo.renderView->SetCustomData("DDGIRenderer", ddgi);
        }

        ddgi->m_IsEnabled      = ddgiSettings.isEnabled;
        ddgi->m_GridDimensions = glm::uvec3(ddgiSettings.gridX, ddgiSettings.gridY, ddgiSettings.gridZ);
        ddgi->m_ProbeSpacing   = ddgiSettings.probeSpacing;
        ddgi->m_RaysPerProbe   = static_cast<uint32_t>(ddgiSettings.raysPerProbe);
        ddgi->m_FollowCamera   = ddgiSettings.followCamera;
        ddgi->m_FixedOrigin    = ddgiSettings.fixedOrigin;

        if (!ddgi->m_IsEnabled)
            return;

        const glm::ivec2 irrSize = ddgi->GetIrradianceAtlasSize();
        const glm::ivec2 visSize = ddgi->GetVisibilityAtlasSize();

        auto irrAtlas = EnsureTexture(renderInfo.renderView, "DDGIIrradiance",      irrSize, Format::R16G16B16A16_SFLOAT, sizeof(uint16_t) * 4);
        auto visAtlas = EnsureTexture(renderInfo.renderView, "DDGIVisibility",      visSize, Format::R16G16_SFLOAT,       sizeof(uint16_t) * 2);
        auto irrTemp  = EnsureTexture(renderInfo.renderView, "DDGITempIrradiance",  irrSize, Format::R16G16B16A16_SFLOAT, sizeof(uint16_t) * 4);
        auto visTemp  = EnsureTexture(renderInfo.renderView, "DDGITempVisibility",  visSize, Format::R16G16_SFLOAT,       sizeof(uint16_t) * 2);

        if (irrAtlas != ddgi->m_IrradianceAtlas || visAtlas != ddgi->m_VisibilityAtlas)
            ddgi->m_FrameIndex = 0;

        ddgi->m_IrradianceAtlas     = irrAtlas;
        ddgi->m_VisibilityAtlas     = visAtlas;
        ddgi->m_TempIrradianceAtlas = irrTemp;
        ddgi->m_TempVisibilityAtlas = visTemp;

        const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
            renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

        const glm::vec3 cameraPosition = renderInfo.camera
            ? renderInfo.camera->GetComponent<Transform>().GetPosition()
            : glm::vec3(0.0f);

        WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

        renderUniformWriter->WriteTextureToFrame("outIrradiance", irrTemp);
        renderUniformWriter->WriteTextureToFrame("outVisibility", visTemp);

        if (const auto frameBuffer = renderInfo.scene->GetRenderView()->GetFrameBuffer(Atmosphere))
        {
            renderUniformWriter->WriteTextureToFrame("skyboxTexture", frameBuffer->GetAttachment(0));
        }

        if (renderInfo.scene->GetTLAS())
        {
            renderUniformWriter->WriteAccelerationStructureToFrame("topLevelAS", renderInfo.scene->GetTLAS());
        }

        if (ddgi->m_ProbeOffsetAtlas)
        {
            renderUniformWriter->WriteTextureToFrame("probeOffsets", ddgi->m_ProbeOffsetAtlas);
        }

        const DDGIRenderer::DDGIData shaderData = ddgi->BuildShaderData(cameraPosition);

        const std::shared_ptr<Buffer> ddgiBuffer = GetOrCreateBuffer(
            renderInfo.renderView,
            renderUniformWriter,
            "DDGIUniform",
            {},
            { Buffer::Usage::UNIFORM_BUFFER },
            MemoryType::CPU,
            true);

        baseMaterial->WriteToBuffer(ddgiBuffer, "DDGIUniform", "ddgi", shaderData);

        std::vector<NativeHandle> uniformWriterNativeHandles;
        std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
        GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);

        if (FlushUniformWriters(uniformWriters))
        {
            renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

            const uint32_t totalProbes = ddgi->m_GridDimensions.x
                                       * ddgi->m_GridDimensions.y
                                       * ddgi->m_GridDimensions.z;
            renderInfo.renderer->Compute(
                pipeline,
                { totalProbes, 1u, 1u },
                uniformWriterNativeHandles,
                renderInfo.frame);

            renderInfo.renderer->PipelineBarrier(
                BarrierBatch{}
                    .Stages(PipelineStage::ComputeShader, PipelineStage::ComputeShader)
                    .Image(irrTemp, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead)
                    .Image(visTemp, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead),
                renderInfo.frame);

            renderInfo.renderer->EndCommandLabel(renderInfo.frame);
        }

    };

    CreateComputePass(createInfo);
}

void RenderPassManager::CreateDDGIProbeBlend()
{
    ComputePass::CreateInfo createInfo{};
    createInfo.type = Pass::Type::COMPUTE;
    createInfo.name = DDGIProbeBlend;

    createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
    {
        PROFILER_SCOPE(DDGIProbeBlend);

        const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
            std::filesystem::path("Materials") / "DDGIProbeBlend.basemat");
        const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
        if (!pipeline)
            return;

        DDGIRenderer* ddgi = (DDGIRenderer*)renderInfo.renderView->GetCustomData("DDGIRenderer");
        if (!ddgi || !ddgi->m_IsEnabled)
            return;

        if (!ddgi->m_IrradianceAtlas || !ddgi->m_TempIrradianceAtlas ||
            !ddgi->m_VisibilityAtlas  || !ddgi->m_TempVisibilityAtlas)
            return;

        const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
            renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

        renderUniformWriter->WriteTextureToFrame("persistentIrradiance",  ddgi->m_IrradianceAtlas);
        renderUniformWriter->WriteTextureToFrame("tempIrradiance",        ddgi->m_TempIrradianceAtlas);
        renderUniformWriter->WriteTextureToFrame("persistentVisibility",  ddgi->m_VisibilityAtlas);
        renderUniformWriter->WriteTextureToFrame("tempVisibility",        ddgi->m_TempVisibilityAtlas);

        const glm::vec3 cameraPosition = renderInfo.camera
            ? renderInfo.camera->GetComponent<Transform>().GetPosition()
            : glm::vec3(0.0f);
        const DDGIRenderer::DDGIData shaderData = ddgi->BuildShaderData(cameraPosition);

        const std::shared_ptr<Buffer> ddgiBuffer = GetOrCreateBuffer(
            renderInfo.renderView,
            renderUniformWriter,
            "DDGIBlendUniform",
            {},
            { Buffer::Usage::UNIFORM_BUFFER },
            MemoryType::CPU,
            true);

        baseMaterial->WriteToBuffer(ddgiBuffer, "DDGIBlendUniform", "ddgi", shaderData);

        std::vector<NativeHandle> uniformWriterNativeHandles;
        std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
        GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);

        if (FlushUniformWriters(uniformWriters))
        {
            renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

            const glm::ivec2 irrSize = ddgi->GetIrradianceAtlasSize();
            const glm::ivec2 visSize = ddgi->GetVisibilityAtlasSize();
            const glm::ivec2 maxSize = glm::max(irrSize, visSize);
            const glm::uvec2 groupCount = glm::uvec2(maxSize) / glm::uvec2(8u) + glm::uvec2(1u);

            // z=0: irradiance, z=1: visibility — both handled in one dispatch
            renderInfo.renderer->Compute(
                pipeline,
                { groupCount.x, groupCount.y, 2u },
                uniformWriterNativeHandles,
                renderInfo.frame);

            renderInfo.renderer->PipelineBarrier(
                BarrierBatch{}
                    .Stages(PipelineStage::ComputeShader, PipelineStage::ComputeShader)
                    .Image(ddgi->m_IrradianceAtlas, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead)
                    .Image(ddgi->m_VisibilityAtlas,  ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead),
                renderInfo.frame);

            renderInfo.renderer->EndCommandLabel(renderInfo.frame);
        }

        // Advance after blend so both ProbeUpdate and ProbeBlend see the same
        // frameIndex this frame. frameIndex == 0 on first frame causes blend to
        // skip hysteresis and copy directly, avoiding garbage from uninitialized atlas.
        ddgi->m_FrameIndex++;
    };

    CreateComputePass(createInfo);
}

void RenderPassManager::CreateDDGIProbeDebug()
{
    ComputePass::CreateInfo createInfo{};
    createInfo.type = Pass::Type::COMPUTE;
    createInfo.name = DDGIProbeDebug;

    createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
    {
        PROFILER_SCOPE(DDGIProbeDebug);

        const GraphicsSettings::DDGI& ddgiSettings = renderInfo.scene->GetGraphicsSettings().ddgi;
        if (!ddgiSettings.isEnabled || !ddgiSettings.visualizeProbes)
            return;

        DDGIRenderer* ddgi = (DDGIRenderer*)renderInfo.renderView->GetCustomData("DDGIRenderer");
        if (!ddgi || !ddgi->m_IsEnabled || !ddgi->m_IrradianceAtlas || !ddgi->m_ProbeOffsetAtlas)
            return;

        const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
            std::filesystem::path("Materials") / "DDGIProbeDebug.basemat");
        const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
        if (!pipeline)
            return;

        const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
            renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

        const glm::vec3 cameraPosition = renderInfo.camera
            ? renderInfo.camera->GetComponent<Transform>().GetPosition()
            : glm::vec3(0.0f);

        WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

        const std::shared_ptr<Texture> colorTexture = renderInfo.renderView->GetFrameBuffer(Transparent)->GetAttachment(0);
        renderUniformWriter->WriteTextureToFrame("outColor", colorTexture);

        const DDGIRenderer::DDGIData shaderData = ddgi->BuildShaderData(cameraPosition);

        const std::shared_ptr<Buffer> ddgiBuffer = GetOrCreateBuffer(
            renderInfo.renderView,
            renderUniformWriter,
            "DDGIDebugUniform",
            {},
            { Buffer::Usage::UNIFORM_BUFFER },
            MemoryType::CPU,
            true);

        baseMaterial->WriteToBuffer(ddgiBuffer, "DDGIDebugUniform", "ddgi", shaderData);

        std::vector<NativeHandle> uniformWriterNativeHandles;
        std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
        GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);

        if (FlushUniformWriters(uniformWriters))
        {
            renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

            const glm::uvec2 viewportSize = glm::uvec2(renderInfo.viewportSize);
            renderInfo.renderer->Compute(
                pipeline,
                { (viewportSize.x + 7u) / 8u, (viewportSize.y + 7u) / 8u, 1u },
                uniformWriterNativeHandles,
                renderInfo.frame);

            renderInfo.renderer->EndCommandLabel(renderInfo.frame);
        }
    };

    CreateComputePass(createInfo);
}
