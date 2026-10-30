#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

class FCambrianaTestWorld
{
public:
	FCambrianaTestWorld()
	{
		World = UWorld::CreateWorld(EWorldType::EditorPreview, false, TEXT("CambrianaTestWorld"));
		GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
	}

	~FCambrianaTestWorld()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	UWorld* Get() const { return World; }

private:
	UWorld* World = nullptr;
};
