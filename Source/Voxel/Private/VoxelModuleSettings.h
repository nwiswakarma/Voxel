// Copyright 2015 Moritz Wundke. All Rights Reserved.
// Released under MIT.
 
#pragma once
 
#include "VoxelModuleSettings.generated.h"
 
UCLASS(config = Engine, defaultconfig)
class VOXEL_API UVoxelModuleSettings : public UObject
{
	GENERATED_BODY()
 
public:
 
	/** Available thread for voxel mesh rendering */
	UPROPERTY(EditAnywhere, config, Category = Voxel, meta = (UIMin = 1, ClampMin = 1))
	int32 RenderThreadCount = 4;
 
	/** Maximum number of available update for all render thread slots */
	UPROPERTY(EditAnywhere, config, Category = Voxel, meta = (UIMin = 1, ClampMin = 1))
	int32 RenderThreadMaxUpdateReserve = 24;
 
	/** Directory path for mesh cache database files */
	UPROPERTY(EditAnywhere, config, Category = Voxel, meta = (UIMin = 1, ClampMin = 1, DisplayName = "Database Path"))
	FDirectoryPath DBPath;
 
	/** Maximum number of available update for all cache thread slots */
	UPROPERTY(EditAnywhere, config, Category = Voxel, meta = (UIMin = 0.001f, ClampMin = 0.001f, DisplayName = "Database Thread Rest Time"))
	float DBCacheThreadRestTime = 0.03f;
 
	/** Whether to vacuum world cache database on application close */
	UPROPERTY(EditAnywhere, config, Category = Voxel, meta = (DisplayName = "Vacuum Cache Database On Close"))
	bool DBCacheVacuumOnClose = false;
 
	//UPROPERTY(EditAnywhere, config, Category = Voxel)
	//TArray<FString> SampleStringList;
 
	//UPROPERTY(EditAnywhere, config, Category = Voxel, meta = (ConfigRestartRequired = true))
	//float SampleFloatRequireRestart;
 
	//UPROPERTY(EditAnywhere, config, Category = Materials, meta = (AllowedClasses = "MaterialInterface"))
	//FStringAssetReference StringMaterialAssetReference;

    UVoxelModuleSettings();
    static FString GetDefaultDBPath();
};
