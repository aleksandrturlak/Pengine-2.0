#pragma once

#include "../Core/Core.h"

#include "RenderPass.h"
#include "Texture.h"

namespace Pengine
{

	class PENGINE_API FrameBuffer
	{
	public:

		/**
		 * @param renderView only needs to be passed if render pass has an attachment that references another render pass attachment.
		 */
		static std::shared_ptr<FrameBuffer> Create(
			std::shared_ptr<RenderPass> renderPass,
			RenderView* renderView,
			const glm::ivec2& size);

		FrameBuffer(
			const std::vector<Texture::CreateInfo>& attachments,
			std::shared_ptr<RenderPass> renderPass,
			RenderView* renderView);
		virtual ~FrameBuffer() = default;
		FrameBuffer(const FrameBuffer&) = delete;
		FrameBuffer& operator=(const FrameBuffer&) = delete;

		[[nodiscard]] virtual std::shared_ptr<Texture> GetAttachment(const size_t index) const = 0;

		[[nodiscard]] virtual std::vector<std::shared_ptr<Texture>> GetAttachments() const = 0;

		[[nodiscard]] std::vector<Texture::CreateInfo>& GetAttachmentCreateInfos() { return m_AttachmentCreateInfos; }

		virtual void Resize(const glm::ivec2& size) = 0;

		virtual void Clear() = 0;

		[[nodiscard]] std::shared_ptr<RenderPass> GetRenderPass() const { return m_RenderPass; }

		[[nodiscard]] glm::ivec2 GetSize() const { return m_Size; }

	protected:
		std::vector<Texture::CreateInfo> m_AttachmentCreateInfos;
		std::shared_ptr<RenderPass> m_RenderPass;
		RenderView* m_RenderView;
		glm::ivec2 m_Size{};
	};

}
