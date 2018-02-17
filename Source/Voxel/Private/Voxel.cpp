// Copyright 2017 Phyronnaz

#include "VoxelModule.h"
#include "VoxelPrivate.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "VoxelModuleSettings.h"
#include "VoxelThreadPool.h"
#include "VoxelDBCacheManager.h"
#include "VoxelDBCacheWorker.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "CustomSettings"

class FVoxel : public IVoxel
{
    // Singleton weak-pointer of the global render thread pool.
    // Valid as long as there is at least one valid thread pool instance.
	TWeakPtr<FVoxelThreadPool> GRenderThreadPool;

    // DB cache worker thread instance.
	TPWVoxelDBCacheManager GDBCacheManager;

    //bool HandleSettingsSaved()
    //{
    //    UE_LOG(LogTemp,Warning, TEXT("VOXEL SETTINGS SAVED"));
    //    return true;
    //}

#if WITH_EDITOR

	void RegisterSettings()
	{
        // Register settings
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
		    ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "VoxelMesh",
		        LOCTEXT("RuntimeGeneralSettingsName", "Voxel Mesh"),
		        LOCTEXT("RuntimeGeneralSettingsDescription", "Voxel mesh plug-in configuration settings."),
		        GetMutableDefault<UVoxelModuleSettings>()
		        );
 
		    //if (SettingsSection.IsValid())
		    //{
            //    SettingsSection->OnModified().BindRaw(this, &FVoxel::HandleSettingsSaved);
		    //}
		}
	}

    void UnregisterSettings()
    {
		// Unregister settings
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
            SettingsModule->UnregisterSettings("Project", "Plugins", "VoxelMesh");
		}
    }

#endif // WITH_EDITOR

public:

	virtual void StartupModule() override
	{
#if WITH_EDITOR
        RegisterSettings();
#endif // WITH_EDITOR
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
        UnregisterSettings();
#endif // WITH_EDITOR
	}

    virtual TSharedPtr<FVoxelThreadPool> GetRenderThreadPoolInstance()
    {
        if (GRenderThreadPool.IsValid())
        {
            return GRenderThreadPool.Pin();
        }
        else
        {
            const UVoxelModuleSettings& Settings = *GetDefault<UVoxelModuleSettings>();

            //UE_LOG(LogTemp,Warning, TEXT("Voxel.RenderThreadCount: %d"), Settings.RenderThreadCount);
            //UE_LOG(LogTemp,Warning, TEXT("Voxel.RenderThreadMaxUpdateReserve: %d"), Settings.RenderThreadMaxUpdateReserve);

            TSharedPtr<FVoxelThreadPool> ThreadPool( new FVoxelThreadPool(Settings.RenderThreadCount, Settings.RenderThreadMaxUpdateReserve) );
            GRenderThreadPool = ThreadPool;
            return ThreadPool;
        }
    }

    virtual TPSVoxelDBCacheWorker GetDBCacheWorker(int32 WorkerId, bool bEnableDropTable)
    {
        TPSVoxelDBCacheManager DBCacheManager( GDBCacheManager.Pin() );

        // Create a new manager instance if there is no valid one
        if (! DBCacheManager.IsValid())
        {
            typedef UVoxelModuleSettings USettings;

            const USettings& Settings = *GetDefault<USettings>();
            const float RestTime = Settings.DBCacheThreadRestTime;
            const bool bVacuumOnClose = Settings.DBCacheVacuumOnClose;
            FString DBPath = Settings.DBPath.Path;

            // If specified directory does not exist,
            // revert to default game saved directory
            if (! FPaths::DirectoryExists(DBPath))
            {
                DBPath = USettings::GetDefaultDBPath();
            }

            // Create a new manager instance
            DBCacheManager = TPSVoxelDBCacheManager( new FVoxelDBCacheManager(RestTime, bVacuumOnClose, DBPath) );
            GDBCacheManager = DBCacheManager;
        }

        return DBCacheManager->CreateWorker(DBCacheManager, WorkerId);
    }
};

IMPLEMENT_MODULE(FVoxel, Voxel)
DEFINE_LOG_CATEGORY(LogVoxel);

#undef LOCTEXT_NAMESPACE
