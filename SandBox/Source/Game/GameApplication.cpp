#include "GameApplication.h"

#include "PlayerSystem.h"
#include "WeaponSystem.h"
#include "ProjectileSystem.h"
#include "EnemySystem.h"
#include "PickupSystem.h"
#include "InventorySystem.h"
#include "GameStateSystem.h"
#include "LootSystem.h"
#include "ShopSystem.h"
#include "InteractableSystem.h"
#include "PatrolSystem.h"

#include "PlayerComponent.h"
#include "WeaponComponent.h"
#include "InventoryComponent.h"
#include "GameStateComponent.h"
#include "ShopComponent.h"
#include "LootContainerComponent.h"
#include "InteractableComponent.h"
#include "RoguelikeState.h"
#include "ItemComponent.h"

#include "Core/MaterialManager.h"
#include "Core/MeshManager.h"
#include "Core/TextureManager.h"
#include "Core/SceneManager.h"
#include "Core/Serializer.h"
#include "Core/FontManager.h"
#include "Core/ClayManager.h"
#include "Core/Input.h"
#include "Core/KeyCode.h"
#include "Core/WindowManager.h"
#include "Core/Window.h"

#include "Components/Canvas.h"
#include "Components/Transform.h"
#include "Components/PointLight.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <string>
#include <algorithm>

// ─── Helpers ─────────────────────────────────────────────────────────────────

static uint16_t Font(const char* name, int size)
{
	return Pengine::FontManager::GetInstance().GetFont(name, size)->id;
}

static Clay_String Str(const std::string& s)
{
	return { false, (int32_t)s.size(), s.c_str() };
}

static Clay_String Str(const char* s)
{
	return { true, (int32_t)strlen(s), s };
}

static const char* RarityName(int r)
{
	switch (r) {
	case 1: return "Uncommon";
	case 2: return "Rare";
	case 3: return "Epic";
	default: return "Common";
	}
}

static Clay_Color RarityColor(int r)
{
	switch (r) {
	case 1: return { 0.4f, 0.9f, 0.4f, 1.0f };
	case 2: return { 0.3f, 0.5f, 1.0f, 1.0f };
	case 3: return { 0.8f, 0.3f, 1.0f, 1.0f };
	default: return { 0.75f, 0.75f, 0.75f, 1.0f };
	}
}

static const char* TypeName(int t)
{
	switch (t) {
	case 0: return "Weapon";
	case 1: return "Armor";
	case 2: return "Backpack";
	case 3: return "Ammo";
	case 4: return "Heal";
	case 5: return "Credits";
	default: return "Item";
	}
}

static Pengine::Texture* GetItemPreviewTex(const InventorySlot& slot)
{
	if (slot.itemTypeInt == static_cast<int>(ItemComponent::Type::Weapon) && !slot.weaponPrefabName.empty())
	{
		std::string imgPath = slot.weaponPrefabName;
		const size_t dot = imgPath.rfind('.');
		if (dot != std::string::npos)
		{
			imgPath.replace(dot, imgPath.size() - dot, ".png");
			if (auto tex = Pengine::TextureManager::GetInstance().Load(imgPath))
				return tex.get();
		}
	}
	if (slot.itemTypeInt == static_cast<int>(ItemComponent::Type::Ammo))
	{
		static const char* ammoPaths[] = {
			"Game/Assets/AmmoPistol/AmmoPistol.png",  // Pistol = 0
			"Game/Assets/AmmoRifle/AmmoRifle.png",    // Rifle  = 1
			nullptr,                                   // Shotgun = 2 (no image yet)
		};
		const int idx = slot.ammoTypeInt;
		if (idx >= 0 && idx < 3 && ammoPaths[idx])
			if (auto tex = Pengine::TextureManager::GetInstance().Load(ammoPaths[idx]))
				return tex.get();
	}
	return Pengine::TextureManager::GetInstance().GetPink().get();
}

static Pengine::Texture* GetWeaponPreviewTex(std::shared_ptr<Pengine::Entity> playerEnt, int slot)
{
	if (playerEnt)
	{
		auto cam = playerEnt->FindEntityInHierarchy("PlayerCamera");
		if (cam)
		{
			for (auto& weak : cam->GetChilds())
			{
				auto child = weak.lock();
				if (!child || !child->HasComponent<WeaponComponent>()) continue;
				auto& wc = child->GetComponent<WeaponComponent>();
				if (wc.slot == slot && !wc.previewImage.empty())
					if (auto tex = Pengine::TextureManager::GetInstance().Load(wc.previewImage))
						return tex.get();
			}
		}
	}
	return Pengine::TextureManager::GetInstance().GetPink().get();
}

// ─── UI script: Raid HUD ─────────────────────────────────────────────────────

static Clay_RenderCommandArray RaidHUDScript(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)
{
	auto scene = Pengine::SceneManager::GetInstance().GetSceneByTag("Main");

	float health = 100.0f, maxHealth = 100.0f, hitFlash = 0.0f;
	int   ammoMag = 0, ammoReserve = 0;
	bool  isReloading = false;
	int   credits = 0, enemiesLeft = 0, raidDepth = 1;
	bool  nearExtract = false;
	static std::string activeWeaponPreview;

	if (scene)
	{
		auto playerEnt = scene->FindEntityByName("Player");
		if (playerEnt && playerEnt->HasComponent<PlayerComponent>())
		{
			auto& pc = playerEnt->GetComponent<PlayerComponent>();
			health    = pc.health;
			maxHealth = pc.maxHealth;
			hitFlash  = pc.hitFlashTimer;
		}
		if (playerEnt && playerEnt->HasComponent<InventoryComponent>())
			credits = playerEnt->GetComponent<InventoryComponent>().credits;

		// Active weapon ammo
		if (playerEnt && playerEnt->HasComponent<PlayerComponent>())
		{
			int activeSlot = playerEnt->GetComponent<PlayerComponent>().activeWeaponSlot;
			// Find active weapon's ammo type, then count matching grid stacks
			if (playerEnt->HasComponent<InventoryComponent>())
			{
				auto& inv = playerEnt->GetComponent<InventoryComponent>();
				auto cam2 = playerEnt->FindEntityInHierarchy("PlayerCamera");
				int weapAmmoType = -1;
				if (cam2)
				{
					for (auto& weak2 : cam2->GetChilds())
					{
						auto child2 = weak2.lock();
						if (!child2 || !child2->HasComponent<WeaponComponent>()) continue;
						auto& wc2 = child2->GetComponent<WeaponComponent>();
						if (wc2.slot == activeSlot) { weapAmmoType = wc2.ammoTypeInt; break; }
					}
				}
				if (weapAmmoType >= 0)
				{
					for (int i = 0; i < InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols; ++i)
					{
						const auto& s = inv.grid[i];
						if (s.occupied && s.itemTypeInt == static_cast<int>(ItemComponent::Type::Ammo)
							&& s.ammoTypeInt == weapAmmoType)
							ammoReserve += s.ammoCount;
					}
				}
			}
			auto cam = playerEnt->FindEntityInHierarchy("PlayerCamera");
			if (cam)
			{
				for (auto& weak : cam->GetChilds())
				{
					auto child = weak.lock();
					if (!child || !child->HasComponent<WeaponComponent>() || !child->IsEnabled()) continue;
					auto& wc = child->GetComponent<WeaponComponent>();
					if (wc.slot == activeSlot)
					{
						ammoMag             = wc.currentAmmo;
						isReloading         = wc.isReloading;
						activeWeaponPreview = wc.previewImage;
						break;
					}
				}
			}
		}

		auto gsEnt = scene->FindEntityByName("GameState");
		if (gsEnt && gsEnt->HasComponent<GameStateComponent>())
		{
			auto& gs = gsEnt->GetComponent<GameStateComponent>();
			enemiesLeft = gs.enemiesRemaining;
			raidDepth   = gs.raidDepth;
		}
	}

	static std::string hpText, ammoText, creditsText, enemyText, depthText, interactLabel;
	hpText       = std::to_string((int)health) + " / " + std::to_string((int)maxHealth);
	ammoText     = isReloading ? "Reloading..." : std::to_string(ammoMag) + " / " + std::to_string(ammoReserve);
	creditsText  = "$" + std::to_string(credits);
	enemyText    = std::to_string(enemiesLeft) + " enemies";
	depthText    = "Depth " + std::to_string(raidDepth);
	interactLabel = InteractableSystem::s_NearInteractableLabel;

	uint16_t f24 = Font("Calibri", 24);
	uint16_t f36 = Font("Calibri", 36);

	float flashAlpha = 0.0f;
	if (hitFlash > 0.0f)
	{
		float t = hitFlash / 0.4f;
		flashAlpha = (t > 0.5f ? (1.0f - t) / 0.5f : t / 0.5f) * 0.47f;
	}

	Pengine::ClayManager::BeginLayout();

	// Root
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 1.0f, 0.0f, 0.0f, flashAlpha },
	});

	// Top row: depth + enemies + credits
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			.padding         = { .left = 16, .right = 16, .top = 12 },
			.childGap        = 20,
			.childAlignment  = { .x = CLAY_ALIGN_X_RIGHT },
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	});
	Pengine::ClayManager::OpenTextElement(Str(depthText),
		{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	Pengine::ClayManager::OpenTextElement(Str(enemyText),
		{ .textColor={0.9f,0.5f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	Pengine::ClayManager::OpenTextElement(Str(creditsText),
		{ .textColor={0.9f,0.85f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	Pengine::ClayManager::CloseElement(); // top row

	// Middle: crosshair + interact prompt
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing         = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
	});
	if (!interactLabel.empty())
		Pengine::ClayManager::OpenTextElement(Str(interactLabel),
			{ .textColor={0.9f,0.9f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	Pengine::ClayManager::CloseElement(); // middle

	// Bottom row: health bar + ammo
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			.padding         = { .left = 16, .right = 16, .bottom = 16 },
			.childAlignment  = { .y = CLAY_ALIGN_Y_BOTTOM },
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	});

	{
		float fillW = (health / std::max(maxHealth, 1.0f)) * 200.0f;
		fillW = std::clamp(fillW, 0.0f, 200.0f);
		Clay_Color barColor = health > maxHealth * 0.6f
			? Clay_Color{0.31f, 0.78f, 0.31f, 0.86f}
			: health > maxHealth * 0.3f
				? Clay_Color{0.86f, 0.63f, 0.16f, 0.86f}
				: Clay_Color{0.82f, 0.20f, 0.20f, 0.86f};

		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = {
				.sizing          = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(18) },
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
			.backgroundColor = {0.20f, 0.20f, 0.20f, 0.71f},
		});
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout          = { .sizing = { CLAY_SIZING_FIXED(fillW), CLAY_SIZING_GROW(0) } },
			.backgroundColor = barColor,
		});
		Pengine::ClayManager::CloseElement();
		Pengine::ClayManager::CloseElement();
		Pengine::ClayManager::OpenTextElement(Str(hpText),
			{ .textColor={0.86f,0.86f,0.86f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}

	// Spacer
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) } },
	});
	Pengine::ClayManager::CloseElement();

	{
		Pengine::Texture* tex = nullptr;
		if (!activeWeaponPreview.empty())
			if (auto t = Pengine::TextureManager::GetInstance().Load(activeWeaponPreview))
				tex = t.get();
		if (!tex) tex = Pengine::TextureManager::GetInstance().GetPink().get();
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) } },
			.backgroundColor = { 1, 1, 1, 1 },
			.image = { .imageData = tex },
		});
		Pengine::ClayManager::CloseElement();
	}

	{
		Clay_Color ammoColor = isReloading
			? Clay_Color{0.86f,0.63f,0.16f,1}
			: (ammoMag == 0 ? Clay_Color{0.82f,0.20f,0.20f,1} : Clay_Color{0.86f,0.86f,0.86f,1});
		Pengine::ClayManager::OpenTextElement(Str(ammoText),
			{ .textColor=ammoColor, .fontId=f36, .fontSize=36, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}

	Pengine::ClayManager::CloseElement(); // bottom row
	Pengine::ClayManager::CloseElement(); // root

	return Pengine::ClayManager::EndLayout();
}

// ─── UI script: Crosshair ────────────────────────────────────────────────────

static Clay_RenderCommandArray CrosshairScript(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)
{
	uint16_t f36 = Font("Calibri", 36);

	Pengine::ClayManager::BeginLayout();
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing         = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
		},
	});
	Pengine::ClayManager::OpenTextElement(Str("+"),
		{ .textColor={1,1,1,0.82f}, .fontId=f36, .fontSize=36, .wrapMode=CLAY_TEXT_WRAP_NONE });
	Pengine::ClayManager::CloseElement();
	return Pengine::ClayManager::EndLayout();
}

// ─── UI script: HomeBase HUD ──────────────────────────────────────────────────

static Clay_RenderCommandArray HomeBaseHUDScript(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)
{
	auto scene = Pengine::SceneManager::GetInstance().GetSceneByTag("Main");

	int credits = 0;
	bool nearAnything = !InteractableSystem::s_NearInteractableLabel.empty();

	if (scene)
	{
		auto p = scene->FindEntityByName("Player");
		if (p && p->HasComponent<InventoryComponent>())
			credits = p->GetComponent<InventoryComponent>().credits;
	}

	static std::string creditsText, interactText;
	creditsText  = "Credits: $" + std::to_string(credits);
	interactText = InteractableSystem::s_NearInteractableLabel;

	uint16_t f28 = Font("Calibri", 28);
	uint16_t f24 = Font("Calibri", 24);

	Pengine::ClayManager::BeginLayout();

	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
	});

	// Top-right: credits
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			.padding         = { .right = 16, .top = 12 },
			.childAlignment  = { .x = CLAY_ALIGN_X_RIGHT },
		},
	});
	Pengine::ClayManager::OpenTextElement(Str(creditsText),
		{ .textColor={0.9f,0.85f,0.3f,1}, .fontId=f28, .fontSize=28, .wrapMode=CLAY_TEXT_WRAP_NONE });
	Pengine::ClayManager::CloseElement();

	// Middle: interact prompt
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing         = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
		},
	});
	if (nearAnything)
		Pengine::ClayManager::OpenTextElement(Str(interactText),
			{ .textColor={0.9f,0.9f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	Pengine::ClayManager::CloseElement();

	Pengine::ClayManager::CloseElement(); // root

	return Pengine::ClayManager::EndLayout();
}

// ─── UI script: Inventory ─────────────────────────────────────────────────────

static Clay_RenderCommandArray InventoryScript(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)
{
	auto scene = Pengine::SceneManager::GetInstance().GetSceneByTag("Main");

	InventoryComponent* invPtr = nullptr;
	std::shared_ptr<Pengine::Entity> playerEnt;

	if (scene)
	{
		playerEnt = scene->FindEntityByName("Player");
		if (playerEnt && playerEnt->HasComponent<InventoryComponent>())
			invPtr = &playerEnt->GetComponent<InventoryComponent>();
	}

	if (!invPtr || !invPtr->inventoryOpen)
	{
		Pengine::ClayManager::BeginLayout();
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
		});
		Pengine::ClayManager::CloseElement();
		return Pengine::ClayManager::EndLayout();
	}

	auto& inv = *invPtr;
	uint16_t f24 = Font("Calibri", 24);
	uint16_t f20 = Font("Calibri", 20);

	bool lmbClicked = false;
	bool rmbClicked = false;
	bool gPressed   = false;
	int  clickedFlatIdx = -1;
	int  hoveredFlatIdx = -1;
	{
		auto win = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
		if (win)
		{
			auto& inp = Pengine::Input::GetInstance(win.get());
			glm::dvec2 mp = inp.GetMousePosition();
			Pengine::ClayManager::SetPointerState({ (float)mp.x, (float)mp.y },
				inp.IsMouseDown(Pengine::KeyCode::MOUSE_BUTTON_1));
			lmbClicked = inp.IsMousePressed(Pengine::KeyCode::MOUSE_BUTTON_1);
			rmbClicked = inp.IsMousePressed(Pengine::KeyCode::MOUSE_BUTTON_2);
			gPressed   = inp.IsKeyPressed(Pengine::KeyCode::KEY_G);
		}
	}

	constexpr int kImgSize = 72;
	constexpr int kCellW   = kImgSize + 16;
	constexpr int kCellGap = 6;

	static std::string cellNames [InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols];
	static std::string cellPrices[InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols];
	static std::string wName[InventoryComponent::kMaxWeaponSlots];

	Pengine::ClayManager::BeginLayout();

	// Full-screen overlay
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing         = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = { 0, 0, 0, 0.55f },
	});

	// Dialog
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_PERCENT(0.66f), CLAY_SIZING_FIT(0) },
			.padding         = { .left=20,.right=20,.top=16,.bottom=16 },
			.childGap        = 12,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.08f, 0.08f, 0.10f, 0.97f },
	});

	// Header
	{
		static std::string title;
		title = "INVENTORY   |   Credits: $" + std::to_string(inv.credits);
		Pengine::ClayManager::OpenTextElement(Str(title),
			{ .textColor={0.9f,0.85f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}

	// Two-panel row
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			.childGap        = 16,
			.childAlignment  = { .y = CLAY_ALIGN_Y_TOP },
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	});

	// ── LEFT: Equipment panel ──────────────────────────────────────────────────
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.10f, 0.10f, 0.12f, 1.0f },
	});
	Pengine::ClayManager::OpenTextElement(Str("EQUIPPED  (click to unequip)"),
		{ .textColor={0.85f,0.85f,0.9f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	// Row: Armor | Backpack
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = kCellGap, .layoutDirection = CLAY_LEFT_TO_RIGHT },
	});
	// Armor cell
	{
		const auto& slot = inv.armorSlot;
		static std::string armorName;
		armorName = slot.occupied ? slot.itemName : "Armor";
		Clay_Color bg = slot.occupied ? Clay_Color{0.15f,0.35f,0.15f,0.95f} : Clay_Color{0.13f,0.13f,0.14f,0.7f};
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.id = CLAY_ID("EquipArmor"),
			.layout = {
				.sizing          = { CLAY_SIZING_GROW(kCellW), CLAY_SIZING_GROW(kCellW) },
				.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
				.childGap        = 4,
				.childAlignment  = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
			},
			.backgroundColor = bg,
		});
		if (slot.occupied)
		{
			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.layout = { .sizing = { CLAY_SIZING_FIXED(kImgSize), CLAY_SIZING_FIXED(kImgSize) } },
				.backgroundColor = { 1,1,1,1 },
				.image = { .imageData = GetItemPreviewTex(slot) },
			});
			Pengine::ClayManager::CloseElement();
		}
		Pengine::ClayManager::OpenTextElement(Str(armorName),
			{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_WORDS, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
		Pengine::ClayManager::CloseElement();
	}
	// Backpack cell
	{
		const auto& slot = inv.backpackSlot;
		static std::string bpName;
		bpName = slot.occupied ? slot.itemName : "Backpack";
		Clay_Color bg = slot.occupied ? Clay_Color{0.15f,0.25f,0.40f,0.95f} : Clay_Color{0.13f,0.13f,0.14f,0.7f};
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.id = CLAY_ID("EquipBackpack"),
			.layout = {
				.sizing          = { CLAY_SIZING_GROW(kCellW), CLAY_SIZING_GROW(kCellW) },
				.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
				.childGap        = 4,
				.childAlignment  = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
			},
			.backgroundColor = bg,
		});
		if (slot.occupied)
		{
			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.layout = { .sizing = { CLAY_SIZING_FIXED(kImgSize), CLAY_SIZING_FIXED(kImgSize) } },
				.backgroundColor = { 1,1,1,1 },
				.image = { .imageData = GetItemPreviewTex(slot) },
			});
			Pengine::ClayManager::CloseElement();
		}
		Pengine::ClayManager::OpenTextElement(Str(bpName),
			{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_WORDS, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
		Pengine::ClayManager::CloseElement();
	}
	Pengine::ClayManager::CloseElement(); // armor/backpack row

	// Row: Weapon 0 | Weapon 1
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = kCellGap, .layoutDirection = CLAY_LEFT_TO_RIGHT },
	});
	for (int s = 0; s < InventoryComponent::kMaxWeaponSlots; ++s)
	{
		bool occupied = inv.weaponSlotOccupied[s];
		wName[s] = occupied ? inv.weaponSlotNames[s] : ("Weapon " + std::to_string(s + 1));
		auto* tex = occupied ? GetWeaponPreviewTex(playerEnt, s) : nullptr;
		Clay_Color bg = occupied ? Clay_Color{0.35f,0.15f,0.15f,0.95f} : Clay_Color{0.13f,0.13f,0.14f,0.7f};
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.id = CLAY_IDI("EquipWeapon", s),
			.layout = {
				.sizing          = { CLAY_SIZING_GROW(kCellW), CLAY_SIZING_GROW(kCellW) },
				.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
				.childGap        = 4,
				.childAlignment  = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
			},
			.backgroundColor = bg,
		});
		if (occupied && tex)
		{
			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.layout = { .sizing = { CLAY_SIZING_FIXED(kImgSize), CLAY_SIZING_FIXED(kImgSize) } },
				.backgroundColor = { 1,1,1,1 },
				.image = { .imageData = tex },
			});
			Pengine::ClayManager::CloseElement();
		}
		Pengine::ClayManager::OpenTextElement(Str(wName[s]),
			{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_WORDS, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
		Pengine::ClayManager::CloseElement();
	}
	Pengine::ClayManager::CloseElement(); // weapon row
	Pengine::ClayManager::CloseElement(); // left equipment panel

	// ── RIGHT: Item grid ───────────────────────────────────────────────────────
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.10f, 0.10f, 0.12f, 1.0f },
	});
	{
		static std::string gridTitle;
		gridTitle = "ITEMS  " + std::to_string(inv.currentRows) + "x" + std::to_string(inv.currentCols)
			+ "   LMB equip slot 1   RMB slot 2   [G] Drop";
		Pengine::ClayManager::OpenTextElement(Str(gridTitle),
			{ .textColor={0.7f,0.7f,0.7f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}
	for (int r = 0; r < inv.currentRows; ++r)
	{
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childGap = kCellGap, .layoutDirection = CLAY_LEFT_TO_RIGHT },
		});
		for (int c = 0; c < inv.currentCols; ++c)
		{
			int flatIdx      = r * InventoryComponent::kMaxGridCols + c;
			const auto& slot = inv.grid[flatIdx];
			if (slot.occupied)
			{
				cellNames [flatIdx] = slot.itemName;
				cellPrices[flatIdx] = std::string(TypeName(slot.itemTypeInt)) + "  $" + std::to_string(slot.creditValue);
			}
			Clay_Color bg = slot.occupied
				? Clay_Color{0.22f,0.22f,0.28f,0.95f}
				: Clay_Color{0.14f,0.14f,0.16f,0.60f};
			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.id = CLAY_IDI("InvCell", flatIdx),
				.layout = {
					.sizing          = { CLAY_SIZING_GROW(kCellW), CLAY_SIZING_GROW(kCellW) },
					.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
					.childGap        = 4,
					.childAlignment  = { .x = CLAY_ALIGN_X_CENTER },
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
				.backgroundColor = bg,
			});
			if (slot.occupied)
			{
				Pengine::ClayManager::OpenElement();
				Pengine::ClayManager::ConfigureOpenElement({
					.layout = { .sizing = { CLAY_SIZING_FIXED(kImgSize), CLAY_SIZING_FIXED(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image = { .imageData = GetItemPreviewTex(slot) },
				});
				Pengine::ClayManager::CloseElement();
				Pengine::ClayManager::OpenTextElement(Str(cellNames[flatIdx]),
					{ .textColor=RarityColor(slot.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_WORDS, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
				Pengine::ClayManager::OpenTextElement(Str(cellPrices[flatIdx]),
					{ .textColor={0.6f,0.6f,0.6f,1}, .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_NONE, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
			}
			Pengine::ClayManager::CloseElement(); // cell

			if (Pengine::ClayManager::IsPointerOver(CLAY_IDI("InvCell", flatIdx)))
			{
				hoveredFlatIdx = flatIdx;
				if ((lmbClicked || rmbClicked) && clickedFlatIdx < 0)
					clickedFlatIdx = flatIdx;
			}
		}
		Pengine::ClayManager::CloseElement(); // row
	}
	Pengine::ClayManager::CloseElement(); // right item grid

	Pengine::ClayManager::CloseElement(); // two-panel row

	Pengine::ClayManager::OpenTextElement(Str("[Tab] Close"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	Pengine::ClayManager::CloseElement(); // dialog
	Pengine::ClayManager::CloseElement(); // overlay

	auto layout = Pengine::ClayManager::EndLayout();

	// ── Handle unequip on click ───────────────────────────────────
	if (lmbClicked)
	{
		if (Pengine::ClayManager::IsPointerOver(CLAY_ID("EquipArmor")) && inv.armorSlot.occupied)
			InventorySystem::UnequipArmor(inv);
		else if (Pengine::ClayManager::IsPointerOver(CLAY_ID("EquipBackpack")) && inv.backpackSlot.occupied)
			InventorySystem::UnequipBackpack(inv, playerEnt, scene);
		else
		{
			for (int s = 0; s < InventoryComponent::kMaxWeaponSlots; ++s)
			{
				if (Pengine::ClayManager::IsPointerOver(CLAY_IDI("EquipWeapon", s)) && inv.weaponSlotOccupied[s])
				{
					InventorySystem::UnequipWeapon(inv, s, playerEnt, scene);
					break;
				}
			}
		}
	}

	// ── Handle drop (G key on hovered cell) ──────────────────────
	if (gPressed && hoveredFlatIdx >= 0 && inv.grid[hoveredFlatIdx].occupied)
	{
		int r = hoveredFlatIdx / InventoryComponent::kMaxGridCols;
		int c = hoveredFlatIdx % InventoryComponent::kMaxGridCols;
		LootSystem::DropItemToWorld(inv, r, c, playerEnt, scene);
	}

	// ── Handle equip on click ─────────────────────────────────────
	if (clickedFlatIdx >= 0)
	{
		int r = clickedFlatIdx / InventoryComponent::kMaxGridCols;
		int c = clickedFlatIdx % InventoryComponent::kMaxGridCols;
		const auto& slot = inv.grid[clickedFlatIdx];
		if (slot.occupied)
		{
			using Type = ItemComponent::Type;
			Type t = static_cast<Type>(slot.itemTypeInt);
			if (t == Type::Armor)
				InventorySystem::EquipArmor(inv, r, c, playerEnt);
			else if (t == Type::Backpack)
				InventorySystem::EquipBackpack(inv, r, c, playerEnt, scene);
			else if (t == Type::Weapon)
				InventorySystem::EquipWeapon(inv, r, c, rmbClicked ? 1 : 0, playerEnt, scene);
			else if (t == Type::Heal)
				InventorySystem::UseItem(inv, r, c, playerEnt);
		}
	}

	return layout;
}

// ─── UI script: Shop ─────────────────────────────────────────────────────────

static Clay_RenderCommandArray ShopScript(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)
{
	auto scene = Pengine::SceneManager::GetInstance().GetSceneByTag("Main");

	ShopComponent*      shopPtr = nullptr;
	InventoryComponent* invPtr  = nullptr;
	std::shared_ptr<Pengine::Entity> playerEnt;

	if (scene)
	{
		playerEnt = scene->FindEntityByName("Player");
		if (playerEnt && playerEnt->HasComponent<InventoryComponent>())
			invPtr = &playerEnt->GetComponent<InventoryComponent>();

		auto shopEnt = scene->FindEntityByName("Trader");
		if (shopEnt && shopEnt->HasComponent<ShopComponent>())
			shopPtr = &shopEnt->GetComponent<ShopComponent>();
	}

	if (!shopPtr || !shopPtr->isOpen)
	{
		Pengine::ClayManager::BeginLayout();
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
		});
		Pengine::ClayManager::CloseElement();
		return Pengine::ClayManager::EndLayout();
	}

	auto& shop = *shopPtr;
	auto& inv  = *invPtr;
	uint16_t f24 = Font("Calibri", 24);
	uint16_t f20 = Font("Calibri", 20);

	bool clicked = false;
	{
		auto win = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
		if (win)
		{
			auto& inp = Pengine::Input::GetInstance(win.get());
			glm::dvec2 mp = inp.GetMousePosition();
			Pengine::ClayManager::SetPointerState({ (float)mp.x, (float)mp.y },
				inp.IsMouseDown(Pengine::KeyCode::MOUSE_BUTTON_1));
			clicked = inp.IsMousePressed(Pengine::KeyCode::MOUSE_BUTTON_1);
		}
	}

	// Cell dimensions
	constexpr int kImgSize = 72;
	constexpr int kCellGap = 6;

	// Per-cell string storage (static — lives across frames)
	static std::string sellNameStr [InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols];
	static std::string sellPriceStr[InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols];
	static std::string buyNameStr [64];
	static std::string buyPriceStr[64];

	int sellClickedIdx  = -1;
	int buyClickedIndex = -1;

	Pengine::ClayManager::BeginLayout();

	// Full-screen overlay
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing         = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = { 0, 0, 0, 0.55f },
	});

	// Dialog — 66% of screen width
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_FIT(0.66f), CLAY_SIZING_FIT(0) },
			.padding         = { .left=20,.right=20,.top=16,.bottom=16 },
			.childGap        = 12,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.08f, 0.08f, 0.10f, 0.97f },
	});

	// Header
	{
		static std::string title;
		title = std::string(shop.shopName) + "   |   Credits: $" + std::to_string(inv.credits);
		Pengine::ClayManager::OpenTextElement(Str(title),
			{ .textColor={0.9f,0.85f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}

	// ── Two-panel row ─────────────────────────────────────────────────────────
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			.childGap        = 16,
			.childAlignment  = { .y = CLAY_ALIGN_Y_TOP },
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	});

	// ── LEFT PANEL: player inventory (sell) ───────────────────────────────────
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.11f, 0.10f, 0.09f, 1.0f },
	});
	Pengine::ClayManager::OpenTextElement(Str("YOUR ITEMS  (click to sell for 50%)"),
		{ .textColor={0.8f,0.65f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	for (int r = 0; r < inv.currentRows; ++r)
	{
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = {
				.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
				.childGap        = kCellGap,
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
		});
		for (int c = 0; c < inv.currentCols; ++c)
		{
			int flatIdx         = r * InventoryComponent::kMaxGridCols + c;
			const auto& slot    = inv.grid[flatIdx];
			int sellPrice       = slot.occupied ? std::max(1, slot.creditValue / 2) : 0;
			if (slot.occupied)
			{
				sellNameStr [flatIdx] = slot.itemName;
				sellPriceStr[flatIdx] = "$" + std::to_string(sellPrice);
			}
			Clay_Color bg = slot.occupied
				? Clay_Color{0.22f, 0.17f, 0.10f, 0.95f}
				: Clay_Color{0.13f, 0.12f, 0.11f, 0.60f};

			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.id = CLAY_IDI("SellCell", flatIdx),
				.layout = {
					.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
					.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
					.childGap        = 4,
					.childAlignment  = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
				.backgroundColor = bg,
			});
			if (slot.occupied)
			{
				Pengine::ClayManager::OpenElement();
				Pengine::ClayManager::ConfigureOpenElement({
					.layout          = { .sizing = { CLAY_SIZING_FIXED(kImgSize), CLAY_SIZING_FIXED(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image           = { .imageData = GetItemPreviewTex(slot) },
				});
				Pengine::ClayManager::CloseElement();
				Pengine::ClayManager::OpenTextElement(Str(sellNameStr[flatIdx]),
					{ .textColor=RarityColor(slot.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_NONE, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
				Pengine::ClayManager::OpenTextElement(Str(sellPriceStr[flatIdx]),
					{ .textColor={0.8f,0.65f,0.3f,1}, .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_NONE, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
			}
			Pengine::ClayManager::CloseElement(); // cell

			if (clicked && sellClickedIdx < 0 && slot.occupied
				&& Pengine::ClayManager::IsPointerOver(CLAY_IDI("SellCell", flatIdx)))
				sellClickedIdx = flatIdx;
		}
		Pengine::ClayManager::CloseElement(); // row
	}
	Pengine::ClayManager::CloseElement(); // left panel

	// ── RIGHT PANEL: trader catalog (buy) ─────────────────────────────────────
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.09f, 0.11f, 0.09f, 1.0f },
	});
	Pengine::ClayManager::OpenTextElement(Str("TRADER  (click to buy)"),
		{ .textColor={0.4f,0.85f,0.4f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	constexpr int kCatalogCols = 4;
	int catalogCount = (int)shop.catalog.size();
	int catalogRows  = (catalogCount + kCatalogCols - 1) / kCatalogCols;

	for (int r = 0; r < catalogRows; ++r)
	{
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = {
				.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
				.childGap        = kCellGap,
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
		});
		for (int c = 0; c < kCatalogCols; ++c)
		{
			int i = r * kCatalogCols + c;
			if (i >= catalogCount)
			{
				continue;
			}

			const auto& item  = shop.catalog[i];
			bool canAfford    = inv.credits >= item.creditValue;
			buyNameStr [i]    = item.itemName;
			buyPriceStr[i]    = "$" + std::to_string(item.creditValue);
			Clay_Color bg     = canAfford
				? Clay_Color{0.16f,0.20f,0.16f,0.95f}
				: Clay_Color{0.12f,0.13f,0.12f,0.70f};
			Clay_Color priceCol = canAfford
				? Clay_Color{0.4f,0.9f,0.4f,1}
				: Clay_Color{0.75f,0.3f,0.3f,1};

			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.id = CLAY_IDI("BuyCell", i),
				.layout = {
					.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
					.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
					.childGap        = 4,
					.childAlignment  = { .x = CLAY_ALIGN_X_CENTER },
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
				.backgroundColor = bg,
			});
			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.layout          = { .sizing = { CLAY_SIZING_FIXED(kImgSize), CLAY_SIZING_FIXED(kImgSize) } },
				.backgroundColor = { 1,1,1,1 },
				.image           = { .imageData = GetItemPreviewTex(item) },
			});
			Pengine::ClayManager::CloseElement();
			Pengine::ClayManager::OpenTextElement(Str(buyNameStr[i]),
				{ .textColor=RarityColor(item.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_NONE, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
			Pengine::ClayManager::OpenTextElement(Str(buyPriceStr[i]),
				{ .textColor=priceCol, .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_NONE, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
			Pengine::ClayManager::CloseElement(); // cell

			if (clicked && buyClickedIndex < 0 && canAfford
				&& Pengine::ClayManager::IsPointerOver(CLAY_IDI("BuyCell", i)))
				buyClickedIndex = i;
		}
		Pengine::ClayManager::CloseElement(); // row
	}
	Pengine::ClayManager::CloseElement(); // right panel

	Pengine::ClayManager::CloseElement(); // two-panel row

	// Footer
	Pengine::ClayManager::OpenTextElement(Str("[E] Close"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	Pengine::ClayManager::CloseElement(); // dialog
	Pengine::ClayManager::CloseElement(); // overlay

	auto layout = Pengine::ClayManager::EndLayout();

	if (sellClickedIdx >= 0)
	{
		int r = sellClickedIdx / InventoryComponent::kMaxGridCols;
		int c = sellClickedIdx % InventoryComponent::kMaxGridCols;
		ShopSystem::SellItem(inv, r, c);
	}
	if (buyClickedIndex >= 0)
		ShopSystem::BuyItem(shop, buyClickedIndex, inv, playerEnt, scene);

	return layout;
}

// ─── UI script: Loot container ───────────────────────────────────────────────

static Clay_RenderCommandArray LootScript(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)
{
	auto scene = Pengine::SceneManager::GetInstance().GetSceneByTag("Main");

	LootContainerComponent* lootPtr = nullptr;
	InventoryComponent*     invPtr  = nullptr;
	std::shared_ptr<Pengine::Entity> playerEnt;

	if (scene)
	{
		playerEnt = scene->FindEntityByName("Player");
		if (playerEnt && playerEnt->HasComponent<InventoryComponent>())
			invPtr = &playerEnt->GetComponent<InventoryComponent>();

		auto lootView = scene->GetRegistry().view<LootContainerComponent>();
		for (auto handle : lootView)
		{
			auto& lc = lootView.get<LootContainerComponent>(handle);
			if (lc.isOpen) { lootPtr = &lc; break; }
		}
	}

	if (!lootPtr || !lootPtr->isOpen || !invPtr)
	{
		Pengine::ClayManager::BeginLayout();
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
		});
		Pengine::ClayManager::CloseElement();
		return Pengine::ClayManager::EndLayout();
	}

	auto& loot = *lootPtr;
	auto& inv  = *invPtr;
	uint16_t f24 = Font("Calibri", 24);
	uint16_t f20 = Font("Calibri", 20);

	bool clicked = false;
	int  lootClickedIndex = -1;
	{
		auto win = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
		if (win)
		{
			auto& inp = Pengine::Input::GetInstance(win.get());
			glm::dvec2 mp = inp.GetMousePosition();
			Pengine::ClayManager::SetPointerState({ (float)mp.x, (float)mp.y },
				inp.IsMouseDown(Pengine::KeyCode::MOUSE_BUTTON_1));
			clicked = inp.IsMousePressed(Pengine::KeyCode::MOUSE_BUTTON_1);
		}
	}

	constexpr int kImgSize   = 72;
	constexpr int kCellW     = kImgSize + 16;
	constexpr int kCellGap   = 6;
	constexpr int kLootCols  = 4;

	static std::string invCellNames [InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols];
	static std::string lootNames [32];
	static std::string lootPrices[32];

	Pengine::ClayManager::BeginLayout();

	// Overlay
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing         = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = { 0, 0, 0, 0.55f },
	});

	// Dialog
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_PERCENT(0.66f), CLAY_SIZING_FIT(0) },
			.padding         = { .left=20,.right=20,.top=16,.bottom=16 },
			.childGap        = 12,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.08f, 0.08f, 0.10f, 0.97f },
	});

	Pengine::ClayManager::OpenTextElement(Str("LOOT"),
		{ .textColor={0.9f,0.75f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	// Two-panel row
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			.childGap        = 16,
			.childAlignment  = { .y = CLAY_ALIGN_Y_TOP },
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	});

	// ── LEFT: Player inventory (display) ──────────────────────────────────────
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.10f, 0.10f, 0.12f, 1.0f },
	});
	Pengine::ClayManager::OpenTextElement(Str("YOUR ITEMS"),
		{ .textColor={0.7f,0.7f,0.7f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	for (int r = 0; r < inv.currentRows; ++r)
	{
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childGap = kCellGap, .layoutDirection = CLAY_LEFT_TO_RIGHT },
		});
		for (int c = 0; c < inv.currentCols; ++c)
		{
			int flatIdx      = r * InventoryComponent::kMaxGridCols + c;
			const auto& slot = inv.grid[flatIdx];
			if (slot.occupied) invCellNames[flatIdx] = slot.itemName;
			Clay_Color bg = slot.occupied ? Clay_Color{0.22f,0.22f,0.28f,0.95f} : Clay_Color{0.14f,0.14f,0.16f,0.60f};
			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.layout = {
					.sizing          = { CLAY_SIZING_FIXED(kCellW), CLAY_SIZING_FIT(0) },
					.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
					.childGap        = 4,
					.childAlignment  = { .x = CLAY_ALIGN_X_CENTER },
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
				.backgroundColor = bg,
			});
			if (slot.occupied)
			{
				Pengine::ClayManager::OpenElement();
				Pengine::ClayManager::ConfigureOpenElement({
					.layout = { .sizing = { CLAY_SIZING_FIXED(kImgSize), CLAY_SIZING_FIXED(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image = { .imageData = GetItemPreviewTex(slot) },
				});
				Pengine::ClayManager::CloseElement();
				Pengine::ClayManager::OpenTextElement(Str(invCellNames[flatIdx]),
					{ .textColor=RarityColor(slot.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_WORDS, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
			}
			Pengine::ClayManager::CloseElement();
		}
		Pengine::ClayManager::CloseElement(); // row
	}
	Pengine::ClayManager::CloseElement(); // left panel

	// ── RIGHT: Loot items (click to take) ─────────────────────────────────────
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.11f, 0.10f, 0.08f, 1.0f },
	});
	Pengine::ClayManager::OpenTextElement(Str("CONTAINER  (click to take)"),
		{ .textColor={0.9f,0.75f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	if (loot.items.empty())
	{
		Pengine::ClayManager::OpenTextElement(Str("Empty"),
			{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}
	else
	{
		int count = (int)loot.items.size();
		int rows  = (count + kLootCols - 1) / kLootCols;
		for (int r = 0; r < rows; ++r)
		{
			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childGap = kCellGap, .layoutDirection = CLAY_LEFT_TO_RIGHT },
			});
			for (int c = 0; c < kLootCols; ++c)
			{
				int i = r * kLootCols + c;
				if (i >= count) { continue; }
				const auto& item = loot.items[i];
				lootNames [i] = item.itemName;
				lootPrices[i] = "$" + std::to_string(item.creditValue);
				Pengine::ClayManager::OpenElement();
				Pengine::ClayManager::ConfigureOpenElement({
					.id = CLAY_IDI("LootItem", i),
					.layout = {
						.sizing          = { CLAY_SIZING_FIXED(kCellW), CLAY_SIZING_FIT(0) },
						.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
						.childGap        = 4,
						.childAlignment  = { .x = CLAY_ALIGN_X_CENTER },
						.layoutDirection = CLAY_TOP_TO_BOTTOM,
					},
					.backgroundColor = { 0.20f, 0.17f, 0.12f, 0.95f },
				});
				Pengine::ClayManager::OpenElement();
				Pengine::ClayManager::ConfigureOpenElement({
					.layout = { .sizing = { CLAY_SIZING_FIXED(kImgSize), CLAY_SIZING_FIXED(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image = { .imageData = GetItemPreviewTex(item) },
				});
				Pengine::ClayManager::CloseElement();
				Pengine::ClayManager::OpenTextElement(Str(lootNames[i]),
					{ .textColor=RarityColor(item.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_WORDS, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
				Pengine::ClayManager::OpenTextElement(Str(lootPrices[i]),
					{ .textColor={0.7f,0.65f,0.4f,1}, .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_NONE, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
				Pengine::ClayManager::CloseElement();

				if (clicked && lootClickedIndex < 0
					&& Pengine::ClayManager::IsPointerOver(CLAY_IDI("LootItem", i)))
					lootClickedIndex = i;
			}
			Pengine::ClayManager::CloseElement(); // row
		}
	}
	Pengine::ClayManager::CloseElement(); // right panel

	Pengine::ClayManager::CloseElement(); // two-panel row

	Pengine::ClayManager::OpenTextElement(Str("[E] Close"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	Pengine::ClayManager::CloseElement(); // dialog
	Pengine::ClayManager::CloseElement(); // overlay

	auto layout = Pengine::ClayManager::EndLayout();

	if (lootClickedIndex >= 0 && lootClickedIndex < (int)loot.items.size())
	{
		if (InventorySystem::GiveItem(inv, loot.items[lootClickedIndex], playerEnt, scene))
		{
			loot.items.erase(loot.items.begin() + lootClickedIndex);
			if (loot.items.empty())
			{
				loot.isOpen   = false;
				loot.isLooted = true;
				auto win = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
				if (win && !inv.inventoryOpen)
					win->DisableCursor();
				if (playerEnt && playerEnt->HasComponent<PlayerComponent>())
					playerEnt->GetComponent<PlayerComponent>().shootCooldown = 0.2f;
			}
		}
	}

	return layout;
}

// ─── UI script: Stash ────────────────────────────────────────────────────────

static Clay_RenderCommandArray StashScript(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)
{
	auto scene = Pengine::SceneManager::GetInstance().GetSceneByTag("Main");

	InteractableComponent* icPtr  = nullptr;
	InventoryComponent*    invPtr = nullptr;
	std::shared_ptr<Pengine::Entity> playerEnt;

	if (scene)
	{
		playerEnt = scene->FindEntityByName("Player");
		if (playerEnt && playerEnt->HasComponent<InventoryComponent>())
			invPtr = &playerEnt->GetComponent<InventoryComponent>();

		auto stashEnt = scene->FindEntityByName("Stash");
		if (stashEnt && stashEnt->HasComponent<InteractableComponent>())
			icPtr = &stashEnt->GetComponent<InteractableComponent>();
	}

	bool show = icPtr && icPtr->isOpen;

	if (!show)
	{
		Pengine::ClayManager::BeginLayout();
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
		});
		Pengine::ClayManager::CloseElement();
		return Pengine::ClayManager::EndLayout();
	}

	auto& inv   = *invPtr;
	auto& stash = RoguelikeState::GetInstance().stash;
	uint16_t f24 = Font("Calibri", 24);
	uint16_t f20 = Font("Calibri", 20);

	bool clicked = false;
	int  withdrawIdx = -1;
	int  depositIdx  = -1;
	{
		auto win = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
		if (win)
		{
			auto& inp = Pengine::Input::GetInstance(win.get());
			glm::dvec2 mp = inp.GetMousePosition();
			Pengine::ClayManager::SetPointerState({ (float)mp.x, (float)mp.y },
				inp.IsMouseDown(Pengine::KeyCode::MOUSE_BUTTON_1));
			clicked = inp.IsMousePressed(Pengine::KeyCode::MOUSE_BUTTON_1);
		}
	}

	constexpr int kImgSize  = 72;
	constexpr int kCellW    = kImgSize + 16;
	constexpr int kCellGap  = 6;
	constexpr int kStashCols = 4;

	static std::string invCellNames[InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols];
	static std::string stashNames[128];
	static std::string stashPrices[128];

	Pengine::ClayManager::BeginLayout();

	// Overlay
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing         = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = { 0, 0, 0, 0.55f },
	});

	// Dialog
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_PERCENT(0.66f), CLAY_SIZING_PERCENT(0.66f) },
			.padding         = { .left=20,.right=20,.top=16,.bottom=16 },
			.childGap        = 12,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.08f, 0.10f, 0.08f, 0.97f },
	});

	{
		static std::string title;
		title = "STASH   " + std::to_string((int)stash.size()) + " items";
		Pengine::ClayManager::OpenTextElement(Str(title),
			{ .textColor={0.6f,0.9f,0.6f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}

	// Two-panel row
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childGap        = 16,
			.childAlignment  = { .y = CLAY_ALIGN_Y_TOP },
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	});

	// ── LEFT: Player inventory (click to deposit) ─────────────────────────────
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.11f, 0.12f, 0.10f, 1.0f },
	});
	Pengine::ClayManager::OpenTextElement(Str("YOUR ITEMS  (click to deposit)"),
		{ .textColor={0.7f,0.9f,0.7f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	for (int r = 0; r < inv.currentRows; ++r)
	{
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = kCellGap, .layoutDirection = CLAY_LEFT_TO_RIGHT },
		});
		for (int c = 0; c < inv.currentCols; ++c)
		{
			int flatIdx      = r * InventoryComponent::kMaxGridCols + c;
			const auto& slot = inv.grid[flatIdx];
			if (slot.occupied) invCellNames[flatIdx] = slot.itemName;
			Clay_Color bg = slot.occupied ? Clay_Color{0.22f,0.28f,0.22f,0.95f} : Clay_Color{0.14f,0.16f,0.14f,0.60f};
			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.id = CLAY_IDI("StashDeposit", flatIdx),
				.layout = {
					.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
					.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
					.childGap        = 4,
					.childAlignment  = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
				.backgroundColor = bg,
			});
			if (slot.occupied)
			{
				Pengine::ClayManager::OpenElement();
				Pengine::ClayManager::ConfigureOpenElement({
					.layout = { .sizing = { CLAY_SIZING_FIXED(kImgSize), CLAY_SIZING_FIXED(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image = { .imageData = GetItemPreviewTex(slot) },
				});
				Pengine::ClayManager::CloseElement();
				Pengine::ClayManager::OpenTextElement(Str(invCellNames[flatIdx]),
					{ .textColor=RarityColor(slot.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_WORDS, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
			}
			Pengine::ClayManager::CloseElement();

			if (clicked && depositIdx < 0 && slot.occupied
				&& Pengine::ClayManager::IsPointerOver(CLAY_IDI("StashDeposit", flatIdx)))
				depositIdx = flatIdx;
		}
		Pengine::ClayManager::CloseElement(); // row
	}
	Pengine::ClayManager::CloseElement(); // left panel

	// ── RIGHT: Stash items (click to withdraw) ────────────────────────────────
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.10f, 0.12f, 0.10f, 1.0f },
	});
	Pengine::ClayManager::OpenTextElement(Str("STASH  (click to withdraw)"),
		{ .textColor={0.6f,0.9f,0.6f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	if (stash.empty())
	{
		Pengine::ClayManager::OpenTextElement(Str("Empty"),
			{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}
	else
	{
		int count = (int)stash.size();
		int rows  = (count + kStashCols - 1) / kStashCols;
		for (int r = 0; r < rows; ++r)
		{
			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = kCellGap, .layoutDirection = CLAY_LEFT_TO_RIGHT },
			});
			for (int c = 0; c < kStashCols; ++c)
			{
				int i = r * kStashCols + c;
				if (i >= count) continue;
				const auto& slot = stash[i];
				stashNames [i] = slot.itemName;
				stashPrices[i] = std::to_string(slot.creditValue) + " cr";
				Pengine::ClayManager::OpenElement();
				Pengine::ClayManager::ConfigureOpenElement({
					.id = CLAY_IDI("StashItem", i),
					.layout = {
						.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
						.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
						.childGap        = 4,
						.childAlignment  = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
						.layoutDirection = CLAY_TOP_TO_BOTTOM,
					},
					.backgroundColor = { 0.14f, 0.22f, 0.14f, 0.95f },
				});
				Pengine::ClayManager::OpenElement();
				Pengine::ClayManager::ConfigureOpenElement({
					.layout = { .sizing = { CLAY_SIZING_FIXED(kImgSize), CLAY_SIZING_FIXED(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image = { .imageData = GetItemPreviewTex(slot) },
				});
				Pengine::ClayManager::CloseElement();
				Pengine::ClayManager::OpenTextElement(Str(stashNames[i]),
					{ .textColor=RarityColor(slot.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_WORDS, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
				Pengine::ClayManager::OpenTextElement(Str(stashPrices[i]),
					{ .textColor={0.6f,0.7f,0.6f,1}, .fontId=f20, .fontSize=20, .wrapMode=CLAY_TEXT_WRAP_NONE, .textAlignment=CLAY_TEXT_ALIGN_CENTER });
				Pengine::ClayManager::CloseElement();

				if (clicked && withdrawIdx < 0
					&& Pengine::ClayManager::IsPointerOver(CLAY_IDI("StashItem", i)))
					withdrawIdx = i;
			}
			Pengine::ClayManager::CloseElement(); // row
		}
	}
	Pengine::ClayManager::CloseElement(); // right panel

	Pengine::ClayManager::CloseElement(); // two-panel row

	Pengine::ClayManager::OpenTextElement(Str("[E] Close"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	Pengine::ClayManager::CloseElement(); // dialog
	Pengine::ClayManager::CloseElement(); // overlay

	auto layout = Pengine::ClayManager::EndLayout();

	// Handle withdraw: move stash[i] → player grid
	if (withdrawIdx >= 0 && withdrawIdx < (int)stash.size())
	{
		if (inv.AddToGrid(stash[withdrawIdx]))
			stash.erase(stash.begin() + withdrawIdx);
	}

	// Handle deposit: move grid slot → stash
	if (depositIdx >= 0)
	{
		int r = depositIdx / InventoryComponent::kMaxGridCols;
		int c = depositIdx % InventoryComponent::kMaxGridCols;
		if (r < inv.currentRows && c < inv.currentCols)
		{
			auto& slot = inv.grid[depositIdx];
			if (slot.occupied)
			{
				stash.push_back(slot);
				inv.ClearGridSlot(r, c);
			}
		}
	}

	return layout;
}

// ─── UI script: Game Over / Death overlay ─────────────────────────────────────

static Clay_RenderCommandArray GameOverScript(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)
{
	auto scene = Pengine::SceneManager::GetInstance().GetSceneByTag("Main");

	bool died = false;
	if (scene)
	{
		auto gsEnt = scene->FindEntityByName("GameState");
		if (gsEnt && gsEnt->HasComponent<GameStateComponent>())
			died = gsEnt->GetComponent<GameStateComponent>().playerDied;
	}

	static std::string msg;
	msg = "YOU DIED";
	float overlayAlpha = died ? 0.65f : 0.0f;
	float textAlpha    = died ? 1.0f  : 0.0f;

	uint16_t f72 = Font("Calibri", 72);
	uint16_t f28 = Font("Calibri", 28);

	Pengine::ClayManager::BeginLayout();
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childGap        = 16,
			.childAlignment  = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0, 0, 0, overlayAlpha },
	});
	Pengine::ClayManager::OpenTextElement(Str(msg),
		{ .textColor={0.86f,0.22f,0.22f,textAlpha}, .fontId=f72, .fontSize=72, .wrapMode=CLAY_TEXT_WRAP_NONE });
	if (died)
		Pengine::ClayManager::OpenTextElement(Str("Returning to Home Base..."),
			{ .textColor={0.7f,0.7f,0.7f,textAlpha}, .fontId=f28, .fontSize=28, .wrapMode=CLAY_TEXT_WRAP_NONE });
	Pengine::ClayManager::CloseElement();
	return Pengine::ClayManager::EndLayout();
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void GameApplication::OnPreStart()
{
	Pengine::SceneManager::GetInstance().SetComponentSystem<PlayerSystem>("PlayerSystem");
	Pengine::SceneManager::GetInstance().SetComponentSystem<WeaponSystem>("WeaponSystem");
	Pengine::SceneManager::GetInstance().SetComponentSystem<ProjectileSystem>("ProjectileSystem");
	Pengine::SceneManager::GetInstance().SetComponentSystem<EnemySystem>("EnemySystem");
	Pengine::SceneManager::GetInstance().SetComponentSystem<PatrolSystem>("PatrolSystem");
	Pengine::SceneManager::GetInstance().SetComponentSystem<PickupSystem>("PickupSystem");
	Pengine::SceneManager::GetInstance().SetComponentSystem<InventorySystem>("InventorySystem");
	Pengine::SceneManager::GetInstance().SetComponentSystem<LootSystem>("LootSystem");
	Pengine::SceneManager::GetInstance().SetComponentSystem<ShopSystem>("ShopSystem");
	Pengine::SceneManager::GetInstance().SetComponentSystem<InteractableSystem>("InteractableSystem");
	Pengine::SceneManager::GetInstance().SetComponentSystem<GameStateSystem>("GameStateSystem");
}

void GameApplication::RegisterUIScripts()
{
	using ScriptFn = std::function<Clay_RenderCommandArray(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)>;

	Pengine::ClayManager::GetInstance().scriptsByName["Crosshair"]   = ScriptFn(CrosshairScript);
	Pengine::ClayManager::GetInstance().scriptsByName["RaidHUD"]     = ScriptFn(RaidHUDScript);
	Pengine::ClayManager::GetInstance().scriptsByName["HomeBaseHUD"] = ScriptFn(HomeBaseHUDScript);
	Pengine::ClayManager::GetInstance().scriptsByName["Inventory"]   = ScriptFn(InventoryScript);
	Pengine::ClayManager::GetInstance().scriptsByName["Shop"]        = ScriptFn(ShopScript);
	Pengine::ClayManager::GetInstance().scriptsByName["Loot"]        = ScriptFn(LootScript);
	Pengine::ClayManager::GetInstance().scriptsByName["Stash"]       = ScriptFn(StashScript);
	Pengine::ClayManager::GetInstance().scriptsByName["GameOver"]    = ScriptFn(GameOverScript);

	// Legacy names kept for backward compatibility
	Pengine::ClayManager::GetInstance().scriptsByName["FPS_HUD"]     = ScriptFn(RaidHUDScript);
	Pengine::ClayManager::GetInstance().scriptsByName["FPS_Overlay"] = ScriptFn(GameOverScript);
}

void GameApplication::OnStart()
{
	RegisterUIScripts();

	// Cache commonly-used assets so they're never evicted
	m_CachedMaterials.emplace_back(Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/BulletHole.mat"));
	m_CachedMaterials.emplace_back(Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/HealthPickUp.mat"));
	m_CachedMaterials.emplace_back(Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/AmmoPickUp.mat"));
	m_CachedMaterials.emplace_back(Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/Projectile.mat"));
	m_CachedMaterials.emplace_back(Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/EnemyBase.mat"));
	m_CachedMaterials.emplace_back(Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/EnemyDissolve.mat"));

	m_CachedMeshes.emplace_back(Pengine::MeshManager::GetInstance().LoadMesh("Meshes/Cube.mesh"));
	m_CachedMeshes.emplace_back(Pengine::MeshManager::GetInstance().LoadMesh("Meshes/Sphere.mesh"));

	// Start with HomeBase
	Pengine::Serializer::DeserializeScene("Game/HomeBase.scene");
}


void GameApplication::OnClose()
{
	m_CachedMaterials.clear();
	m_CachedMeshes.clear();
}
