#include "SeaOfThieves.h"

HMODULE SeaOfThieves::hMod = NULL;
BOOL SeaOfThieves::PlayerESP = TRUE;
BOOL SeaOfThieves::ShipESP = TRUE;
BOOL SeaOfThieves::ItemESP = TRUE;
BOOL SeaOfThieves::DebugESP = FALSE;
BOOL SeaOfThieves::BarrelsESP = TRUE;
BOOL SeaOfThieves::DropPins = FALSE;

using namespace SDK;
using namespace std;

#define _DEBUG FALSE

typedef void(__thiscall* tPostRender)(UGameViewportClient* uObject, UCanvas* Canvas);
tPostRender OriginalPostRender;
const size_t PostRenderIndex = 88;

UAthenaGameViewportClient* AthenaGameViewportClient = nullptr;
FGuid localPlayerCrewId;

const uint8_t LEFT = 0;
const uint8_t CENTER = 1;
const uint8_t RIGHT = 2;

bool hooked = false;

uint8_t selectedCrew = 0;
UFont* perfCounterFont = nullptr;
UFont* robotoFont = nullptr;
UFont* specialFont = nullptr;
AShip* currentShip = nullptr;
FLinearColor white = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
FLinearColor black = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
FVector2D topLeft = FVector2D(0, 21);
uint32_t SeaOfThieves::logCount = 1;
uint32_t SeaOfThieves::logOffset = 21;
vector<const wchar_t*> directions = { L"North", L"North North East", L"North East", L"East North East", L"East", L"East South East", L"South East", L"South South East", L"South", L"South South West", L"South West", L"West South West", L"West",  L"West North West", L"North West", L"North North West" };

bool NullChecks()
{
	if (!AthenaGameViewportClient->GameInstance)
		return false;
	if (AthenaGameViewportClient->GameInstance->LocalPlayers.Num() < 1)
		return false;
	if (!AthenaGameViewportClient->GameInstance->LocalPlayers[0]->PlayerController)
		return false;
	if (!AthenaGameViewportClient->GameInstance->LocalPlayers[0]->PlayerController->Pawn)
		return false;
	if (!AthenaGameViewportClient->World)
		return false;
	if (!AthenaGameViewportClient->World->PersistentLevel)
		return false;
	return true;
}

FVector RotateCorner(FVector origin, FVector corner, float theta)
{
	float x = corner.X - origin.X;
	float y = corner.Y - origin.Y;

	return {
		origin.X + (x * UKismetMathLibrary::Cos(theta) - y * UKismetMathLibrary::Sin(theta)),
		origin.Y + (x * UKismetMathLibrary::Sin(theta) + y * UKismetMathLibrary::Cos(theta)),
		corner.Z
	};
}

FLinearColor Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
	return FLinearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

FLinearColor GetShipColor(float distance)
{
	float percentage = distance / 3600;
	if (percentage > 1.0f)
		percentage = 1.0f;

	if (percentage > 0.66f)
		return Color(0, 255, 0);
	if (percentage > 0.33f)
		return Color(255, 255, 0);
	return Color(255, 0, 0);
}

std::wstring FillSubstitutes(wstring pattern, TArray<FTreasureMapTextEntry> ss)
{
	for (int32_t i = 0; i < ss.Num(); i++)
	{
		if (ss.IsValidIndex(i) && ss[i].Name.c_str() && UKismetTextLibrary::Conv_TextToString(ss[i].Substitution).c_str())
		{
			wstring name = L"{" + wstring(ss[i].Name.c_str()) + L"}";
			wstring s = UKismetTextLibrary::Conv_TextToString(ss[i].Substitution).c_str();
			spdlog::info(string(name.begin(), name.end()) + " " + string(s.begin(), s.end()));
			int found = pattern.find(name);

			if (found != string::npos)
				pattern.replace(found, name.length(), s);
		}
	}

	return pattern;
}

void SeaOfThieves::GetFonts()
{
	while (!robotoFont)
	{
		robotoFont = UObject::FindObject<UFont>("Font Roboto.Roboto");
		spdlog::info("Roboto {:p}", (PVOID)robotoFont);
		if (!robotoFont)
			Sleep(5000);
	}
	while (!specialFont)
	{
		specialFont = UObject::FindObject<UFont>("Font Windlass.Windlass");
		spdlog::info("WindlassFont {:p}", (PVOID)specialFont);
		if (!specialFont)
			Sleep(5000);
	}
	//while (!perfCounterFont && GetLocalPlayer())
	//{
	//	perfCounterFont = UObject::FindObject<UFont>("Font PerfCounterFont.PerfCounterFont");
	//	spdlog::info("PerfCounterFont {:p}", (PVOID)perfCounterFont);
	//	if (!perfCounterFont)
	//		Sleep(5000);
	//}
}

bool SeaOfThieves::WorldToScreen(FVector location, FVector2D* screen)
{
	APlayerController* playerController = GetPlayerController();
	
	if (playerController)
	{
		return playerController->ProjectWorldLocationToScreen(location, screen);
	}

	return FALSE;
}

void SeaOfThieves::Initialise()
{
	auto tmpDir = string(getenv("localappdata")) + "\\Temp\\";
	spdlog::flush_on(spdlog::level::debug);
	spdlog::set_default_logger(spdlog::basic_logger_mt("SeaOfThots", tmpDir + "SeaOfThots", true));
#if _DEBUG == TRUE
	spdlog::set_level(spdlog::level::debug);
#endif

	// Log the initialisation
	spdlog::info("Initialising SeaOfThots");

	const auto currentProcess = GetCurrentProcess();
	const auto currentModule = GetModuleHandle(nullptr);

	MODULEINFO modInfo = { 0 };
	GetModuleInformation(currentProcess, currentModule, &modInfo, sizeof MODULEINFO);

	PBYTE start = (PBYTE)modInfo.lpBaseOfDll;
	PBYTE end = start + modInfo.SizeOfImage;

	if (start > end)
	{
		spdlog::info("moduleStart > moduleEnd; pattern scan will fail");
		return;
	}

	UpdateGNames(start, end, "48 8B 1D ? ? ? ? 48 85 ? 75 3A");
	UpdateGObjects(start, end, "89 0d ? ? ? ? 48 8b df 48 89 5c 24");

	AthenaGameViewportClient = UObject::FindObject<UAthenaGameViewportClient>("AthenaGameViewportClient Transient.AthenaGameEngine_1.AthenaGameViewportClient_1");
	spdlog::info("AthenaGameViewportClient {:p}", (PVOID)AthenaGameViewportClient);
	
	if (AthenaGameViewportClient == nullptr)
	{
		spdlog::error("AthenaGameViewportClient is nullptr");
		return;
	}

	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&GetFonts, NULL, NULL, NULL);

	auto lp = GetLocalPlayer();
	if (lp)
		if (lp->Controller)
			((AOnlineAthenaPlayerController*)lp->Controller)->IdleDisconnectEnabled = false;

	SeaOfThieves::HookRender();
}

void SeaOfThieves::UpdateGObjects(PBYTE start, PBYTE end, const char* pattern)
{
	const auto gObjectsAddress = Utilities::Memory::FindPattern(start, end, pattern);
	if (gObjectsAddress == NULL)
	{
		spdlog::info("GObjects Address is NULL");
		spdlog::info("start: {:p}", start);
		spdlog::info("end: {:p}", end);
		return;
	}

	auto offset = *reinterpret_cast<uint32_t*>(gObjectsAddress + 2);

	// Log our offset for our GObject
	UObject::GObjects = reinterpret_cast<decltype(UObject::GObjects)>(gObjectsAddress + 6 + offset);
	if (UObject::GObjects == nullptr)
	{
		spdlog::debug("FName::GNames is nullptr");
		return;
	}
	spdlog::info("GObjects Address: {:p}", (void*)UObject::GObjects);
	spdlog::info("GObjects.Num(): {}", UObject::GetGlobalObjects().Num());
}

void SeaOfThieves::UpdateGNames(PBYTE start, PBYTE end, const char* pattern)
{
	const auto gNamesAddress = Utilities::Memory::FindPattern(start, end, pattern);
	if (gNamesAddress == NULL)
	{
		spdlog::info("gNamesAddress is NULL");
		spdlog::info("start: {:p}", start);
		spdlog::info("end: {:p}", end);
		return;
	}

	auto offset = *reinterpret_cast<uint32_t*>(gNamesAddress + 3);
	auto finalAddress = reinterpret_cast<uint64_t*>(gNamesAddress + 7 + offset);

	// Log our offset for our GNames
	FName::GNames = reinterpret_cast<decltype(FName::GNames)>(*finalAddress);
	if (FName::GNames == nullptr)
	{
		spdlog::debug("FName::GNames is nullptr");
		return;
	}
	spdlog::info("GNames Address: {:p}", (void*)FName::GNames);
	spdlog::info("GNames.Num(): {}", SDK::FName::GetGlobalNames().Num());
}

void SeaOfThieves::HookRender()
{
	if (!hooked)
	{
		OriginalPostRender = (tPostRender)HookMethod((LPVOID)AthenaGameViewportClient, (PVOID)SeaOfThieves::Render, PostRenderIndex * sizeof uintptr_t);

		spdlog::info("PostRender {:p}", (PVOID)(AthenaGameViewportClient + (PostRenderIndex * sizeof(uintptr_t))));
		spdlog::info("Successfully hooked PostRender");
		hooked = true;
	}
}

void SeaOfThieves::UnHookRender()
{
	if (hooked) 
	{
		HookMethod((LPVOID)AthenaGameViewportClient, (PVOID)OriginalPostRender, PostRenderIndex * sizeof uintptr_t);

		spdlog::info("Unhooked PostRender");
		hooked = false;
	}
}

void SeaOfThieves::RenderText(AHUD* hud, wstring text, FVector2D location, uint8_t alignment = CENTER, FLinearColor color = white, bool outlined = true, UFont* font = specialFont, float scale = 0.4)
{
	FVector2D textSize = hud->Canvas->K2_TextSize(font, text.c_str(), FVector2D(scale, scale));
	switch (alignment)
	{
	case CENTER:
		location.X -= textSize.X / 2;
		location.Y -= textSize.Y / 2;
		break;
	case RIGHT:
		location.X -= textSize.X;
		break;
	}

	if (outlined)
	{
		// top-left
		hud->DrawText(text.c_str(), black, location.X - 1, location.Y - 1, font, scale, false);
		// top
		hud->DrawText(text.c_str(), black, location.X, location.Y - 1, font, scale, false);
		// top-right
		hud->DrawText(text.c_str(), black, location.X + 1, location.Y - 1, font, scale, false);
		// right
		hud->DrawText(text.c_str(), black, location.X + 1, location.Y, font, scale, false);
		// bottom-right
		hud->DrawText(text.c_str(), black, location.X + 1, location.Y + 1, font, scale, false);
		// bottom
		hud->DrawText(text.c_str(), black, location.X, location.Y + 1, font, scale, false);
		// bottom-left
		hud->DrawText(text.c_str(), black, location.X - 1, location.Y + 1, font, scale, false);
		// left
		hud->DrawText(text.c_str(), black, location.X - 1, location.Y, font, scale, false);
	}

	hud->DrawText(text.c_str(), color, location.X, location.Y, font, scale, false);
}

void SeaOfThieves::RenderText(AHUD* hud, string text, FVector2D location, uint8_t alignment = CENTER, FLinearColor color = white, bool outlined = true, UFont* font = specialFont, float scale = 0.4)
{
	RenderText(hud, wstring(text.begin(), text.end()), location, alignment, color, outlined, font, scale);
}

void SeaOfThieves::Render(UGameViewportClient* thisPointer, UCanvas* canvas)
{
#if _DEBUG == TRUE
	spdlog::debug("Started Render");
#endif
	if (NullChecks())
	{
#if _DEBUG == TRUE
		spdlog::debug("NULL Checks passed");
#endif
		APlayerController* playerController = GetPlayerController();
		ULevel* persistentLevel = AthenaGameViewportClient->World->PersistentLevel;
#if _DEBUG == TRUE
		spdlog::debug("Got PlayerController & PersistentLevel");
#endif

		if (playerController)
		{
			AHUD* hud = playerController->MyHUD;
			if (!hud)
				return;
			hud->Canvas = canvas;
			//menu->SetFont(perfCounterFont); 

#if _DEBUG == TRUE
			spdlog::debug("DrawPlayerList");
#endif
			DrawPlayerList(hud);
#if _DEBUG == TRUE
			spdlog::debug("DrawCompass");
#endif
			DrawCompass(hud);
#if _DEBUG == TRUE
			spdlog::debug("DrawCrosshair");
#endif
			DrawCrosshair(hud);

			TArray<ULevel*> levels = GetLevels();

			for (int32_t j = 6; j < levels.Num(); j++)
			{
				if (!levels[j])
					continue;

				TArray<AActor*> AActors = levels[j]->AActors;
				
				for (int32_t i = 0; i < AActors.Num(); i++)
				{
					AActor* actor = AActors[i];

					if (!actor)
						continue;

					if (actor->IsA(AStorageContainer::StaticClass()))
					{
#if _DEBUG == TRUE
						spdlog::debug("DrawBarrel");
#endif
						DrawBarrel(hud, actor);
						continue;
					}

					if (actor->IsA(ACookingPot::StaticClass()))
					{
#if _DEBUG == TRUE
						spdlog::debug("DrawCookingPot");
#endif
						DrawCookingPot(hud, actor);
						continue;
					}

					if (actor->IsA(AAmmoChest::StaticClass()))
					{
#if _DEBUG == TRUE
						spdlog::debug("DrawAmmoChest");
#endif
						DrawAmmoChest(hud, actor);
						continue;
					}

					if (actor->IsA(ALandmark::StaticClass()))
					{
#if _DEBUG == TRUE
						spdlog::debug("DrawLandmark");
#endif
						DrawLandmark(hud, actor);
						continue;
					}

					if (SeaOfThieves::DebugESP)
						DrawDebug(hud, actor);
				}
			}

			for (int32_t i = 0; i < persistentLevel->AActors.Num(); i++)
			{
				// Create a reference
				AActor* actor = persistentLevel->AActors[i];
				// Verify it's not null
				if (!actor)
					continue;

				// See if it's a player
				if (actor->IsA(AAthenaPlayerCharacter::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawPlayer");
#endif
					DrawPlayer(hud, actor);
					continue;
				}

				if (actor->IsA(ACollectorsChestItemProxy::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawCollectorsChest");
#endif
					DrawCollectorsChest(hud, actor);
					continue;
				}

				// Check if it's BootyItemInfo
				if (actor->IsA(ABootyProxy::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawItem");
#endif
					DrawItem(hud, actor);
					continue;
				}

				// Check if it's a ship
				if (actor->IsA(AShip::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawShip");
#endif
					DrawShip(hud, actor);
					continue;
				}

				if (actor->IsA(AShipNetProxy::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawShipFar");
#endif
					DrawShipFar(hud, actor);
					continue;
				}

				if (actor->IsA(AMapTable::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawMapPins");
#endif
					DrawMapPins(hud, actor);
					continue;
				}

				if (actor->IsA(AAthenaAICharacter::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawSkeleton");
#endif
					DrawSkeleton(hud, actor);
					continue;
				}

				if (actor->IsA(AStrongholdKey::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawStrongholdKey");
#endif
					DrawStrongholdKey(hud, actor);
					continue;
				}

				if (actor->IsA(AAmmoChest::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawAmmoChest");
#endif
					DrawAmmoChest(hud, actor);
					continue;
				}

				if (actor->IsA(ACookingPot::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawCookingPot");
#endif
					DrawCookingPot(hud, actor);
					continue;
				}

				if (actor->IsA(AStorageContainer::StaticClass()))
				{
#if _DEBUG == TRUE
					spdlog::debug("DrawBarrel");
#endif
					DrawBarrel(hud, actor);
					continue;
				}

				if (SeaOfThieves::DebugESP)
					DrawDebug(hud, actor);
			}

#if _DEBUG == TRUE
			spdlog::debug("DrawSelf");
#endif
			DrawSelf(hud);

			if (playerController->WasInputKeyJustReleased(FKey("F1")))
				SeaOfThieves::ItemESP = !SeaOfThieves::ItemESP;
			
			if (playerController->WasInputKeyJustReleased(FKey("F3")))
				SeaOfThieves::DropPins = !SeaOfThieves::DropPins;

			if (playerController->WasInputKeyJustReleased(FKey("F2")))
				SeaOfThieves::BarrelsESP = !SeaOfThieves::BarrelsESP;

			/*if (playerController->WasInputKeyJustReleased(FKey("End")))
			{
				SeaOfThieves::UnHookRender();
				HMODULE hMod = GetModuleHandle(L"SeaOfThots.dll");
				if (hMod) FreeLibrary(hMod);
			}*/
		
			currentShip = nullptr;
		}
	}

	SeaOfThieves::logCount = 0;
	OriginalPostRender(thisPointer, canvas);
}

void SeaOfThieves::DrawItem(AHUD* hud, AActor* item)
{
	if (SeaOfThieves::ItemESP)
	{
		auto type = item->GetName();
		if (type.find("Collector") != string::npos)
			return;

		auto localPlayer = AthenaGameViewportClient->GameInstance->LocalPlayers[0];
		if (!localPlayer)
			return;
		auto distance = localPlayer->PlayerController->Pawn->GetDistanceTo((AActor*)item) * 0.01f;

		// Get the location
		auto location = item->RootComponent->K2_GetComponentLocation();

		// Slightly raise it so we can see it
		location.Z += 100;

		auto itemInfo = ((ABootyItemInfo*)((ABootyProxy*)item)->ItemInfo);
		auto rarity = itemInfo->Rarity;

		wstring itemName = UKismetTextLibrary::Conv_TextToString(itemInfo->Desc->Title).c_str();

		if (type.find("Rome") != string::npos)
		{
			itemName = L"Sea Dogs Chest";
		}

		FLinearColor common = Color(128, 128, 128); // grey
		FLinearColor rare = Color(173, 255, 47); // greenyellow
		FLinearColor legendary = Color(255, 0, 255); // magenta
		FLinearColor mythical = Color(255, 165, 0); // orange
		FLinearColor special = Color(255, 255, 0); // turquoise

		FLinearColor color = common;

		if (rarity == "Common")
			color = white;
		else if (rarity == "Rare")
			color = rare;
		else if (rarity == "Legendary")
			color = legendary;
		else if (rarity == "Mythical")
			color = mythical;

		if (type.find("Fort") != string::npos
			|| type.find("BigGunpowderBarrel") != string::npos
			|| type.find("Ashen") != string::npos
			|| type.find("Drunken") != string::npos
			|| type.find("Weeping") != string::npos
			|| type.find("Rome") != string::npos)
			color = special;

		FVector2D screen;

		if (WorldToScreen(location, &screen))
			RenderText(hud, wstring(itemName + L" " + to_wstring((int)distance) + L"m ").c_str(), screen, true, color);
	}
}

void SeaOfThieves::Log(AHUD* hud, wstring text, FLinearColor color)
{
	FVector2D location(0, 21.0f + (SeaOfThieves::logCount++ * SeaOfThieves::logOffset));
	RenderText(hud, text, location, LEFT, color, true, robotoFont, 1);
}

void SeaOfThieves::Log(AHUD* hud, std::string text, FLinearColor color)
{
	Log(hud, wstring(text.begin(), text.end()), color);
}

void SeaOfThieves::DrawSelf(AHUD* hud)
{
	auto localPlayer = GetLocalPlayer();
	if (!localPlayer)
		return;

	if (localPlayerCrewId == FGuid())
		return;

	FVector2D start(hud->Canvas->SizeX - 450.0f, 100.0f);
	FVector2D valueStart(hud->Canvas->SizeX - 325.0f, 100.0f);

#if _DEBUG == TRUE
	spdlog::debug("localPlayer->GetCurrentShip()");
#endif
	AShip* ship = (AShip*)localPlayer->GetCurrentShip();

	if (ship)
	{
		FVector v = ship->GetVelocity() * 0.01f;
		string vs = Utilities::string_format("%.2fm/s", UKismetMathLibrary::Sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z));
		RenderText(hud, L"Ship Speed: ", start, RIGHT, white, true, perfCounterFont, 1);
		RenderText(hud, vs, valueStart, LEFT, white, true, perfCounterFont, 1);
		start.Y += 25;
		valueStart.Y += 25;
	}

#if _DEBUG == TRUE
	spdlog::debug("localPlayer->GetAttachParentActor()");
#endif
	AActor* parent = localPlayer->GetAttachParentActor();
	if (parent)
	{
		RenderText(hud, L"Attached to: ", start, RIGHT, white, true, perfCounterFont, 1);
		if (parent->IsA(ACannon::StaticClass()))
			RenderText(hud, L"Cannon", valueStart, LEFT, white, true, perfCounterFont, 1);
		if (parent->IsA(AHarpoonLauncher::StaticClass()))
			RenderText(hud, L"Harpoon", valueStart, LEFT, white, true, perfCounterFont, 1);
		if (parent->IsA(AWheel::StaticClass()))
			RenderText(hud, L"Wheel", valueStart, LEFT, white, true, perfCounterFont, 1);
		if (parent->IsA(ALadder::StaticClass()))
			RenderText(hud, L"Ladder", valueStart, LEFT, white, true, perfCounterFont, 1);
		if (parent->IsA(ASailHoist::StaticClass()))
			RenderText(hud, L"Sail Hoist", valueStart, LEFT, white, true, perfCounterFont, 1);
		if (parent->IsA(ASailAngle::StaticClass()))
			RenderText(hud, L"Sail Angler", valueStart, LEFT, white, true, perfCounterFont, 1);
		if (parent->IsA(ACapstanArm::StaticClass()))
		{			
			RenderText(hud, L"Anchor", valueStart, LEFT, white, true, perfCounterFont, 1);
		}

		start.Y += 25;
		valueStart.Y += 25;
	}

#if _DEBUG == TRUE
	spdlog::debug("localPlayer->DrowningComponent");
#endif
	if (localPlayer->DrowningComponent)
	{
		RenderText(hud, L"Oxygen: ", start, RIGHT, white, true, perfCounterFont, 1);
		RenderText(hud, to_wstring((int)(localPlayer->DrowningComponent->OxygenLevel * 100)), valueStart, LEFT, white, true, perfCounterFont, 1);
		start.Y += 25;
		valueStart.Y += 25;
	}

#if _DEBUG == TRUE
	spdlog::debug("localPlayer->WieldedItemComponent");
#endif
	if (localPlayer->WieldedItemComponent)
	{
#if _DEBUG == TRUE
		spdlog::debug("WieldedItemComponent not null");
#endif)
		if (localPlayer->WieldedItemComponent->CurrentlyWieldedItem)
		{
#if _DEBUG == TRUE
			spdlog::debug("CurrentlyWieldedItem not null");
#endif)
			AWieldableItem* item = (AWieldableItem*)localPlayer->WieldedItemComponent->CurrentlyWieldedItem;
			if (item->ItemInfo && item->ItemInfo->Desc)
			{
				wstring name = UKismetTextLibrary::Conv_TextToString(item->ItemInfo->Desc->Title).c_str();
				RenderText(hud, L"Holding: ", start, RIGHT, white, true, perfCounterFont, 1);
				RenderText(hud, name, valueStart, RIGHT, white, true, perfCounterFont, 1);
				start.Y += 25;
				valueStart.Y += 25;
			}
		}
	}
}

void SeaOfThieves::DrawPlayerList(AHUD* hud)
{

}

void SeaOfThieves::DrawPlayer(AHUD* hud, AActor* actor)
{
	// NULL checks
	if (actor == nullptr)
		return;
	auto player = (AAthenaPlayerCharacter*)actor;
	if (!player)
		return;
	if (player->PlayerState == nullptr)
		return;
	AAthenaPlayerCharacter* localPlayer = GetLocalPlayer();
	if (localPlayer == nullptr)
		return;

	bool enemy = !UCrewFunctions::AreCharactersInSameCrew(localPlayer, player);
	wstring localPlayerName = wstring(localPlayer->PlayerState->PlayerName.c_str());
	FLinearColor color(enemy ? 1.0f : 0.0f, enemy ? 0.0f : 1.0f, 0.0f);
	FVector2D screen;
	FVector location = player->K2_GetActorLocation();
	int health = (int)(player->HealthComponent->GetCurrentHealth() / player->HealthComponent->MaxHealth * 100);
	double distance = (localPlayer->GetDistanceTo(player) * 0.01);
	FVector nameLocation = location;
	nameLocation.Z += 100;

	wstring playerName(player->PlayerState->PlayerName.c_str());
	if (playerName == localPlayerName)
		return;

	FRotator currentRotation = localPlayer->Controller->ControlRotation;
	FRotator lookAtRotation = UKismetMathLibrary::FindLookAtRotation(localPlayer->K2_GetActorLocation(), FVector(location.X, location.Y, location.Z - 35));
	lookAtRotation.Roll = 0;
	FRotator rotationOffset = lookAtRotation - currentRotation;
	rotationOffset.Roll = 0;
	FVector v = player->GetVelocity();

	if (GetPlayerController()->IsInputKeyDown(FKey("C"))) {
		FHitResult ignore;
		localPlayer->Controller->ControlRotation = lookAtRotation;
	}

	FVector2D healthLocationScreen;
	FVector healthLocation = location;
	healthLocation.Z -= 50.0f;
	if (WorldToScreen(location, &screen)
		&& WorldToScreen(healthLocation, &healthLocationScreen))
	{
		int height = (screen.Y - healthLocationScreen.Y) * 2;
		int width = height * 0.75;
		DrawHealthBar(hud, player->HealthComponent, healthLocation, FVector2D(width, height * 0.1));
	}

	DrawBoundingBox(hud, player, color);

	// Draw the name
	if (WorldToScreen(nameLocation, &screen))
		RenderText(hud, wstring(playerName + L" " + to_wstring((int)distance) + L"m " + to_wstring(health) + L"%").c_str(), screen, true, color);
	SeaOfThieves::Log(hud, wstring(playerName + L" [ " + to_wstring((int)distance) + L"m ]").c_str(), color);
}

void SeaOfThieves::DrawShip(AHUD* hud, AActor* actor)
{
	// NULL checks
	if (actor == nullptr)
		return;

	AAthenaPlayerCharacter* localPlayer = GetLocalPlayer();
	if (localPlayer == nullptr)
		return;
	AShip* ship = (AShip*)actor;
	FVector location = ship->RootComponent->K2_GetComponentLocation();
	// Add 25 meters vertically
	location.Z += 25 * 100;
	float distance = localPlayer->GetDistanceTo(actor) * 0.01f;

	if (distance >= 1300)
		return;

	float waterMax = ship->GetInternalWater()->InternalWaterParams.MaxWaterAmount;
	float waterAmount = ship->GetInternalWater()->GetWaterAmount();
	FGuid crewId = ship->CrewOwnershipComponent->CachedCrewId;

	if (crewId == localPlayerCrewId)
	{
		currentShip = ship;
		return;
	}
	
	string name = ship->GetName();
	auto damageZones = ship->GetHullDamage()->ActiveHullDamageZones;
	wstring shipType;
	if (name.find("Large") != string::npos)
		shipType = L"Galleon";
	else if (name.find("Medium") != string::npos)
		shipType = L"Brigantine";
	else if (name.find("Small") != string::npos)
		shipType = L"Sloop";

	if (name.find("AI") != string::npos)
		shipType = L"Skeleton " + shipType;

	FLinearColor color = GetShipColor(distance);

	if (name.find("azure") != string::npos)
		color = Color(0, 255, 255);
	else if (name.find("regal") != string::npos)
		color = Color(255, 0, 255);
	else if (name.find("lucky") != string::npos)
		color = Color(0, 255, 0);
	else if (name.find("flaming") != string::npos)
		color = Color(255, 0, 0);
	else if (name.find("golden") != string::npos)
		color = Color(255, 255, 0);
	else if (name.find("boy") != string::npos)
		color = white;

	wstring text = shipType + L" " + to_wstring((int)distance) + L"m (" + to_wstring((int)(waterAmount / waterMax * 10000) / 100) + L"%) (" + to_wstring(damageZones.Num()) + L" holes)";

	FVector2D screen;

	if (WorldToScreen(location, &screen))
		RenderText(hud, text.c_str(), screen, true, color);

	for (uint32_t i = 0; i < damageZones.Num(); i++) {
		auto zone = damageZones[i];

		auto zoneLocation = zone->RootComponent->K2_GetComponentLocation();
		auto color = white;
		if (zone->IsUnderInternalWater)
			color = Color(255, 0, 0);
		if (WorldToScreen(zoneLocation, &screen))
			RenderText(hud, L"X", screen, CENTER, color, true);
	}
	Log(hud, text.c_str(), color);
}

void SeaOfThieves::DrawShipFar(AHUD* hud, AActor* actor)
{
	AAthenaPlayerCharacter* localPlayer = GetLocalPlayer();
	if (!localPlayer)
		return;

	AShipNetProxy* ship = (AShipNetProxy*)actor;
	FVector location = ship->RootComponent->K2_GetComponentLocation();
	// Add 25 meters vertically
	location.Z += 25 * 100;
	float distance = localPlayer->GetDistanceTo(actor) * 0.01f;

	if (distance < 1300)
		return;

	string name = actor->GetName();

	wstring shipType;
	if (name.find("Large") != string::npos)
		shipType = L"Galleon";
	else if (name.find("Medium") != string::npos)
		shipType = L"Brigantine";
	else if (name.find("Small") != string::npos)
		shipType = L"Sloop";

	if (name.find("AI") != string::npos)
		shipType = L"Skeleton " + shipType;

	FVector2D screen;

	if (WorldToScreen(location, &screen))
		RenderText(hud, wstring(shipType + L" " + to_wstring((int)distance) + L"m").c_str(), screen, true, GetShipColor(distance));
}

void SeaOfThieves::DrawCrosshair(AHUD* hud)
{
	const float centerX = hud->Canvas->SizeX / 2;
	const float centerY = hud->Canvas->SizeY / 2;
	// Vertical Line
	hud->Canvas->K2_DrawLine(FVector2D(centerX, centerY - 5), FVector2D(centerX, centerY + 5), 2, white);
	// Horizontal Line
	hud->Canvas->K2_DrawLine(FVector2D(centerX - 5, centerY), FVector2D(centerX + 5, centerY), 2, white);
}

void SeaOfThieves::DrawCompass(AHUD* hud)
{
	auto localPlayer = GetLocalPlayer();
	if (!localPlayer)
		return;

	auto pc = GetPlayerController();
	if (pc)
	{
		auto cameraManager = GetPlayerController()->PlayerCameraManager;

		if (cameraManager)
		{
			FRotator rotation = cameraManager->GetCameraRotation();

			int yaw = ((int)rotation.Yaw + 450) % 360;
			wstring compassBearing;
			const float offset = 11.25;
			const float centerX = (float)hud->Canvas->SizeX / 2;
			const int index = round((int)((int)(yaw + 11.25) % 360 * 0.04444444444) % 16);

			RenderText(hud, to_wstring(yaw).c_str(), FVector2D(centerX, 25), CENTER, white, true, specialFont, 0.55);
			RenderText(hud, directions[index], FVector2D(centerX, 50), CENTER, white, true, specialFont, 0.55);
		}
	}
}

void SeaOfThieves::DrawSkeleton(AHUD* hud, AActor* actor)
{
	auto localPlayer = GetLocalPlayer();
	if (!localPlayer)
		return;

	AAthenaAICharacter* skeleton = (AAthenaAICharacter*)actor;

	FVector location = actor->RootComponent->K2_GetComponentLocation();
	double distance = (localPlayer->GetDistanceTo(actor) * 0.01);
	FVector nameLocation = location;
	nameLocation.Z += 100;

	wstring skeleName = L"Skeleton";

	if (skeleton->AssignedMesh)
	{
		string meshName = skeleton->AssignedMesh->GetName();
		if (meshName.find("nme_skellyshadow") != string::npos)
		{
			skeleName = L"Shadow " + skeleName;
			if (skeleton->TeamColorTexture)
			{
				string skeleColor = skeleton->TeamColorTexture->GetName();
				if (skeleColor.find("venom") != string::npos)
					skeleName = L"Purple " + skeleName;
				else if (skeleColor.find("shark") != string::npos)
					skeleName = L"Blue " + skeleName;
				else if (skeleColor.find("volcano") != string::npos)
					skeleName = L"Red " + skeleName;
				else if (skeleColor.find("lightning") != string::npos)
					skeleName = L"White " + skeleName;
				else if (skeleColor.find("player") != string::npos)
					skeleName = L"Pink " + skeleName;
				else if (skeleColor.find("skeleton") != string::npos)
					skeleName = L"Green " + skeleName;
				else skeleName = wstring(skeleColor.begin(), skeleColor.end());
			}
		}
		else if (meshName.find("nme_skelly_gen") != string::npos) {}
		else
			skeleName = wstring(meshName.begin(), meshName.end());
	}

	if (skeleton->AINameplate && skeleton->AINameplate->DisplayName.Num() != 0)
		skeleName = skeleton->AINameplate->DisplayName.c_str();


	// Draw the name
	FVector2D screen;
	if (WorldToScreen(nameLocation, &screen))
		RenderText(hud, wstring(skeleName + L" " + to_wstring((int)distance) + L"m").c_str(), screen, true, Color(140, 0, 180));
}

void SeaOfThieves::DrawMapPins(AHUD* hud, AActor* actor)
{
	auto player = GetLocalPlayer();
	if (!player)
		return;
	auto table = (AMapTable*)actor;

	if (table)
	{
		auto pins = table->MapPins;

		for (uint32_t i = 0; i < pins.Num(); i++)
		{
			auto pin = pins[i];
			FVector loc(pin.X * 100, pin.Y * 100, SeaOfThieves::DropPins ? player->K2_GetActorLocation().Z : 10000.f);
			float distance = UVectorMaths::Distance(player->RootComponent->K2_GetComponentLocation(), loc) * 0.01f;
			FVector2D screen;
			if (WorldToScreen(loc, &screen))
				RenderText(hud, wstring(L"Map Pin " + to_wstring((int)distance) + L"m").c_str(), screen, CENTER, white, true);
		}
	}
}

void SeaOfThieves::DrawStrongholdKey(AHUD* hud, AActor* actor)
{
	auto localPlayer = GetLocalPlayer();
	if (!localPlayer)
		return;
	auto distance = localPlayer->GetDistanceTo(actor) * 0.01f;
	AStrongholdKey* key = (AStrongholdKey*)actor;

	// Get the location
	auto location = actor->RootComponent->K2_GetComponentLocation();

	// Slightly raise it so we can see it
	location.Z += 100;

	auto itemInfo = ((AStrongholdKeyItemInfo*)(key)->ItemInfo);

	wstring itemName = UKismetTextLibrary::Conv_TextToString(itemInfo->Desc->Title).c_str();

	FLinearColor color = Color(255, 255, 0);

	FVector2D screen;
	if (WorldToScreen(location, &screen))
		RenderText(hud, wstring(itemName + L" " + to_wstring((int)distance) + L"m ").c_str(), screen, CENTER, color);
}

void SeaOfThieves::DrawAmmoChest(AHUD* hud, AActor* actor)
{
	auto localPlayer = GetLocalPlayer();
	if (!localPlayer)
		return;
	auto distance = localPlayer->GetDistanceTo(actor) * 0.01f;
	AAmmoChest* chest = (AAmmoChest*)actor;

	// Get the location
	auto location = actor->RootComponent->K2_GetComponentLocation();

	// Slightly raise it so we can see it
	location.Z += 100;

	if (distance > 150.0)
		return;

	wstring itemName = L"Ammo Chest";

	FLinearColor color = Color(255, 255, 255);

	FVector2D screen;
	if (WorldToScreen(location, &screen))
		RenderText(hud, wstring(itemName + L" " + to_wstring((int)distance) + L"m ").c_str(), screen, CENTER, color);
}

void SeaOfThieves::DrawCookingPot(AHUD* hud, AActor* actor)
{
	auto localPlayer = GetLocalPlayer();
	if (!localPlayer)
		return;

	ACookingPot* pot = (ACookingPot*)actor;

	FVector location = actor->RootComponent->K2_GetComponentLocation();
	double distance = (localPlayer->GetDistanceTo(actor) * 0.01);
	FVector nameLocation = location;
	nameLocation.Z += 100;
	FVector stateLocation = location;
	stateLocation.Z += 50;

	if (distance > 250.0)
		return;

	FVector2D screen;

	if (pot->CookerComponent && pot->CookerComponent->CookingState.Cooking)
	{
		string itemName = string(pot->CookerComponent->CookingState.CurrentCookableTypeName.GetName());
		wstring itemNameW = wstring(itemName.begin(), itemName.end());
		wstring state;

		switch (pot->CookerComponent->CookingState.SmokeFeedbackLevel)
		{
		case ECookingSmokeFeedbackLevel::ECookingSmokeFeedbackLevel__NotCooking:
			state = L"Not Cooking";
			break;
		case ECookingSmokeFeedbackLevel::ECookingSmokeFeedbackLevel__Raw:
			state = L"Raw";
			break;
		case ECookingSmokeFeedbackLevel::ECookingSmokeFeedbackLevel__CookedWarning:
			state = L"Almost Cooked";
			break;
		case ECookingSmokeFeedbackLevel::ECookingSmokeFeedbackLevel__Cooked:
			state = L"Cooked";
			break;
		case ECookingSmokeFeedbackLevel::ECookingSmokeFeedbackLevel__BurnedWarning:
			state = L"Burning";
			break;
		case ECookingSmokeFeedbackLevel::ECookingSmokeFeedbackLevel__Burned:
			state = L"Burned";
			break;
		case ECookingSmokeFeedbackLevel::ECookingSmokeFeedbackLevel__ECookingSmokeFeedbackLevel_MAX:
			state = L"Fucked";
			break;
		}
		if (WorldToScreen(stateLocation, &screen))
			RenderText(hud, wstring(itemNameW + L" " + state).c_str(), screen, true);

		if (pot->CookerComponent->CookingState.SmokeFeedbackLevel >= ECookingSmokeFeedbackLevel::ECookingSmokeFeedbackLevel__Cooked)
			RenderText(hud, wstring(itemNameW + L" " + state).c_str(), FVector2D(hud->Canvas->SizeX / 2, 150), CENTER, Color(255, 0, 0), TRUE, specialFont, 1);
	}
}

void SeaOfThieves::DrawCollectorsChest(AHUD* hud, AActor* actor)
{
	if (SeaOfThieves::ItemESP)
	{
		ACollectorsChestItemProxy* collectors = (ACollectorsChestItemProxy*)actor;
		int distance = (int)GetLocalPlayer()->GetDistanceTo(collectors) * 0.01f;
		FVector location = collectors->K2_GetActorLocation();

		FVector2D screen;

		if (WorldToScreen(location, &screen))
		{
			// Draw the chest name
			if (collectors->ItemInfo && collectors->ItemInfo->Desc)
			{
				wstring name = UKismetTextLibrary::Conv_TextToString(collectors->ItemInfo->Desc->Title).c_str();
				RenderText(hud, (name + L" " + to_wstring(distance) + L"m").c_str(), screen);
			}
			else {
				RenderText(hud, (L"Collector's Chest " + to_wstring(distance) + L"m").c_str(), screen);
			}

			if (collectors->ItemSlot1
				&& collectors->ItemSlot1->StoredItemInfo.ItemInfo
				&& collectors->ItemSlot1->StoredItemInfo.ItemInfo->Desc)
			{
				wstring itemName = UKismetTextLibrary::Conv_TextToString(collectors->ItemSlot1->StoredItemInfo.ItemInfo->Desc->Title).c_str();

				RenderText(hud, (L"- " + itemName).c_str(), FVector2D(screen.X + 15, screen.Y + 15), CENTER, white, true, specialFont, 0.325);
			}

			if (collectors->ItemSlot2
				&& collectors->ItemSlot2->StoredItemInfo.ItemInfo
				&& collectors->ItemSlot2->StoredItemInfo.ItemInfo->Desc)
			{
				wstring itemName = UKismetTextLibrary::Conv_TextToString(collectors->ItemSlot2->StoredItemInfo.ItemInfo->Desc->Title).c_str();

				RenderText(hud, (L"- " + itemName).c_str(), FVector2D(screen.X + 15, screen.Y + 15 * 2), CENTER, white, true, specialFont, 0.325);
			}

			if (collectors->ItemSlot3
				&& collectors->ItemSlot3->StoredItemInfo.ItemInfo
				&& collectors->ItemSlot3->StoredItemInfo.ItemInfo->Desc)
			{
				wstring itemName = UKismetTextLibrary::Conv_TextToString(collectors->ItemSlot3->StoredItemInfo.ItemInfo->Desc->Title).c_str();

				RenderText(hud, (L"- " + itemName).c_str(), FVector2D(screen.X + 15, screen.Y + 15 * 3), CENTER, white, true, specialFont, 0.325);
			}
		}
	}
}

void SeaOfThieves::DrawShipwreck(AHUD* hud, AActor* actor)
{

}

void SeaOfThieves::DrawMermaid(AHUD* hud, AActor* actor)
{

}

void SeaOfThieves::DrawStorm(AHUD* hud, AActor* actor)
{
}

void SeaOfThieves::DrawBoundingBox(AHUD* hud, AActor* actor, FLinearColor color)
{
	UWorld* world = AthenaGameViewportClient->World;
	if (!world)
		return;

	FVector origin, extent;
	actor->GetActorBounds(true, &origin, &extent);
	FRotator rotation = actor->K2_GetActorRotation();
	float yaw = UKismetMathLibrary::DegreesToRadians((int)(rotation.Yaw + 450.0f) % 360);

	//UKismetSystemLibrary::DrawDebugBox(actor, origin, extent, color, actor->RootComponent->K2_GetComponentRotation(), 0.0f);
	extent.Z = extent.Z;
	FVector t1 = origin, t2 = origin, t3 = origin, t4 = origin, b1 = origin, b2 = origin, b3 = origin, b4 = origin;
	t1.X -= 35.f;
	t1.Y -= 35.f;
	t1.Z -= extent.Z;
	t2.X += 35.f;
	t2.Y -= 35.f;
	t2.Z -= extent.Z;
	t3.X += 35.f;
	t3.Y += 35.f;
	t3.Z -= extent.Z;
	t4.X -= 35.f;
	t4.Y += 35.f;
	t4.Z -= extent.Z;

	t1 = RotateCorner(origin, t1, yaw);
	t2 = RotateCorner(origin, t2, yaw);
	t3 = RotateCorner(origin, t3, yaw);
	t4 = RotateCorner(origin, t4, yaw);

	FVector2D ts1, ts2, ts3, ts4;
	if (!WorldToScreen(t1, &ts1)
		|| !WorldToScreen(t2, &ts2)
		|| !WorldToScreen(t3, &ts3)
		|| !WorldToScreen(t4, &ts4))
	{
		return;
	}

	b1.X -= 35.f;
	b1.Y -= 35.f;
	b1.Z += extent.Z;
	b2.X += 35.f;
	b2.Y -= 35.f;
	b2.Z += extent.Z;
	b3.X += 35.f;
	b3.Y += 35.f;
	b3.Z += extent.Z;
	b4.X -= 35.f;
	b4.Y += 35.f;
	b4.Z += extent.Z;

	b1 = RotateCorner(origin, b1, yaw);
	b2 = RotateCorner(origin, b2, yaw);
	b3 = RotateCorner(origin, b3, yaw);
	b4 = RotateCorner(origin, b4, yaw);

	FVector2D bs1, bs2, bs3, bs4;
	if (!WorldToScreen(b1, &bs1)
		|| !WorldToScreen(b2, &bs2)
		|| !WorldToScreen(b3, &bs3)
		|| !WorldToScreen(b4, &bs4))
	{
		return;
	}

	hud->Canvas->K2_DrawLine(ts1, ts2, 2.f, color);
	hud->Canvas->K2_DrawLine(ts2, ts3, 2.f, color);
	hud->Canvas->K2_DrawLine(ts3, ts4, 2.f, color);
	hud->Canvas->K2_DrawLine(ts4, ts1, 2.f, color);
	hud->Canvas->K2_DrawLine(bs1, bs2, 2.f, color);
	hud->Canvas->K2_DrawLine(bs2, bs3, 2.f, color);
	hud->Canvas->K2_DrawLine(bs3, bs4, 2.f, color);
	hud->Canvas->K2_DrawLine(bs4, bs1, 2.f, color);

	if (GetPlayerController() && !GetPlayerController()->IsInputKeyDown(FKey("F8")))
	{
		hud->Canvas->K2_DrawLine(ts1, bs1, 2.f, color);
		hud->Canvas->K2_DrawLine(ts2, bs2, 2.f, color);
		hud->Canvas->K2_DrawLine(ts3, bs3, 2.f, color);
		hud->Canvas->K2_DrawLine(ts4, bs4, 2.f, color);
	}
}

void SeaOfThieves::DrawHealthBar(AHUD* hud, UHealthComponent* healthComponent, FVector position, FVector2D size)
{
	// Get the current health as a percentage between 
	int health = (int)(healthComponent->GetCurrentHealth() / healthComponent->MaxHealth * 100);

	// Get the 2D coords
	FVector2D screen;
	if (WorldToScreen(position, &screen))
	{
		hud->DrawRect(black, screen.X - size.X / 2 - 1, screen.Y - size.X / 2 - 1, size.X + 2, size.Y + 2);
		hud->DrawRect(Color(0, 255, 0), screen.X - size.X / 2, screen.Y - size.X / 2, size.X / 100 * health, size.Y);
	}
}

void SeaOfThieves::DrawBarrel(AHUD* hud, AActor* actor)
{
	if (BarrelsESP)
	{
		FVector2D screen;
		FVector location = actor->K2_GetActorLocation();
		location.Z += 50.0f;
		if (WorldToScreen(location, &screen))
		{
			AStorageContainer* barrel = (AStorageContainer*)actor;
			if (barrel->StorageContainer)
			{
				TArray<FStorageContainerNode> cNodes = barrel->StorageContainer->ContainerNodes.ContainerNodes;
				for (int8_t k = 0; k < cNodes.Num(); k++)
				{
					FVector2D location(screen.X + 15, screen.Y + (k * 15));
					FStorageContainerNode node = cNodes[k];

					if (node.ItemDesc)
					{
						UItemDesc* itemDesc = node.ItemDesc->CreateDefaultObject<UItemDesc>();

						if (itemDesc)
						{
							wstring name = UKismetTextLibrary::Conv_TextToString(itemDesc->Title).c_str();
							string test(name.begin(), name.end());
							string value = Utilities::string_format("%2d %s", node.NumItems, test.c_str());
							RenderText(hud, wstring(value.begin(), value.end()), location, LEFT, white, true, robotoFont, 1);
						}
					}
				}
			}
		}
	}
}

void SeaOfThieves::DrawLandmark(AHUD* hud, AActor* actor)
{
	ALandmark* landmark = (ALandmark*)actor;
	wstring stringToDraw;
	if (LandmarkIsNeeded(hud, landmark, &stringToDraw))
	{
		FVector2D screen;
		if (WorldToScreen(actor->K2_GetActorLocation(), &screen))
		{
			RenderText(hud, stringToDraw, screen);
		}
	}
}

void SeaOfThieves::DrawDebug(AHUD* hud, AActor* actor)
{
	int distance = (int)(GetLocalPlayer()->GetDistanceTo(actor) * 0.01);
	FVector location = actor->K2_GetActorLocation();
	string name = actor->GetName();

	if (name.find("_") != string::npos)
		return;
	if (name.find("cmn") != string::npos)
		return;
	if (name.find("wsp") != string::npos)
		return;
	if (name.find("water") != string::npos)
		return;
	if (name.find("Light") != string::npos)
		return;
	if (name.find("vfx") != string::npos)
		return;
	if (name.find("ske") != string::npos)
		return;
	if (name.find("wld") != string::npos)
		return;
	if (name.find("dvr") != string::npos)
		return;
	if (name.find("ref") != string::npos)
		return;
	if (name.find("volume") != string::npos)
		return;
	if (name.find("Volume") != string::npos)
		return;
	if (name.find("rocks") != string::npos)
		return;
	if (name.find("jetty") != string::npos)
		return;
	if (name.find("shop") != string::npos)
		return;
	if (name.find("bp") != string::npos)
		return;
	if (name.find("bld") != string::npos)
		return;
	if (name.find("bsp") != string::npos)
		return;
	if (name.find("Nav") != string::npos)
		return;
	if (name.find("Water") != string::npos)
		return;
	if (name.find("FishCreature") != string::npos)
		return;

	name = actor->GetFullName();

	wstring nameW(name.begin(), name.end());
	FVector2D screen;

	if (WorldToScreen(location, &screen))
	{
		RenderText(hud, (nameW).c_str(), screen);
	}
}

bool SeaOfThieves::LandmarkIsNeeded(AHUD* hud, ALandmark* landmark, wstring* stringToDraw)
{
	AAthenaPlayerCharacter* localPlayer = GetLocalPlayer();

	if (localPlayer && landmark)
	{
		if (localPlayer->MapRadialInventoryComponent)
		{
			TArray<AActor*> maps = localPlayer->MapRadialInventoryComponent->RemappedRadialSlots;

			for (int32_t i = 0; i < maps.Num(); i++)
			{
				if (!maps[i])
					continue;

				if (maps[i]->IsA(ARiddleMap::StaticClass()))
				{
					ARiddleMap* riddleMap = (ARiddleMap*)maps[i];

					if (riddleMap->Contents.Progress != 0)
					{
						TArray<FTreasureMapTextEntry> substitutions = riddleMap->Contents.Text[riddleMap->Contents.Progress].Substitutions;
						for (int32_t j = 0; j < substitutions.Num(); j++)
						{
							FTreasureMapTextEntry sub = substitutions[j];
							if (wstring(sub.Name.c_str()) == L"location")
							{
								wstring pattern = UKismetTextLibrary::Conv_TextToString(sub.Substitution).c_str();
								wstring name = UKismetTextLibrary::Conv_TextToString(landmark->Name).c_str();
								if (pattern == name)
								{
									wstring full = UKismetTextLibrary::Conv_TextToString(riddleMap->Contents.Text[riddleMap->Contents.Progress].Pattern).c_str();
									if (full.find(L"lantern") != string::npos || full.find(L"light") != string::npos)
										* stringToDraw = L"Raise lantern";
									else if (full.find(L"song") != string::npos || full.find(L"shanty") != string::npos || full.find(L"tune") != string::npos)
										* stringToDraw = L"Play song";
									else if (full.find(L"map") != string::npos)
										* stringToDraw = L"Read map";
									else *stringToDraw = FillSubstitutes(full, riddleMap->Contents.Text[riddleMap->Contents.Progress].Substitutions);
									return true;
								}
							}
						}
					}
				}
			}
		}
	}
	return false;
}

ACrewService* SeaOfThieves::GetCrewService()
{
	if (AthenaGameViewportClient)
		if (AthenaGameViewportClient->World)
			if (AthenaGameViewportClient->World->GameState)
				return ((AAthenaGameState*)AthenaGameViewportClient->World->GameState)->CrewService;
	return nullptr;
}

AAthenaPlayerCharacter* SeaOfThieves::GetLocalPlayer()
{
	if (AthenaGameViewportClient)
		return (AAthenaPlayerCharacter*)AthenaGameViewportClient->GameInstance->LocalPlayers[0]->PlayerController->Pawn;
	return nullptr;
}

APlayerController* SeaOfThieves::GetPlayerController()
{
	if (AthenaGameViewportClient)
		return AthenaGameViewportClient->GameInstance->LocalPlayers[0]->PlayerController;
	return nullptr;
}

ULevel* SeaOfThieves::GetLevel()
{
	if (AthenaGameViewportClient)
		if (AthenaGameViewportClient->World)
			return AthenaGameViewportClient->World->PersistentLevel;
	return nullptr;
}

TArray<ULevel*> SeaOfThieves::GetLevels()
{
	if (AthenaGameViewportClient && AthenaGameViewportClient->World)
		return AthenaGameViewportClient->World->Levels;
	return TArray<ULevel*>();
}

/// GetActors gets the actors from the current level, but will
/// return an empty array when not possible, eg. player is in a menu
Result SeaOfThieves::GetActors(TArray<AActor*>* actors)
{
	if (!actors)
		return Result::NullPointer;

	if (!AthenaGameViewportClient)
		return Result::ViewportNull;

	if (!AthenaGameViewportClient->World)
		return Result::WorldNull;

	if (!AthenaGameViewportClient->World->PersistentLevel)
		return Result::LevelNull;

	if (!AthenaGameViewportClient->World->PersistentLevel->ActorCluster)
		return Result::ActorClusterNull;

	actors = &AthenaGameViewportClient->World->PersistentLevel->ActorCluster->Actors;
	return Result::Success;
}

void SeaOfThieves::SetModule(HMODULE hMod)
{
	SeaOfThieves::hMod = hMod;
}