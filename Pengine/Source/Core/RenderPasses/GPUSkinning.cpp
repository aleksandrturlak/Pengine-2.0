#include "../RenderPassManager.h"

#include "../Logger.h"
#include "../MaterialManager.h"
#include "../Profiler.h"
#include "../Scene.h"

#include "../../Components/Renderer3D.h"
#include "../../Components/SkeletalAnimator.h"
#include "../../Graphics/AccelerationStructure.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/BarrierBatch.h"
#include "../../Graphics/Mesh.h"

using namespace Pengine;

void RenderPassManager::CreateGPUSkinningPass()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = GPUSkinning;

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(GPUSkinning);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "GPUSkinning.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(GPUSkinning);
		if (!pipeline)
			return;

		struct SkinningPushConstants
		{
			uint64_t positionBuffer;
			uint64_t normalBuffer;
			uint64_t skinnedDataBuffer;
			uint64_t boneMatricesBuffer;
			uint64_t outputBuffer;
			uint32_t vertexCount;
			uint32_t padding;
		};

		struct DispatchJob
		{
			SkinningPushConstants pc;
			NativeHandle outputBuffer;
			Renderer3D* r3d;
		};

		std::vector<DispatchJob> jobs;
		jobs.reserve(64);

		const auto view = renderInfo.scene->GetRegistry().view<Renderer3D>();
		view.each([&](entt::entity entity, Renderer3D& r3d)
		{
			if (!r3d.mesh || r3d.mesh->GetType() != Mesh::Type::SKINNED)
				return;
			if (!r3d.skinnedVertexBuffer)
				return;
			if (!r3d.skeletalAnimatorEntityUUID.IsValid())
				return;

			const auto skeletalAnimatorEntity = renderInfo.scene->FindEntityByUUID(r3d.skeletalAnimatorEntityUUID);
			if (!skeletalAnimatorEntity)
				return;

			const auto* skeletalAnimator = renderInfo.scene->GetRegistry().try_get<SkeletalAnimator>(
				skeletalAnimatorEntity->GetHandle());
			if (!skeletalAnimator)
				return;

			const auto& layouts = r3d.mesh->GetVertexLayouts();
			uint64_t positionAddress = 0;
			uint64_t normalAddress = 0;
			uint64_t skinnedDataAddress = 0;
			for (size_t i = 0; i < layouts.size(); ++i)
			{
				const std::string& tag = layouts[i].tag;
				if (tag == "Position")
					positionAddress = r3d.mesh->GetVertexBuffer(i)->GetDeviceAddress().Get();
				else if (tag == "Normal")
					normalAddress = r3d.mesh->GetVertexBuffer(i)->GetDeviceAddress().Get();
				else if (tag == "Bones")
					skinnedDataAddress = r3d.mesh->GetVertexBuffer(i)->GetDeviceAddress().Get();
			}

			if (!positionAddress || !normalAddress || !skinnedDataAddress)
				return;

			DispatchJob& job = jobs.emplace_back();
			job.pc.positionBuffer     = positionAddress;
			job.pc.normalBuffer       = normalAddress;
			job.pc.skinnedDataBuffer  = skinnedDataAddress;
			job.pc.boneMatricesBuffer = skeletalAnimator->GetBuffer()->GetDeviceAddress().Get();
			job.pc.outputBuffer       = r3d.skinnedVertexBuffer->GetDeviceAddress().Get();
			job.pc.vertexCount        = static_cast<uint32_t>(r3d.mesh->GetVertexCount());
			job.pc.padding            = 0;
			job.outputBuffer          = r3d.skinnedVertexBuffer->GetNativeHandle();
			job.r3d                   = &r3d;
		});

		if (jobs.empty())
			return;

		renderInfo.renderer->BeginCommandLabel(GPUSkinning, topLevelRenderPassDebugColor, renderInfo.frame);
		renderInfo.renderer->BindPipeline(pipeline, renderInfo.frame);

		for (const DispatchJob& job : jobs)
		{
			renderInfo.renderer->PushConstants(
				pipeline,
				ShaderStage::Compute,
				0,
				sizeof(SkinningPushConstants),
				&job.pc,
				renderInfo.frame);

			const uint32_t groups = (job.pc.vertexCount + 63) / 64;
			renderInfo.renderer->Dispatch({ groups, 1, 1 }, renderInfo.frame);
		}

		BarrierBatch barrier{};
		barrier.Stages(
			PipelineStage::ComputeShader,
			PipelineStage::VertexShader | PipelineStage::AccelerationStructureBuild);
		for (const DispatchJob& job : jobs)
		{
			barrier.Buffer(job.outputBuffer, Access::ShaderWrite, Access::ShaderRead);
		}
		renderInfo.renderer->PipelineBarrier(barrier, renderInfo.frame);

		for (const DispatchJob& job : jobs)
		{
			Renderer3D& r3d = *job.r3d;
			const uint64_t skinnedVertexAddr = r3d.skinnedVertexBuffer->GetDeviceAddress().Get();
			if (!r3d.skinnedBLAS)
			{
				r3d.skinnedBLAS = AccelerationStructure::CreateSkinnedBLAS(
					*r3d.mesh, skinnedVertexAddr, renderInfo.frame);
			}
			else
			{
				r3d.skinnedBLAS->Rebuild(*r3d.mesh, skinnedVertexAddr, renderInfo.frame);
			}
		}

		renderInfo.renderer->EndCommandLabel(renderInfo.frame);
	};

	CreateComputePass(createInfo);
}
