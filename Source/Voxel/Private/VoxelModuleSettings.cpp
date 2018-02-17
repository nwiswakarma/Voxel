// 

#include "VoxelModuleSettings.h"

UVoxelModuleSettings::UVoxelModuleSettings()
{
    DBPath.Path = GetDefaultDBPath();
}

FString UVoxelModuleSettings::GetDefaultDBPath()
{
    FString Path = FString::Printf(TEXT("%sDB"), *FPaths::GameSavedDir());
    FPaths::MakePlatformFilename(Path);

    // Create a new directory if the default path does not exist
    if (! FPaths::DirectoryExists(Path))
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        PlatformFile.CreateDirectory(*Path);
    }

    check(FPaths::DirectoryExists(Path));

    return Path;
}

