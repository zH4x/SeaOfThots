#pragma once
#define DrawText DrawText
#include "utilities/utilities.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "../SDK/include/SDK.hpp"
#include "VMTHook.h"
#include "Result.h"
#include <string>
#include <cmath>
#include <vector>

using namespace SDK;

class SeaOfThieves
{
private:
	static HMODULE hMod;

	static BOOL PlayerESP;
	static BOOL ShipESP;
	static BOOL ItemESP;
	static BOOL BarrelsESP;
	static BOOL DebugESP;
	static BOOL DrawMenu;
	static BOOL DropPins;
	static uint32_t logCount;
	static uint32_t logOffset;

public:
	static void Initialise();
	static void SetModule(HMODULE hMod);
	static void UpdateGObjects(PBYTE, PBYTE, const char*);
	static void UpdateGNames(PBYTE, PBYTE, const char*);
	static void HookRender();
	static void UnHookRender();
	static void Render(UGameViewportClient* thisPointer, UCanvas* canvas);

	static ACrewService* GetCrewService();
	static ULevel* GetLevel();
	static TArray<ULevel*> GetLevels();
	static Result GetActors(TArray<AActor*> *actors);
	static AAthenaPlayerCharacter* GetLocalPlayer();
	static APlayerController* GetPlayerController();
	static bool WorldToScreen(FVector location, FVector2D* screen);

	static void DrawItem(AHUD* hud, AActor* item);
	static void DrawPlayerList(AHUD* hud);
	static void DrawPlayer(AHUD* hud, AActor* player);
	static void DrawShip(AHUD* hud, AActor* ship);
	static void DrawShipFar(AHUD* hud, AActor* ship);
	static void DrawCrosshair(AHUD* hud);
	static void DrawMapPins(AHUD* hud, AActor* actor);
	static void DrawCompass(AHUD* hud);
	static void DrawSelf(AHUD* hud);
	static void DrawSkeleton(AHUD* hud, AActor* actor);
	static void DrawStrongholdKey(AHUD* hud, AActor* actor);
	static void DrawAmmoChest(AHUD* hud, AActor* actor);
	static void DrawCookingPot(AHUD* hud, AActor* actor);
	static void DrawCollectorsChest(AHUD* hud, AActor* actor);
	static void DrawShipwreck(AHUD* hud, AActor* actor);
	static void DrawMermaid(AHUD* hud, AActor* actor);
	static void DrawStorm(AHUD* hud, AActor* actor);
	static void DrawBarrel(AHUD* hud, AActor* actor);
	static void DrawLandmark(AHUD* hud, AActor* actor);
	static void DrawDebug(AHUD* hud, AActor* actor);

	static void DrawHealthBar(AHUD* hud, UHealthComponent* healthComponent, FVector position, FVector2D size);
	static void DrawBoundingBox(AHUD* hud, AActor* actor, FLinearColor color = FLinearColor(255,255,255));

	static bool LandmarkIsNeeded(AHUD* hud, ALandmark* landmark, std::wstring* stringToDraw);

	static void GetFonts();
	static void RenderText(AHUD* hud, std::wstring text, FVector2D location, uint8_t alignment, FLinearColor color, bool outlined, UFont* font, float scale);
	static void RenderText(AHUD* hud, std::string text, FVector2D location, uint8_t alignment, FLinearColor color, bool outlined, UFont* font, float scale);
	static void Log(AHUD* hud, std::wstring text, FLinearColor color = FLinearColor(1.0f, 1.0f, 1.0f));
	static void Log(AHUD* hud, std::string text, FLinearColor color = FLinearColor(1.0f, 1.0f, 1.0f));
};