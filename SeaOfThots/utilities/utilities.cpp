#include "utilities.h"
using namespace SDK;

float Utilities::DotProduct(FVector a, FVector b)
{
	return (a.X * b.X + a.Y * b.Y + a.Z * b.Z);
}

std::string Utilities::string_format(const std::string fmt_str, ...) {
	int final_n, n = ((int)fmt_str.size()) * 2; /* Reserve two times as much as the length of the fmt_str */
	std::unique_ptr<char[]> formatted;
	va_list ap;
	while (1) {
		formatted.reset(new char[n]); /* Wrap the plain char array into the unique_ptr */
		strcpy(&formatted[0], fmt_str.c_str());
		va_start(ap, fmt_str);
		final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
		va_end(ap);
		if (final_n < 0 || final_n >= n)
			n += abs(final_n - n + 1);
		else
			break;
	}
	return std::string(formatted.get());
}

bool Utilities::WorldToScreen(FVector pos, UWorld* worldRef, FVector2D* screen, AHUD* hud) {
	ULocalPlayer* localPlayer = worldRef->OwningGameInstance->LocalPlayers[0];
	APlayerCameraManager* cameraManager = localPlayer->PlayerController->PlayerCameraManager;
	FRotator cameraRotation = cameraManager->GetCameraRotation();
	FVector cameraPosition = cameraManager->GetCameraLocation();
	float cameraFov = cameraManager->GetFOVAngle();

	FVector Screenlocation = FVector(0, 0, 0);
	FVector playerRotation(cameraRotation.Pitch, cameraRotation.Yaw, cameraRotation.Roll);

	vMatrix tempMatrix = Matrix(playerRotation, FVector(0, 0, 0));

	FVector vAxisX, vAxisY, vAxisZ;

	vAxisX = FVector(tempMatrix[0][0], tempMatrix[0][1], tempMatrix[0][2]);
	vAxisY = FVector(tempMatrix[1][0], tempMatrix[1][1], tempMatrix[1][2]);
	vAxisZ = FVector(tempMatrix[2][0], tempMatrix[2][1], tempMatrix[2][2]);

	FVector vDelta = pos - cameraPosition;
	FVector vTransformed = FVector(DotProduct(vDelta, vAxisY), DotProduct(vDelta, vAxisZ), DotProduct(vDelta, vAxisX));

	if (vTransformed.Z < 1.f)
		vTransformed.Z = 1.f;

	float ScreenCenterX = hud->Canvas->SizeX / 2.0f;
	float ScreenCenterY = hud->Canvas->SizeY / 2.0f;

	auto tmpFOV = tanf(cameraFov * (float)PI / 360.f);

	screen->X = ScreenCenterX + vTransformed.X * (ScreenCenterX / tmpFOV) / vTransformed.Z;
	screen->Y = ScreenCenterY - vTransformed.Y * (ScreenCenterX / tmpFOV) / vTransformed.Z;



	auto Ratio = 1920 / 1080;
	if (Ratio < 4.0f / 3.0f)
		Ratio = 4.0f / 3.0f;

	auto FOV = Ratio / (16.0f / 9.0f) * tanf(cameraFov * PI / 360.0f);

	int debug = FOV;

	//screen->x = ScreenCenterX + vTransformed.x * ScreenCenterX / FOV / vTransformed.z;
	//screen->y = ScreenCenterY - vTransformed.y * ScreenCenterX / FOV / vTransformed.z;

	return true;
}

Utilities::vMatrix Utilities::Matrix(FVector rot, FVector origin)
{
	origin = FVector(0, 0, 0);
	float radPitch = (rot.X * float(PI) / 180.f);
	float radYaw = (rot.Y * float(PI) / 180.f);
	float radRoll = (rot.Z * float(PI) / 180.f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	vMatrix matrix;
	matrix[0][0] = CP * CY;
	matrix[0][1] = CP * SY;
	matrix[0][2] = SP;
	matrix[0][3] = 0.f;

	matrix[1][0] = SR * SP * CY - CR * SY;
	matrix[1][1] = SR * SP * SY + CR * CY;
	matrix[1][2] = -SR * CP;
	matrix[1][3] = 0.f;

	matrix[2][0] = -(CR * SP * CY + SR * SY);
	matrix[2][1] = CY * SR - CR * SP * SY;
	matrix[2][2] = CR * CP;
	matrix[2][3] = 0.f;

	return matrix;
}