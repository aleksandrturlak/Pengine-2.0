#pragma once

#include "../Core/Core.h"

#include "Buffer.h"
#include "UniformWriter.h"

namespace Pengine
{

	class RenderPass;
	class Window;
	class Camera;
	class Renderer;
	class RenderView;
	class Scene;
	class Entity;

	class PENGINE_API Pass
	{
		public:

			enum class Type
			{
				GRAPHICS,
				COMPUTE
			};

			struct RenderCallbackInfo
			{
				std::shared_ptr<RenderPass> renderPass;
				std::shared_ptr<Window> window;
				std::shared_ptr<RenderView> renderView;
				std::shared_ptr<Renderer> renderer;
				std::shared_ptr<Scene> scene;
				std::shared_ptr<Entity> camera;
				glm::mat4 projection;
				glm::ivec2 viewportSize;
				void* frame;
			};

			explicit Pass(
				Type type,
				const std::string& name,
				const std::function<void(RenderCallbackInfo)>& executeCallback,
				const std::function<void(Pass*)>& createCallback);
			virtual ~Pass() = default;
			Pass(const Pass&) = delete;
			Pass(Pass&&) = delete;
			Pass& operator=(const Pass&) = delete;
			Pass& operator=(Pass&&) = delete;

			virtual void Execute(const RenderCallbackInfo& renderInfo) const {};

			[[nodiscard]] Type GetType() const { return m_Type; }

			[[nodiscard]] const std::string& GetName() const { return m_Name; }

			[[nodiscard]] std::shared_ptr<UniformWriter> GetUniformWriter() const { return m_UniformWriter; }

			[[nodiscard]] std::shared_ptr<Buffer> GetBuffer(const std::string& name) const;

			void SetBuffer(const std::string& name, const std::shared_ptr<Buffer>& buffer);

			void SetUniformWriter(std::shared_ptr<UniformWriter> uniformWriter);

			size_t GetId() const;

		  protected:
			Type m_Type;
			std::string m_Name = none;
			bool m_IsInitialized = false;

			std::function<void(RenderCallbackInfo)> m_ExecuteCallback;
			std::function<void(Pass*)> m_CreateCallback;

			std::shared_ptr<UniformWriter> m_UniformWriter;
			std::unordered_map<std::string, std::shared_ptr<Buffer>> m_BuffersByName;

			size_t m_Id;

			friend class Renderer;
	};

}
