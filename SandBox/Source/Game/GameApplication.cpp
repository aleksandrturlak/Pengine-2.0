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

// ─── UI script: Raid HUD ─────────────────────────────────────────────────────

static Clay_RenderCommandArray RaidHUDScript(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)
{
	auto scene = Pengine::SceneManager::GetInstance().GetSceneByTag("Main");

	float health = 100.0f, maxHealth = 100.0f, hitFlash = 0.0f;
	int   ammoMag = 0, ammoReserve = 0;
	bool  isReloading = false;
	int   credits = 0, enemiesLeft = 0, raidDepth = 1;
	bool  nearExtract = false;

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
						ammoMag     = wc.currentAmmo;
						isReloading = wc.isReloading;
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

	Pengine::ClayManager::BeginLayout();

	// Full-screen dark overlay
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing         = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = { 0, 0, 0, 0.55f },
	});

	// Inventory panel
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_FIXED(700), CLAY_SIZING_FIT(0) },
			.padding         = { .left=16,.right=16,.top=12,.bottom=16 },
			.childGap        = 12,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.1f, 0.1f, 0.12f, 0.96f },
	});

	// Title + credits
	{
		static std::string title;
		title = "INVENTORY    $" + std::to_string(inv.credits);
		Pengine::ClayManager::OpenTextElement(Str(title),
			{ .textColor={0.9f,0.85f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}

	// ── Equipment row ─────────────────────────────────────────────
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			.childGap        = 8,
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	});

	// Armor slot
	{
		static std::string armorLabel;
		armorLabel = inv.armorSlot.occupied ? inv.armorSlot.itemName : "Armor: --";
		Clay_Color bg = inv.armorSlot.occupied
			? Clay_Color{0.15f,0.35f,0.15f,0.9f}
			: Clay_Color{0.2f,0.2f,0.22f,0.9f};
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.id = CLAY_ID("EquipArmor"),
			.layout = {
				.sizing  = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
				.padding = { .left=8, .right=8, .top=4, .bottom=4 },
				.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			},
			.backgroundColor = bg,
		});
		Pengine::ClayManager::OpenTextElement(Str(armorLabel),
			{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
		Pengine::ClayManager::CloseElement();
	}

	// Backpack slot
	{
		static std::string bpLabel;
		bpLabel = inv.backpackSlot.occupied ? inv.backpackSlot.itemName : "Backpack: --";
		Clay_Color bg = inv.backpackSlot.occupied
			? Clay_Color{0.15f,0.25f,0.40f,0.9f}
			: Clay_Color{0.2f,0.2f,0.22f,0.9f};
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.id = CLAY_ID("EquipBackpack"),
			.layout = {
				.sizing  = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
				.padding = { .left=8, .right=8, .top=4, .bottom=4 },
				.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			},
			.backgroundColor = bg,
		});
		Pengine::ClayManager::OpenTextElement(Str(bpLabel),
			{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
		Pengine::ClayManager::CloseElement();
	}

	// Weapon slots
	for (int s = 0; s < InventoryComponent::kMaxWeaponSlots; ++s)
	{
		static std::string wLabel[InventoryComponent::kMaxWeaponSlots];
		wLabel[s] = inv.weaponSlotOccupied[s]
			? ("Slot " + std::to_string(s+1) + ": " + inv.weaponSlots[s].substr(inv.weaponSlots[s].rfind('/')+1))
			: ("Slot " + std::to_string(s+1) + ": Empty");
		Clay_Color bg = inv.weaponSlotOccupied[s]
			? Clay_Color{0.35f,0.15f,0.15f,0.9f}
			: Clay_Color{0.2f,0.2f,0.22f,0.9f};
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.id = CLAY_IDI("EquipWeapon", s),
			.layout = {
				.sizing  = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
				.padding = { .left=8, .right=8, .top=4, .bottom=4 },
				.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			},
			.backgroundColor = bg,
		});
		Pengine::ClayManager::OpenTextElement(Str(wLabel[s]),
			{ .textColor={0.9f,0.9f,0.9f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
		Pengine::ClayManager::CloseElement();
	}

	Pengine::ClayManager::CloseElement(); // equipment row

	// ── Grid label ────────────────────────────────────────────────
	{
		static std::string gridLabel;
		gridLabel = "Items (" + std::to_string(inv.currentRows) + "x" + std::to_string(inv.currentCols) + "):";
		Pengine::ClayManager::OpenTextElement(Str(gridLabel),
			{ .textColor={0.7f,0.7f,0.7f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}

	// ── Item grid ─────────────────────────────────────────────────
	static std::string cellLabels[InventoryComponent::kMaxGridRows][InventoryComponent::kMaxGridCols];
	static std::string cellSub[InventoryComponent::kMaxGridRows][InventoryComponent::kMaxGridCols];

	for (int r = 0; r < inv.currentRows; ++r)
	{
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = {
				.sizing          = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
				.childGap        = 4,
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
		});
		for (int c = 0; c < inv.currentCols; ++c)
		{
			int flatIdx = r * InventoryComponent::kMaxGridCols + c;
			const auto& slot = inv.grid[flatIdx];
			Clay_Color bg = slot.occupied
				? Clay_Color{0.22f,0.22f,0.28f,0.95f}
				: Clay_Color{0.14f,0.14f,0.16f,0.80f};

			cellLabels[r][c] = slot.occupied ? slot.itemName : "";
			cellSub[r][c]    = slot.occupied
				? (std::string(TypeName(slot.itemTypeInt)) + "  $" + std::to_string(slot.creditValue))
				: "";

			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.id = CLAY_IDI("InvCell", flatIdx),
				.layout = {
					.sizing          = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
					.padding         = { .left=8,.right=8,.top=4,.bottom=4 },
					.childGap        = 2,
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
				.backgroundColor = bg,
			});
			if (slot.occupied)
			{
				Clay_Color nameColor = RarityColor(slot.rarityInt);
				Pengine::ClayManager::OpenTextElement(Str(cellLabels[r][c]),
					{ .textColor=nameColor, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
				Pengine::ClayManager::OpenTextElement(Str(cellSub[r][c]),
					{ .textColor={0.6f,0.6f,0.6f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
			}
			else
			{
				Pengine::ClayManager::OpenTextElement(Str("--"),
					{ .textColor={0.3f,0.3f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
			}
			Pengine::ClayManager::CloseElement();

			if (Pengine::ClayManager::IsPointerOver(CLAY_IDI("InvCell", flatIdx)))
			{
				hoveredFlatIdx = flatIdx;
				if ((lmbClicked || rmbClicked) && clickedFlatIdx < 0)
					clickedFlatIdx = flatIdx;
			}
		}
		Pengine::ClayManager::CloseElement(); // row
	}

	Pengine::ClayManager::OpenTextElement(Str("[Tab] Close  LMB use/equip slot 1  RMB slot 2  [G] Drop"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	Pengine::ClayManager::CloseElement(); // panel
	Pengine::ClayManager::CloseElement(); // overlay

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
	if (gPressed && hoveredFlatIdx >= 0)
	{
		int r = hoveredFlatIdx / InventoryComponent::kMaxGridCols;
		int c = hoveredFlatIdx % InventoryComponent::kMaxGridCols;
		if (inv.grid[hoveredFlatIdx].occupied)
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
			{
				int weaponSlot = rmbClicked ? 1 : 0;
				InventorySystem::EquipWeapon(inv, r, c, weaponSlot, playerEnt, scene);
			}
			else if (t == Type::Heal)
				InventorySystem::UseItem(inv, r, c, playerEnt);
		}
	}

	return Pengine::ClayManager::EndLayout();
}

// ─── UI script: Shop ─────────────────────────────────────────────────────────

static Clay_RenderCommandArray ShopScript(Pengine::Canvas*, std::shared_ptr<Pengine::Entity>)
{
	auto scene = Pengine::SceneManager::GetInstance().GetSceneByTag("Main");

	ShopComponent*     shopPtr = nullptr;
	InventoryComponent* invPtr = nullptr;
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

	bool show = shopPtr && shopPtr->isOpen;

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

	auto& shop = *shopPtr;
	auto& inv  = *invPtr;
	uint16_t f24 = Font("Calibri", 24);

	// Feed pointer state and handle clicks (must be inside BeginLayout/EndLayout)
	bool shopClicked = false;
	int  shopClickedIndex = -1;
	{
		auto win = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
		if (win)
		{
			auto& inp = Pengine::Input::GetInstance(win.get());
			glm::dvec2 mp = inp.GetMousePosition();
			Pengine::ClayManager::SetPointerState({ (float)mp.x, (float)mp.y },
				inp.IsMouseDown(Pengine::KeyCode::MOUSE_BUTTON_1));
			shopClicked = inp.IsMousePressed(Pengine::KeyCode::MOUSE_BUTTON_1);
		}
	}

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

	// Shop panel
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_FIXED(520), CLAY_SIZING_FIT(0) },
			.padding         = { .left=16,.right=16,.top=12,.bottom=16 },
			.childGap        = 8,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.08f, 0.08f, 0.10f, 0.97f },
	});

	{
		static std::string title;
		title = std::string(shop.shopName) + "    Your Credits: $" + std::to_string(inv.credits);
		Pengine::ClayManager::OpenTextElement(Str(title),
			{ .textColor={0.9f,0.85f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}

	// Catalog items
	static std::string shopItemName[64];
	static std::string shopItemSub[64];
	for (int i = 0; i < (int)shop.catalog.size() && i < 64; ++i)
	{
		const auto& item = shop.catalog[i];
		shopItemName[i] = item.itemName;
		shopItemSub[i]  = std::string(TypeName(item.itemTypeInt)) + "   Buy: $" + std::to_string(item.creditValue);

		bool canAfford = inv.credits >= item.creditValue;
		Clay_Color bg = canAfford
			? Clay_Color{0.18f,0.18f,0.22f,0.95f}
			: Clay_Color{0.12f,0.12f,0.14f,0.80f};

		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.id = CLAY_IDI("ShopItem", i),
			.layout = {
				.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
				.padding         = { .left=8,.right=8,.top=8,.bottom=8 },
				.childGap        = 6,
				.childAlignment  = { .y = CLAY_ALIGN_Y_CENTER },
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
			.backgroundColor = bg,
		});
		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.layout = {
				.sizing         = { CLAY_SIZING_FIXED(180), CLAY_SIZING_FIT(0) },
				.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			},
		});
		Pengine::ClayManager::OpenTextElement(Str(shopItemName[i]),
			{ .textColor=RarityColor(item.rarityInt), .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
		Pengine::ClayManager::CloseElement();
		Pengine::ClayManager::OpenTextElement(Str(shopItemSub[i]),
			{ .textColor={0.6f,0.6f,0.6f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
		Pengine::ClayManager::CloseElement();

		if (shopClicked && shopClickedIndex < 0
			&& Pengine::ClayManager::IsPointerOver(CLAY_IDI("ShopItem", i)))
			shopClickedIndex = i;
	}

	if (shopClickedIndex >= 0)
		ShopSystem::BuyItem(shop, shopClickedIndex, inv, playerEnt, scene);

	// ── Sell section ──────────────────────────────────────────────
	Pengine::ClayManager::OpenTextElement(Str("Sell  click item to sell for 50 pct"),
		{ .textColor={0.7f,0.6f,0.4f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	static std::string sellName[InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols];
	static std::string sellSub [InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols];
	int sellClickedIdx = -1;

	for (int r = 0; r < inv.currentRows; ++r)
	for (int c = 0; c < inv.currentCols; ++c)
	{
		int flatIdx = r * InventoryComponent::kMaxGridCols + c;
		const auto& slot = inv.grid[flatIdx];
		if (!slot.occupied) continue;

		int sellPrice = std::max(1, slot.creditValue / 2);
		sellName[flatIdx] = slot.itemName;
		sellSub [flatIdx] = std::string(TypeName(slot.itemTypeInt)) + "   Sell: $" + std::to_string(sellPrice);

		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.id = CLAY_IDI("SellItem", flatIdx),
			.layout = {
				.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
				.padding         = { .left=8,.right=8,.top=8,.bottom=8 },
				.childGap        = 6,
				.childAlignment  = { .y = CLAY_ALIGN_Y_CENTER },
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
			.backgroundColor = { 0.20f, 0.16f, 0.10f, 0.95f },
		});
		Pengine::ClayManager::OpenTextElement(Str(sellName[flatIdx]),
			{ .textColor=RarityColor(slot.rarityInt), .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
		Pengine::ClayManager::OpenTextElement(Str(sellSub[flatIdx]),
			{ .textColor={0.7f,0.6f,0.4f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
		Pengine::ClayManager::CloseElement();

		if (shopClicked && sellClickedIdx < 0
			&& Pengine::ClayManager::IsPointerOver(CLAY_IDI("SellItem", flatIdx)))
			sellClickedIdx = flatIdx;
	}

	if (sellClickedIdx >= 0)
	{
		int r = sellClickedIdx / InventoryComponent::kMaxGridCols;
		int c = sellClickedIdx % InventoryComponent::kMaxGridCols;
		ShopSystem::SellItem(inv, r, c);
	}

	Pengine::ClayManager::OpenTextElement(Str("[E] Close  |  Click top to buy  |  Click bottom to sell"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	Pengine::ClayManager::CloseElement(); // panel
	Pengine::ClayManager::CloseElement(); // overlay

	return Pengine::ClayManager::EndLayout();
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

		// Find any open loot container (s_NearInteractableName can be cleared while UI is open)
		auto lootView = scene->GetRegistry().view<LootContainerComponent>();
		for (auto handle : lootView)
		{
			auto& lc = lootView.get<LootContainerComponent>(handle);
			if (lc.isOpen) { lootPtr = &lc; break; }
		}
	}

	bool show = lootPtr && lootPtr->isOpen && invPtr;

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

	auto& loot = *lootPtr;
	auto& inv  = *invPtr;
	uint16_t f24 = Font("Calibri", 24);

	bool lootClicked = false;
	int  lootClickedIndex = -1;
	{
		auto win = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
		if (win)
		{
			auto& inp = Pengine::Input::GetInstance(win.get());
			glm::dvec2 mp = inp.GetMousePosition();
			Pengine::ClayManager::SetPointerState({ (float)mp.x, (float)mp.y },
				inp.IsMouseDown(Pengine::KeyCode::MOUSE_BUTTON_1));
			lootClicked = inp.IsMousePressed(Pengine::KeyCode::MOUSE_BUTTON_1);
		}
	}
	
	Pengine::ClayManager::BeginLayout();

	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing         = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = { 0, 0, 0, 0.5f },
	});

	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_FIXED(480), CLAY_SIZING_FIT(0) },
			.padding         = { .left=16,.right=16,.top=12,.bottom=16 },
			.childGap        = 8,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = { 0.10f, 0.08f, 0.06f, 0.97f },
	});

	Pengine::ClayManager::OpenTextElement(Str("LOOT"),
		{ .textColor={0.9f,0.75f,0.3f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	if (loot.items.empty())
	{
		Pengine::ClayManager::OpenTextElement(Str("Empty"),
			{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
	}
	else
	{
		static std::string lootItemName[32];
		static std::string lootItemSub[32];
		for (int i = 0; i < (int)loot.items.size() && i < 32; ++i)
		{
			const auto& item = loot.items[i];
			lootItemName[i] = item.itemName;
			lootItemSub[i]  = std::string(TypeName(item.itemTypeInt)) + "  $" + std::to_string(item.creditValue);

			Pengine::ClayManager::OpenElement();
			Pengine::ClayManager::ConfigureOpenElement({
				.id = CLAY_IDI("LootItem", i),
				.layout = {
					.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
					.padding         = { .left=8,.right=8,.top=8,.bottom=8 },
					.childGap        = 8,
					.childAlignment  = { .y = CLAY_ALIGN_Y_CENTER },
					.layoutDirection = CLAY_LEFT_TO_RIGHT,
				},
				.backgroundColor = { 0.18f, 0.15f, 0.12f, 0.95f },
			});
			Pengine::ClayManager::OpenTextElement(Str(lootItemName[i]),
				{ .textColor=RarityColor(item.rarityInt), .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
			Pengine::ClayManager::OpenTextElement(Str(lootItemSub[i]),
				{ .textColor={0.55f,0.55f,0.55f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
			Pengine::ClayManager::CloseElement();

			if (lootClicked && lootClickedIndex < 0
				&& Pengine::ClayManager::IsPointerOver(CLAY_IDI("LootItem", i)))
				lootClickedIndex = i;
		}
	}

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
				if (win && invPtr && !invPtr->inventoryOpen)
					win->DisableCursor();
				// Prevent the LMB click that picked up the last item from firing the weapon
				if (playerEnt && playerEnt->HasComponent<PlayerComponent>())
					playerEnt->GetComponent<PlayerComponent>().shootCooldown = 0.2f;
			}
		}
	}

	Pengine::ClayManager::OpenTextElement(Str("[E] Close  |  Click to take"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	Pengine::ClayManager::CloseElement();
	Pengine::ClayManager::CloseElement();

	return Pengine::ClayManager::EndLayout();
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

	// Panel
	Pengine::ClayManager::OpenElement();
	Pengine::ClayManager::ConfigureOpenElement({
		.layout = {
			.sizing          = { CLAY_SIZING_FIXED(520), CLAY_SIZING_FIT(0) },
			.padding         = { .left=16,.right=16,.top=12,.bottom=16 },
			.childGap        = 10,
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

	// Stash items — click to withdraw
	static std::string stashName[128];
	static std::string stashSub[128];
	for (int i = 0; i < (int)stash.size() && i < 128; ++i)
	{
		const auto& slot = stash[i];
		stashName[i] = slot.itemName;
		stashSub[i]  = std::string(TypeName(slot.itemTypeInt)) + "  " + std::to_string(slot.creditValue) + " cr";

		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.id = CLAY_IDI("StashItem", i),
			.layout = {
				.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
				.padding         = { .left=8,.right=8,.top=8,.bottom=8 },
				.childGap        = 8,
				.childAlignment  = { .y = CLAY_ALIGN_Y_CENTER },
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
			.backgroundColor = { 0.12f, 0.18f, 0.12f, 0.95f },
		});
		Pengine::ClayManager::OpenTextElement(Str(stashName[i]),
			{ .textColor=RarityColor(slot.rarityInt), .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
		Pengine::ClayManager::OpenTextElement(Str(stashSub[i]),
			{ .textColor={0.6f,0.7f,0.6f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
		Pengine::ClayManager::CloseElement();

		if (clicked && withdrawIdx < 0
			&& Pengine::ClayManager::IsPointerOver(CLAY_IDI("StashItem", i)))
			withdrawIdx = i;
	}

	if (stash.empty())
		Pengine::ClayManager::OpenTextElement(Str("Stash is empty"),
			{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	// Player grid items — click to deposit
	Pengine::ClayManager::OpenTextElement(Str("Inventory  click to deposit"),
		{ .textColor={0.7f,0.7f,0.7f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	static std::string gridName[InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols];
	for (int r = 0; r < inv.currentRows; ++r)
	for (int c = 0; c < inv.currentCols; ++c)
	{
		int flatIdx = r * InventoryComponent::kMaxGridCols + c;
		const auto& slot = inv.grid[flatIdx];
		if (!slot.occupied) continue;

		gridName[flatIdx] = slot.itemName;

		Pengine::ClayManager::OpenElement();
		Pengine::ClayManager::ConfigureOpenElement({
			.id = CLAY_IDI("StashDeposit", flatIdx),
			.layout = {
				.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
				.padding         = { .left=8,.right=8,.top=8,.bottom=8 },
				.childAlignment  = { .y = CLAY_ALIGN_Y_CENTER },
			},
			.backgroundColor = { 0.18f, 0.18f, 0.12f, 0.95f },
		});
		Pengine::ClayManager::OpenTextElement(Str(gridName[flatIdx]),
			{ .textColor=RarityColor(slot.rarityInt), .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });
		Pengine::ClayManager::CloseElement();

		if (clicked && depositIdx < 0
			&& Pengine::ClayManager::IsPointerOver(CLAY_IDI("StashDeposit", flatIdx)))
			depositIdx = flatIdx;
	}

	Pengine::ClayManager::OpenTextElement(Str("E Close"),
		{ .textColor={0.5f,0.5f,0.5f,1}, .fontId=f24, .fontSize=24, .wrapMode=CLAY_TEXT_WRAP_NONE });

	Pengine::ClayManager::CloseElement(); // panel
	Pengine::ClayManager::CloseElement(); // overlay

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

	return Pengine::ClayManager::EndLayout();
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
