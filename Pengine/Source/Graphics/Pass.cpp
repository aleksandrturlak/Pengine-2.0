#include "Pass.h"

#include "../Utils/Utils.h"

using namespace Pengine;

static size_t g_NextPassId = 0;

Pass::Pass(
	Type type,
	const std::string& name,
	const std::function<void(RenderCallbackInfo)>& executeCallback,
	const std::function<void(Pass*)>& createCallback)
	: m_Type(type)
	, m_Name(name)
	, m_ExecuteCallback(executeCallback)
	, m_CreateCallback(createCallback)
	, m_Id(g_NextPassId++)
{

}

std::shared_ptr<Buffer> Pass::GetBuffer(const std::string& name) const
{
	return Utils::Find(name, m_BuffersByName);
}

void Pass::SetBuffer(const std::string& name, const std::shared_ptr<Buffer>& buffer)
{
	m_BuffersByName[name] = buffer;
}

void Pass::SetUniformWriter(std::shared_ptr<UniformWriter> uniformWriter)
{
	m_UniformWriter = uniformWriter;
}

size_t Pass::GetId() const
{
	return m_Id;
}