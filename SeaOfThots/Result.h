#pragma once

enum Result
{
	Success = 0x0000,

	NullPointer = 0x1000,
	WorldNull = 0x1001,
	LevelNull = 0x1002,
	ViewportNull = 0x1003,
	ActorClusterNull = 0x1004,
};