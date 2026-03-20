#include "ClayManager.h"

#include "../Components/Canvas.h"

using namespace Pengine;

ClayManager& ClayManager::GetInstance()
{
	static ClayManager ClayManager;
	return ClayManager;
}

void ClayManager::Init(Canvas* canvas, Clay_Context* context)
{
	Clay_SetCurrentContext(context);
	Clay_SetMeasureTextFunction(canvas->measureText, nullptr);
	if (canvas->queryScrollOffset)
	{
		Clay_SetQueryScrollOffsetFunction(canvas->queryScrollOffset, nullptr);
	}
	Clay_SetLayoutDimensions({ (float)canvas->size.x, (float)canvas->size.y });
}

void ClayManager::BeginLayout()
{
	Clay_BeginLayout();
}

Clay_RenderCommandArray ClayManager::EndLayout()
{
	return Clay_EndLayout();
}

void ClayManager::OpenElement()
{
	Clay__OpenElement();
}

void ClayManager::ConfigureOpenElement(const Clay_ElementDeclaration& config)
{
	Clay__ConfigureOpenElement(config);
}

void ClayManager::CloseElement()
{
	Clay__CloseElement();
}

void ClayManager::OpenTextElement(const Clay_String& text, const Clay_TextElementConfig& config)
{
	Clay__OpenTextElement(text, Clay__StoreTextElementConfig(config));
}

void ClayManager::SetPointerState(const Clay_Vector2& position, bool isPointerDown)
{
	Clay_SetPointerState(position, isPointerDown);
}

bool ClayManager::IsHovered()
{
	return Clay_Hovered();
}

bool ClayManager::IsPointerOver(Clay_ElementId elementId)
{
	return Clay_PointerOver(elementId);
}
