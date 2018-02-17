// Copyright 2017 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"

/**
 * The public interface to this module
 */
class IVoxel : public IModuleInterface
{
public:
	FORCEINLINE static IVoxel& Get()
	{
		return FModuleManager::LoadModuleChecked<IVoxel>("Voxel");
	}

	FORCEINLINE static bool IsAvailable()
	{
        return FModuleManager::Get().IsModuleLoaded("Voxel");
	}

    virtual TSharedPtr<class FVoxelThreadPool> GetRenderThreadPoolInstance() = 0;
    virtual TSharedPtr<class FVoxelDBCacheWorker> GetDBCacheWorker(int32 WorkerId, bool bEnableDropTable = false) = 0;
};

DECLARE_STATS_GROUP(TEXT("Voxels"), STATGROUP_Voxel, STATCAT_Advanced);
