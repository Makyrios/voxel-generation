#pragma once

enum class EDirection
{
	Forward,
	Right,
	Backward,
	Left,
	Up,
	Down
};

UENUM()
enum class EBlock
{
	Air UMETA(DisplayName = "Air"),
	Grass UMETA(DisplayName = "Grass"),
	Dirt UMETA(DisplayName = "Dirt"),
	Stone UMETA(DisplayName = "Stone")
};
