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
#include "Core/Input.h"
#include "ComponentSystems/UISystem.h"
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

static void OpenText(clay::Context* ctx, std::string_view text, clay::TextElementConfig cfg)
{
	ctx->openTextElement(text, ctx->storeTextConfig(cfg));
}

static uint16_t Font(const char* name, int size)
{
	return Pengine::FontManager::GetInstance().GetFont(name, size)->id;
}

static std::string_view Str(const std::string& s)
{
	return s;
}

static std::string_view Str(const char* s)
{
	return s;
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

static clay::Color RarityColor(int r)
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

static std::vector<clay::RenderCommand>RaidHUDScript(Pengine::Canvas*, clay::Context* ctx, std::shared_ptr<Pengine::Entity>)
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

	ctx->beginLayout();

	// Root
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 1.0f, 0.0f, 0.0f, flashAlpha },
	});

	// Top row: depth + enemies + credits
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::fit(0) },
			.padding         = { .left = 16, .right = 16, .top = 12 },
			.childGap        = 20,
			.childAlignment  = { .x = clay::AlignX::Right },
			.layoutDirection = clay::LayoutDirection::LeftToRight,
		},
	});
	OpenText(ctx,Str(depthText),
		{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	OpenText(ctx,Str(enemyText),
		{ .textColor={0.9f,0.5f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	OpenText(ctx,Str(creditsText),
		{ .textColor={0.9f,0.85f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	ctx->closeElement(); // top row

	// Middle: crosshair + interact prompt
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing         = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.childAlignment = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
	});
	if (!interactLabel.empty())
		OpenText(ctx,Str(interactLabel),
			{ .textColor={0.9f,0.9f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	ctx->closeElement(); // middle

	// Bottom row: health bar + ammo
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::fit(0) },
			.padding         = { .left = 16, .right = 16, .bottom = 16 },
			.childAlignment  = { .y = clay::AlignY::Bottom },
			.layoutDirection = clay::LayoutDirection::LeftToRight,
		},
	});

	{
		float fillW = (health / std::max(maxHealth, 1.0f)) * 200.0f;
		fillW = std::clamp(fillW, 0.0f, 200.0f);
		clay::Color barColor = health > maxHealth * 0.6f
			? clay::Color{0.31f, 0.78f, 0.31f, 0.86f}
			: health > maxHealth * 0.3f
				? clay::Color{0.86f, 0.63f, 0.16f, 0.86f}
				: clay::Color{0.82f, 0.20f, 0.20f, 0.86f};

		ctx->openElement();
		ctx->configureOpenElement({
			.layout = {
				.sizing          = { clay::sizing::fixed(200), clay::sizing::fixed(18) },
				.layoutDirection = clay::LayoutDirection::LeftToRight,
			},
			.backgroundColor = {0.20f, 0.20f, 0.20f, 0.71f},
		});
		ctx->openElement();
		ctx->configureOpenElement({
			.layout          = { .sizing = { clay::sizing::fixed(fillW), clay::sizing::grow(0) } },
			.backgroundColor = barColor,
		});
		ctx->closeElement();
		ctx->closeElement();
		OpenText(ctx,Str(hpText),
			{ .textColor={0.86f,0.86f,0.86f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	}

	// Spacer
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::fit(0) } },
	});
	ctx->closeElement();

	{
		Pengine::Texture* tex = nullptr;
		if (!activeWeaponPreview.empty())
			if (auto t = Pengine::TextureManager::GetInstance().Load(activeWeaponPreview))
				tex = t.get();
		if (!tex) tex = Pengine::TextureManager::GetInstance().GetPink().get();
		ctx->openElement();
		ctx->configureOpenElement({
			.layout = { .sizing = { clay::sizing::fixed(48), clay::sizing::fixed(48) } },
			.backgroundColor = { 1, 1, 1, 1 },
			.image = { .imageData = tex },
		});
		ctx->closeElement();
	}

	{
		clay::Color ammoColor = isReloading
			? clay::Color{0.86f,0.63f,0.16f,1}
			: (ammoMag == 0 ? clay::Color{0.82f,0.20f,0.20f,1} : clay::Color{0.86f,0.86f,0.86f,1});
		OpenText(ctx,Str(ammoText),
			{ .textColor=ammoColor, .fontId=f36, .fontSize=36, .wrapMode=clay::TextWrapMode::None });
	}

	ctx->closeElement(); // bottom row
	ctx->closeElement(); // root

	return ctx->endLayout();
}

// ─── UI script: Crosshair ────────────────────────────────────────────────────

static std::vector<clay::RenderCommand>CrosshairScript(Pengine::Canvas*, clay::Context* ctx, std::shared_ptr<Pengine::Entity>)
{
	uint16_t f36 = Font("Calibri", 36);

	ctx->beginLayout();
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing         = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.childAlignment = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
		},
	});
	OpenText(ctx,Str("+"),
		{ .textColor={1,1,1,0.82f}, .fontId=f36, .fontSize=36, .wrapMode=clay::TextWrapMode::None });
	ctx->closeElement();
	return ctx->endLayout();
}

// ─── UI script: HomeBase HUD ──────────────────────────────────────────────────

static std::vector<clay::RenderCommand>HomeBaseHUDScript(Pengine::Canvas*, clay::Context* ctx, std::shared_ptr<Pengine::Entity>)
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

	ctx->beginLayout();

	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
	});

	// Top-right: credits
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::fit(0) },
			.padding         = { .right = 16, .top = 12 },
			.childAlignment  = { .x = clay::AlignX::Right },
		},
	});
	OpenText(ctx,Str(creditsText),
		{ .textColor={0.9f,0.85f,0.3f,1}, .fontId=f28, .fontSize=28, .wrapMode=clay::TextWrapMode::None });
	ctx->closeElement();

	// Middle: interact prompt
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing         = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.childAlignment = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
		},
	});
	if (nearAnything)
		OpenText(ctx,Str(interactText),
			{ .textColor={0.9f,0.9f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	ctx->closeElement();

	ctx->closeElement(); // root

	return ctx->endLayout();
}

// ─── UI script: Inventory ─────────────────────────────────────────────────────

static std::vector<clay::RenderCommand>InventoryScript(Pengine::Canvas*, clay::Context* ctx, std::shared_ptr<Pengine::Entity>)
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
		ctx->beginLayout();
		ctx->openElement();
		ctx->configureOpenElement({
			.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::grow(0) } },
		});
		ctx->closeElement();
		return ctx->endLayout();
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
			ctx->setPointerState({ (float)mp.x, (float)mp.y },
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

	ctx->beginLayout();

	// Full-screen overlay
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing         = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.childAlignment = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
		},
		.backgroundColor = { 0, 0, 0, 0.55f },
	});

	// Dialog
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::percent(0.66f), clay::sizing::fit(0) },
			.padding         = { .left=20,.right=20,.top=16,.bottom=16 },
			.childGap        = 12,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.08f, 0.08f, 0.10f, 0.97f },
	});

	// Header
	{
		static std::string title;
		title = "INVENTORY   |   Credits: $" + std::to_string(inv.credits);
		OpenText(ctx,Str(title),
			{ .textColor={0.9f,0.85f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	}

	// Two-panel row
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::fit(0) },
			.childGap        = 16,
			.childAlignment  = { .y = clay::AlignY::Top },
			.layoutDirection = clay::LayoutDirection::LeftToRight,
		},
	});

	// ── LEFT: Equipment panel ──────────────────────────────────────────────────
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::fit(0), clay::sizing::fit(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.10f, 0.10f, 0.12f, 1.0f },
	});
	OpenText(ctx,Str("EQUIPPED  (click to unequip)"),
		{ .textColor={0.85f,0.85f,0.9f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });

	// Row: Armor | Backpack
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::grow(0) }, .childGap = kCellGap, .layoutDirection = clay::LayoutDirection::LeftToRight },
	});
	// Armor cell
	{
		const auto& slot = inv.armorSlot;
		static std::string armorName;
		armorName = slot.occupied ? slot.itemName : "Armor";
		clay::Color bg = slot.occupied ? clay::Color{0.15f,0.35f,0.15f,0.95f} : clay::Color{0.13f,0.13f,0.14f,0.7f};
		ctx->openElement();
		ctx->configureOpenElement({
			.id = clay::makeId("EquipArmor"),
			.layout = {
				.sizing          = { clay::sizing::grow(kCellW), clay::sizing::grow(kCellW) },
				.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
				.childGap        = 4,
				.childAlignment  = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
				.layoutDirection = clay::LayoutDirection::TopToBottom,
			},
			.backgroundColor = bg,
		});
		if (slot.occupied)
		{
			ctx->openElement();
			ctx->configureOpenElement({
				.layout = { .sizing = { clay::sizing::fixed(kImgSize), clay::sizing::fixed(kImgSize) } },
				.backgroundColor = { 1,1,1,1 },
				.image = { .imageData = GetItemPreviewTex(slot) },
			});
			ctx->closeElement();
		}
		OpenText(ctx,Str(armorName),
			{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::Words, .textAlignment=clay::TextAlignment::Center });
		ctx->closeElement();
	}
	// Backpack cell
	{
		const auto& slot = inv.backpackSlot;
		static std::string bpName;
		bpName = slot.occupied ? slot.itemName : "Backpack";
		clay::Color bg = slot.occupied ? clay::Color{0.15f,0.25f,0.40f,0.95f} : clay::Color{0.13f,0.13f,0.14f,0.7f};
		ctx->openElement();
		ctx->configureOpenElement({
			.id = clay::makeId("EquipBackpack"),
			.layout = {
				.sizing          = { clay::sizing::grow(kCellW), clay::sizing::grow(kCellW) },
				.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
				.childGap        = 4,
				.childAlignment  = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
				.layoutDirection = clay::LayoutDirection::TopToBottom,
			},
			.backgroundColor = bg,
		});
		if (slot.occupied)
		{
			ctx->openElement();
			ctx->configureOpenElement({
				.layout = { .sizing = { clay::sizing::fixed(kImgSize), clay::sizing::fixed(kImgSize) } },
				.backgroundColor = { 1,1,1,1 },
				.image = { .imageData = GetItemPreviewTex(slot) },
			});
			ctx->closeElement();
		}
		OpenText(ctx,Str(bpName),
			{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::Words, .textAlignment=clay::TextAlignment::Center });
		ctx->closeElement();
	}
	ctx->closeElement(); // armor/backpack row

	// Row: Weapon 0 | Weapon 1
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::grow(0) }, .childGap = kCellGap, .layoutDirection = clay::LayoutDirection::LeftToRight },
	});
	for (int s = 0; s < InventoryComponent::kMaxWeaponSlots; ++s)
	{
		bool occupied = inv.weaponSlotOccupied[s];
		wName[s] = occupied ? inv.weaponSlotNames[s] : ("Weapon " + std::to_string(s + 1));
		auto* tex = occupied ? GetWeaponPreviewTex(playerEnt, s) : nullptr;
		clay::Color bg = occupied ? clay::Color{0.35f,0.15f,0.15f,0.95f} : clay::Color{0.13f,0.13f,0.14f,0.7f};
		ctx->openElement();
		ctx->configureOpenElement({
			.id = clay::makeId("EquipWeapon", s),
			.layout = {
				.sizing          = { clay::sizing::grow(kCellW), clay::sizing::grow(kCellW) },
				.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
				.childGap        = 4,
				.childAlignment  = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
				.layoutDirection = clay::LayoutDirection::TopToBottom,
			},
			.backgroundColor = bg,
		});
		if (occupied && tex)
		{
			ctx->openElement();
			ctx->configureOpenElement({
				.layout = { .sizing = { clay::sizing::fixed(kImgSize), clay::sizing::fixed(kImgSize) } },
				.backgroundColor = { 1,1,1,1 },
				.image = { .imageData = tex },
			});
			ctx->closeElement();
		}
		OpenText(ctx,Str(wName[s]),
			{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::Words, .textAlignment=clay::TextAlignment::Center });
		ctx->closeElement();
	}
	ctx->closeElement(); // weapon row
	ctx->closeElement(); // left equipment panel

	// ── RIGHT: Item grid ───────────────────────────────────────────────────────
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::fit(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.10f, 0.10f, 0.12f, 1.0f },
	});
	{
		static std::string gridTitle;
		gridTitle = "ITEMS  " + std::to_string(inv.currentRows) + "x" + std::to_string(inv.currentCols)
			+ "   LMB equip slot 1   RMB slot 2   [G] Drop";
		OpenText(ctx,Str(gridTitle),
			{ .textColor={0.7f,0.7f,0.7f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	}
	for (int r = 0; r < inv.currentRows; ++r)
	{
		ctx->openElement();
		ctx->configureOpenElement({
			.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::fit(0) }, .childGap = kCellGap, .layoutDirection = clay::LayoutDirection::LeftToRight },
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
			clay::Color bg = slot.occupied
				? clay::Color{0.22f,0.22f,0.28f,0.95f}
				: clay::Color{0.14f,0.14f,0.16f,0.60f};
			ctx->openElement();
			ctx->configureOpenElement({
				.id = clay::makeId("InvCell", flatIdx),
				.layout = {
					.sizing          = { clay::sizing::grow(kCellW), clay::sizing::grow(kCellW) },
					.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
					.childGap        = 4,
					.childAlignment  = { .x = clay::AlignX::Center },
					.layoutDirection = clay::LayoutDirection::TopToBottom,
				},
				.backgroundColor = bg,
			});
			if (slot.occupied)
			{
				ctx->openElement();
				ctx->configureOpenElement({
					.layout = { .sizing = { clay::sizing::fixed(kImgSize), clay::sizing::fixed(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image = { .imageData = GetItemPreviewTex(slot) },
				});
				ctx->closeElement();
				OpenText(ctx,Str(cellNames[flatIdx]),
					{ .textColor=RarityColor(slot.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::Words, .textAlignment=clay::TextAlignment::Center });
				OpenText(ctx,Str(cellPrices[flatIdx]),
					{ .textColor={0.6f,0.6f,0.6f,1}, .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::None, .textAlignment=clay::TextAlignment::Center });
			}
			ctx->closeElement(); // cell

			if (ctx->pointerOver(clay::makeId("InvCell", flatIdx)))
			{
				hoveredFlatIdx = flatIdx;
				if ((lmbClicked || rmbClicked) && clickedFlatIdx < 0)
					clickedFlatIdx = flatIdx;
			}
		}
		ctx->closeElement(); // row
	}
	ctx->closeElement(); // right item grid

	ctx->closeElement(); // two-panel row

	OpenText(ctx,Str("[Tab] Close"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });

	ctx->closeElement(); // dialog
	ctx->closeElement(); // overlay

	auto layout = ctx->endLayout();

	// ── Handle unequip on click ───────────────────────────────────
	if (lmbClicked)
	{
		if (ctx->pointerOver(clay::makeId("EquipArmor")) && inv.armorSlot.occupied)
			InventorySystem::UnequipArmor(inv);
		else if (ctx->pointerOver(clay::makeId("EquipBackpack")) && inv.backpackSlot.occupied)
			InventorySystem::UnequipBackpack(inv, playerEnt, scene);
		else
		{
			for (int s = 0; s < InventoryComponent::kMaxWeaponSlots; ++s)
			{
				if (ctx->pointerOver(clay::makeId("EquipWeapon", s)) && inv.weaponSlotOccupied[s])
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

static std::vector<clay::RenderCommand>ShopScript(Pengine::Canvas*, clay::Context* ctx, std::shared_ptr<Pengine::Entity>)
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
		ctx->beginLayout();
		ctx->openElement();
		ctx->configureOpenElement({
			.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::grow(0) } },
		});
		ctx->closeElement();
		return ctx->endLayout();
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
			ctx->setPointerState({ (float)mp.x, (float)mp.y },
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

	ctx->beginLayout();

	// Full-screen overlay
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing         = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.childAlignment = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
		},
		.backgroundColor = { 0, 0, 0, 0.55f },
	});

	// Dialog — 66% of screen width
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::fit(0.66f), clay::sizing::fit(0) },
			.padding         = { .left=20,.right=20,.top=16,.bottom=16 },
			.childGap        = 12,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.08f, 0.08f, 0.10f, 0.97f },
	});

	// Header
	{
		static std::string title;
		title = std::string(shop.shopName) + "   |   Credits: $" + std::to_string(inv.credits);
		OpenText(ctx,Str(title),
			{ .textColor={0.9f,0.85f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	}

	// ── Two-panel row ─────────────────────────────────────────────────────────
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::fit(0) },
			.childGap        = 16,
			.childAlignment  = { .y = clay::AlignY::Top },
			.layoutDirection = clay::LayoutDirection::LeftToRight,
		},
	});

	// ── LEFT PANEL: player inventory (sell) ───────────────────────────────────
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.11f, 0.10f, 0.09f, 1.0f },
	});
	OpenText(ctx,Str("YOUR ITEMS  (click to sell for 50%)"),
		{ .textColor={0.8f,0.65f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });

	for (int r = 0; r < inv.currentRows; ++r)
	{
		ctx->openElement();
		ctx->configureOpenElement({
			.layout = {
				.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
				.childGap        = kCellGap,
				.layoutDirection = clay::LayoutDirection::LeftToRight,
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
			clay::Color bg = slot.occupied
				? clay::Color{0.22f, 0.17f, 0.10f, 0.95f}
				: clay::Color{0.13f, 0.12f, 0.11f, 0.60f};

			ctx->openElement();
			ctx->configureOpenElement({
				.id = clay::makeId("SellCell", flatIdx),
				.layout = {
					.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
					.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
					.childGap        = 4,
					.childAlignment  = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
					.layoutDirection = clay::LayoutDirection::TopToBottom,
				},
				.backgroundColor = bg,
			});
			if (slot.occupied)
			{
				ctx->openElement();
				ctx->configureOpenElement({
					.layout          = { .sizing = { clay::sizing::fixed(kImgSize), clay::sizing::fixed(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image           = { .imageData = GetItemPreviewTex(slot) },
				});
				ctx->closeElement();
				OpenText(ctx,Str(sellNameStr[flatIdx]),
					{ .textColor=RarityColor(slot.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::None, .textAlignment=clay::TextAlignment::Center });
				OpenText(ctx,Str(sellPriceStr[flatIdx]),
					{ .textColor={0.8f,0.65f,0.3f,1}, .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::None, .textAlignment=clay::TextAlignment::Center });
			}
			ctx->closeElement(); // cell

			if (clicked && sellClickedIdx < 0 && slot.occupied
				&& ctx->pointerOver(clay::makeId("SellCell", flatIdx)))
				sellClickedIdx = flatIdx;
		}
		ctx->closeElement(); // row
	}
	ctx->closeElement(); // left panel

	// ── RIGHT PANEL: trader catalog (buy) ─────────────────────────────────────
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.09f, 0.11f, 0.09f, 1.0f },
	});
	OpenText(ctx,Str("TRADER  (click to buy)"),
		{ .textColor={0.4f,0.85f,0.4f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });

	constexpr int kCatalogCols = 4;
	int catalogCount = (int)shop.catalog.size();
	int catalogRows  = (catalogCount + kCatalogCols - 1) / kCatalogCols;

	for (int r = 0; r < catalogRows; ++r)
	{
		ctx->openElement();
		ctx->configureOpenElement({
			.layout = {
				.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
				.childGap        = kCellGap,
				.layoutDirection = clay::LayoutDirection::LeftToRight,
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
			clay::Color bg     = canAfford
				? clay::Color{0.16f,0.20f,0.16f,0.95f}
				: clay::Color{0.12f,0.13f,0.12f,0.70f};
			clay::Color priceCol = canAfford
				? clay::Color{0.4f,0.9f,0.4f,1}
				: clay::Color{0.75f,0.3f,0.3f,1};

			ctx->openElement();
			ctx->configureOpenElement({
				.id = clay::makeId("BuyCell", i),
				.layout = {
					.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
					.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
					.childGap        = 4,
					.childAlignment  = { .x = clay::AlignX::Center },
					.layoutDirection = clay::LayoutDirection::TopToBottom,
				},
				.backgroundColor = bg,
			});
			ctx->openElement();
			ctx->configureOpenElement({
				.layout          = { .sizing = { clay::sizing::fixed(kImgSize), clay::sizing::fixed(kImgSize) } },
				.backgroundColor = { 1,1,1,1 },
				.image           = { .imageData = GetItemPreviewTex(item) },
			});
			ctx->closeElement();
			OpenText(ctx,Str(buyNameStr[i]),
				{ .textColor=RarityColor(item.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::None, .textAlignment=clay::TextAlignment::Center });
			OpenText(ctx,Str(buyPriceStr[i]),
				{ .textColor=priceCol, .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::None, .textAlignment=clay::TextAlignment::Center });
			ctx->closeElement(); // cell

			if (clicked && buyClickedIndex < 0 && canAfford
				&& ctx->pointerOver(clay::makeId("BuyCell", i)))
				buyClickedIndex = i;
		}
		ctx->closeElement(); // row
	}
	ctx->closeElement(); // right panel

	ctx->closeElement(); // two-panel row

	// Footer
	OpenText(ctx,Str("[E] Close"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });

	ctx->closeElement(); // dialog
	ctx->closeElement(); // overlay

	auto layout = ctx->endLayout();

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

static std::vector<clay::RenderCommand>LootScript(Pengine::Canvas*, clay::Context* ctx, std::shared_ptr<Pengine::Entity>)
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
		ctx->beginLayout();
		ctx->openElement();
		ctx->configureOpenElement({
			.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::grow(0) } },
		});
		ctx->closeElement();
		return ctx->endLayout();
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
			ctx->setPointerState({ (float)mp.x, (float)mp.y },
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

	ctx->beginLayout();

	// Overlay
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing         = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.childAlignment = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
		},
		.backgroundColor = { 0, 0, 0, 0.55f },
	});

	// Dialog
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::percent(0.66f), clay::sizing::fit(0) },
			.padding         = { .left=20,.right=20,.top=16,.bottom=16 },
			.childGap        = 12,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.08f, 0.08f, 0.10f, 0.97f },
	});

	OpenText(ctx,Str("LOOT"),
		{ .textColor={0.9f,0.75f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });

	// Two-panel row
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::fit(0) },
			.childGap        = 16,
			.childAlignment  = { .y = clay::AlignY::Top },
			.layoutDirection = clay::LayoutDirection::LeftToRight,
		},
	});

	// ── LEFT: Player inventory (display) ──────────────────────────────────────
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::fit(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.10f, 0.10f, 0.12f, 1.0f },
	});
	OpenText(ctx,Str("YOUR ITEMS"),
		{ .textColor={0.7f,0.7f,0.7f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	for (int r = 0; r < inv.currentRows; ++r)
	{
		ctx->openElement();
		ctx->configureOpenElement({
			.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::fit(0) }, .childGap = kCellGap, .layoutDirection = clay::LayoutDirection::LeftToRight },
		});
		for (int c = 0; c < inv.currentCols; ++c)
		{
			int flatIdx      = r * InventoryComponent::kMaxGridCols + c;
			const auto& slot = inv.grid[flatIdx];
			if (slot.occupied) invCellNames[flatIdx] = slot.itemName;
			clay::Color bg = slot.occupied ? clay::Color{0.22f,0.22f,0.28f,0.95f} : clay::Color{0.14f,0.14f,0.16f,0.60f};
			ctx->openElement();
			ctx->configureOpenElement({
				.layout = {
					.sizing          = { clay::sizing::fixed(kCellW), clay::sizing::fit(0) },
					.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
					.childGap        = 4,
					.childAlignment  = { .x = clay::AlignX::Center },
					.layoutDirection = clay::LayoutDirection::TopToBottom,
				},
				.backgroundColor = bg,
			});
			if (slot.occupied)
			{
				ctx->openElement();
				ctx->configureOpenElement({
					.layout = { .sizing = { clay::sizing::fixed(kImgSize), clay::sizing::fixed(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image = { .imageData = GetItemPreviewTex(slot) },
				});
				ctx->closeElement();
				OpenText(ctx,Str(invCellNames[flatIdx]),
					{ .textColor=RarityColor(slot.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::Words, .textAlignment=clay::TextAlignment::Center });
			}
			ctx->closeElement();
		}
		ctx->closeElement(); // row
	}
	ctx->closeElement(); // left panel

	// ── RIGHT: Loot items (click to take) ─────────────────────────────────────
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::fit(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.11f, 0.10f, 0.08f, 1.0f },
	});
	OpenText(ctx,Str("CONTAINER  (click to take)"),
		{ .textColor={0.9f,0.75f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	if (loot.items.empty())
	{
		OpenText(ctx,Str("Empty"),
			{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	}
	else
	{
		int count = (int)loot.items.size();
		int rows  = (count + kLootCols - 1) / kLootCols;
		for (int r = 0; r < rows; ++r)
		{
			ctx->openElement();
			ctx->configureOpenElement({
				.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::fit(0) }, .childGap = kCellGap, .layoutDirection = clay::LayoutDirection::LeftToRight },
			});
			for (int c = 0; c < kLootCols; ++c)
			{
				int i = r * kLootCols + c;
				if (i >= count) { continue; }
				const auto& item = loot.items[i];
				lootNames [i] = item.itemName;
				lootPrices[i] = "$" + std::to_string(item.creditValue);
				ctx->openElement();
				ctx->configureOpenElement({
					.id = clay::makeId("LootItem", i),
					.layout = {
						.sizing          = { clay::sizing::fixed(kCellW), clay::sizing::fit(0) },
						.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
						.childGap        = 4,
						.childAlignment  = { .x = clay::AlignX::Center },
						.layoutDirection = clay::LayoutDirection::TopToBottom,
					},
					.backgroundColor = { 0.20f, 0.17f, 0.12f, 0.95f },
				});
				ctx->openElement();
				ctx->configureOpenElement({
					.layout = { .sizing = { clay::sizing::fixed(kImgSize), clay::sizing::fixed(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image = { .imageData = GetItemPreviewTex(item) },
				});
				ctx->closeElement();
				OpenText(ctx,Str(lootNames[i]),
					{ .textColor=RarityColor(item.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::Words, .textAlignment=clay::TextAlignment::Center });
				OpenText(ctx,Str(lootPrices[i]),
					{ .textColor={0.7f,0.65f,0.4f,1}, .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::None, .textAlignment=clay::TextAlignment::Center });
				ctx->closeElement();

				if (clicked && lootClickedIndex < 0
					&& ctx->pointerOver(clay::makeId("LootItem", i)))
					lootClickedIndex = i;
			}
			ctx->closeElement(); // row
		}
	}
	ctx->closeElement(); // right panel

	ctx->closeElement(); // two-panel row

	OpenText(ctx,Str("[E] Close"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });

	ctx->closeElement(); // dialog
	ctx->closeElement(); // overlay

	auto layout = ctx->endLayout();

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

static std::vector<clay::RenderCommand>StashScript(Pengine::Canvas*, clay::Context* ctx, std::shared_ptr<Pengine::Entity>)
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
		ctx->beginLayout();
		ctx->openElement();
		ctx->configureOpenElement({
			.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::grow(0) } },
		});
		ctx->closeElement();
		return ctx->endLayout();
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
			ctx->setPointerState({ (float)mp.x, (float)mp.y },
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

	ctx->beginLayout();

	// Overlay
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing         = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.childAlignment = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
		},
		.backgroundColor = { 0, 0, 0, 0.55f },
	});

	// Dialog
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::percent(0.66f), clay::sizing::percent(0.66f) },
			.padding         = { .left=20,.right=20,.top=16,.bottom=16 },
			.childGap        = 12,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.08f, 0.10f, 0.08f, 0.97f },
	});

	{
		static std::string title;
		title = "STASH   " + std::to_string((int)stash.size()) + " items";
		OpenText(ctx,Str(title),
			{ .textColor={0.6f,0.9f,0.6f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	}

	// Two-panel row
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.childGap        = 16,
			.childAlignment  = { .y = clay::AlignY::Top },
			.layoutDirection = clay::LayoutDirection::LeftToRight,
		},
	});

	// ── LEFT: Player inventory (click to deposit) ─────────────────────────────
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.11f, 0.12f, 0.10f, 1.0f },
	});
	OpenText(ctx,Str("YOUR ITEMS  (click to deposit)"),
		{ .textColor={0.7f,0.9f,0.7f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	for (int r = 0; r < inv.currentRows; ++r)
	{
		ctx->openElement();
		ctx->configureOpenElement({
			.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::grow(0) }, .childGap = kCellGap, .layoutDirection = clay::LayoutDirection::LeftToRight },
		});
		for (int c = 0; c < inv.currentCols; ++c)
		{
			int flatIdx      = r * InventoryComponent::kMaxGridCols + c;
			const auto& slot = inv.grid[flatIdx];
			if (slot.occupied) invCellNames[flatIdx] = slot.itemName;
			clay::Color bg = slot.occupied ? clay::Color{0.22f,0.28f,0.22f,0.95f} : clay::Color{0.14f,0.16f,0.14f,0.60f};
			ctx->openElement();
			ctx->configureOpenElement({
				.id = clay::makeId("StashDeposit", flatIdx),
				.layout = {
					.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
					.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
					.childGap        = 4,
					.childAlignment  = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
					.layoutDirection = clay::LayoutDirection::TopToBottom,
				},
				.backgroundColor = bg,
			});
			if (slot.occupied)
			{
				ctx->openElement();
				ctx->configureOpenElement({
					.layout = { .sizing = { clay::sizing::fixed(kImgSize), clay::sizing::fixed(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image = { .imageData = GetItemPreviewTex(slot) },
				});
				ctx->closeElement();
				OpenText(ctx,Str(invCellNames[flatIdx]),
					{ .textColor=RarityColor(slot.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::Words, .textAlignment=clay::TextAlignment::Center });
			}
			ctx->closeElement();

			if (clicked && depositIdx < 0 && slot.occupied
				&& ctx->pointerOver(clay::makeId("StashDeposit", flatIdx)))
				depositIdx = flatIdx;
		}
		ctx->closeElement(); // row
	}
	ctx->closeElement(); // left panel

	// ── RIGHT: Stash items (click to withdraw) ────────────────────────────────
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.padding         = { .left=10,.right=10,.top=10,.bottom=10 },
			.childGap        = 8,
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0.10f, 0.12f, 0.10f, 1.0f },
	});
	OpenText(ctx,Str("STASH  (click to withdraw)"),
		{ .textColor={0.6f,0.9f,0.6f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	if (stash.empty())
	{
		OpenText(ctx,Str("Empty"),
			{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });
	}
	else
	{
		int count = (int)stash.size();
		int rows  = (count + kStashCols - 1) / kStashCols;
		for (int r = 0; r < rows; ++r)
		{
			ctx->openElement();
			ctx->configureOpenElement({
				.layout = { .sizing = { clay::sizing::grow(0), clay::sizing::grow(0) }, .childGap = kCellGap, .layoutDirection = clay::LayoutDirection::LeftToRight },
			});
			for (int c = 0; c < kStashCols; ++c)
			{
				int i = r * kStashCols + c;
				if (i >= count) continue;
				const auto& slot = stash[i];
				stashNames [i] = slot.itemName;
				stashPrices[i] = std::to_string(slot.creditValue) + " cr";
				ctx->openElement();
				ctx->configureOpenElement({
					.id = clay::makeId("StashItem", i),
					.layout = {
						.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
						.padding         = { .left=4,.right=4,.top=6,.bottom=6 },
						.childGap        = 4,
						.childAlignment  = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
						.layoutDirection = clay::LayoutDirection::TopToBottom,
					},
					.backgroundColor = { 0.14f, 0.22f, 0.14f, 0.95f },
				});
				ctx->openElement();
				ctx->configureOpenElement({
					.layout = { .sizing = { clay::sizing::fixed(kImgSize), clay::sizing::fixed(kImgSize) } },
					.backgroundColor = { 1,1,1,1 },
					.image = { .imageData = GetItemPreviewTex(slot) },
				});
				ctx->closeElement();
				OpenText(ctx,Str(stashNames[i]),
					{ .textColor=RarityColor(slot.rarityInt), .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::Words, .textAlignment=clay::TextAlignment::Center });
				OpenText(ctx,Str(stashPrices[i]),
					{ .textColor={0.6f,0.7f,0.6f,1}, .fontId=f20, .fontSize=20, .wrapMode=clay::TextWrapMode::None, .textAlignment=clay::TextAlignment::Center });
				ctx->closeElement();

				if (clicked && withdrawIdx < 0
					&& ctx->pointerOver(clay::makeId("StashItem", i)))
					withdrawIdx = i;
			}
			ctx->closeElement(); // row
		}
	}
	ctx->closeElement(); // right panel

	ctx->closeElement(); // two-panel row

	OpenText(ctx,Str("[E] Close"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=clay::TextWrapMode::None });

	ctx->closeElement(); // dialog
	ctx->closeElement(); // overlay

	auto layout = ctx->endLayout();

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

static std::vector<clay::RenderCommand>GameOverScript(Pengine::Canvas*, clay::Context* ctx, std::shared_ptr<Pengine::Entity>)
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

	ctx->beginLayout();
	ctx->openElement();
	ctx->configureOpenElement({
		.layout = {
			.sizing          = { clay::sizing::grow(0), clay::sizing::grow(0) },
			.childGap        = 16,
			.childAlignment  = { .x = clay::AlignX::Center, .y = clay::AlignY::Center },
			.layoutDirection = clay::LayoutDirection::TopToBottom,
		},
		.backgroundColor = { 0, 0, 0, overlayAlpha },
	});
	OpenText(ctx,Str(msg),
		{ .textColor={0.86f,0.22f,0.22f,textAlpha}, .fontId=f72, .fontSize=72, .wrapMode=clay::TextWrapMode::None });
	if (died)
		OpenText(ctx,Str("Returning to Home Base..."),
			{ .textColor={0.7f,0.7f,0.7f,textAlpha}, .fontId=f28, .fontSize=28, .wrapMode=clay::TextWrapMode::None });
	ctx->closeElement();
	return ctx->endLayout();
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
	Pengine::UISystem::Scripts()["Crosshair"]   = CrosshairScript;
	Pengine::UISystem::Scripts()["RaidHUD"]     = RaidHUDScript;
	Pengine::UISystem::Scripts()["HomeBaseHUD"] = HomeBaseHUDScript;
	Pengine::UISystem::Scripts()["Inventory"]   = InventoryScript;
	Pengine::UISystem::Scripts()["Shop"]        = ShopScript;
	Pengine::UISystem::Scripts()["Loot"]        = LootScript;
	Pengine::UISystem::Scripts()["Stash"]       = StashScript;
	Pengine::UISystem::Scripts()["GameOver"]    = GameOverScript;

	// Legacy names kept for backward compatibility
	Pengine::UISystem::Scripts()["FPS_HUD"]     = RaidHUDScript;
	Pengine::UISystem::Scripts()["FPS_Overlay"] = GameOverScript;
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
