// 

#include "VoxelDBCacheManager.h"
#include "VoxelDBCacheWorker.h"
#include "GWTAsyncThread.h"
#include "SQLiteTypes.h"

#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"

DECLARE_CYCLE_STAT(TEXT("VoxelDBManager ~ DB SETUP"), STAT_DBM_Setup, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelDBManager ~ DB SHUTDOWN"), STAT_DBM_Shutdown, STATGROUP_Voxel);

FVoxelDBCacheManager::FVoxelDBCacheManager(float InThreadRestTime, bool bInVacuumOnClose, const FString& InDBPath)
    : DB(nullptr)
    , DBThread(new FGWTAsyncThread(InThreadRestTime))

    , DBPath(InDBPath + TEXT("/VoxelWorld.db"))
    , DBTmpPath(InDBPath + TEXT("/VoxelWorld_tmp.db"))

    , TBName(TEXT("Worlds"))
    , TBSchema(TEXT("(Id INTEGER PRIMARY KEY, ChunkCount INTEGER, LODCount INTEGER, Data BLOB)"))

    , bVacuumOnClose(bInVacuumOnClose)
{
    check(FPaths::DirectoryExists(InDBPath));
    Setup();
}

FVoxelDBCacheManager::~FVoxelDBCacheManager()
{
    check(DB != nullptr);

    SCOPE_CYCLE_COUNTER(STAT_DBM_Shutdown);

    // Shutdown cache thread
    delete DBThread;

    Commit();

    if (bVacuumOnClose)
    {
        Vacuum();
    }

    // Close cache database
    int32 DBCloseRetVal = sqlite3_close(DB);
    check(DBCloseRetVal == SQLITE_OK);

    // Drop existing transient database file
    IPlatformFile& PlatformFile( FPlatformFileManager::Get().GetPlatformFile() );
    PlatformFile.DeleteFile(*DBTmpPath);

}

void FVoxelDBCacheManager::Setup()
{
    check(DB == nullptr);

    SCOPE_CYCLE_COUNTER(STAT_DBM_Setup);

    // Worlds table name and schema
    const FString& WorldsTableName( TBName );
    const FString& WorldsTableSchema( TBSchema );

    // Open persistent database connection
    {
        int32 DBOpenRetVal = sqlite3_open_v2(
            TCHAR_TO_UTF8(*DBPath),
            &DB,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            NULL
        );
        check(DBOpenRetVal == SQLITE_OK);

        // Create worlds table
        {
            FString CreateSQL( FString::Printf(TEXT("CREATE TABLE IF NOT EXISTS %s %s;"), *WorldsTableName, *WorldsTableSchema) );
            sqlite3_exec(DB, TCHAR_TO_UTF8(*CreateSQL), NULL, NULL, NULL);
        }

        // Create persistently cached world references
        {
            FString FetchSQL = FString::Printf(TEXT("SELECT Id FROM %s;"), *WorldsTableName);
            sqlite3_stmt* fetch_stmt;
            sqlite3_prepare_v2(DB, TCHAR_TO_UTF8(*FetchSQL), -1, &fetch_stmt, NULL);
            while (sqlite3_step(fetch_stmt) == SQLITE_ROW)
            {
                uint32 WorldId = static_cast<uint32>( sqlite3_column_int64(fetch_stmt, 0) );
                CachedIds.Emplace(WorldId);
            }
            sqlite3_finalize(fetch_stmt);
        }
    }

    // Create transient database tables
    {
        // Open transient database connection
        sqlite3* DBTransient = nullptr;
        {
            int32 DBOpenRetVal = sqlite3_open_v2(
                TCHAR_TO_UTF8(*DBTmpPath),
                &DBTransient,
                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                NULL
            );
            check(DBOpenRetVal == SQLITE_OK);
        }

        // Create worlds table
        {
            FString CreateSQL( FString::Printf(TEXT("CREATE TABLE IF NOT EXISTS %s %s;"), *WorldsTableName, *WorldsTableSchema) );
            sqlite3_exec(DBTransient, TCHAR_TO_UTF8(*CreateSQL), NULL, NULL, NULL);
        }

        // Close transient database connection
        int32 DBCloseRetVal = sqlite3_close(DBTransient);
        check(DBCloseRetVal == SQLITE_OK);
    }

    // Start database cache thread
    DBThread->StartThread();
}

void FVoxelDBCacheManager::Commit()
{
    check(DB);

    // Attach transient database
    {
        FString AttachSQL( FString::Printf(TEXT("ATTACH DATABASE \'%s\' AS TransientDB;"), *DBTmpPath) );
        sqlite3_exec(DB, TCHAR_TO_UTF8(*AttachSQL), NULL, NULL, NULL);
    }

    // Commit data
    {
        FString CommitSQL;
        CommitSQL = TEXT("INSERT INTO %s (Id, ChunkCount, LODCount, Data) SELECT Id, ChunkCount, LODCount, Data FROM TransientDB.%s;");
        CommitSQL = FString::Printf(*CommitSQL, *TBName, *TBName);
        sqlite3_exec(DB, TCHAR_TO_UTF8(*CommitSQL), NULL, NULL, NULL);
    }

    // Detach transient database
    {
        sqlite3_exec(DB, "DETACH DATABASE TransientDB;", NULL, NULL, NULL);
    }
}

void FVoxelDBCacheManager::Vacuum()
{
    check(DB);

    // Vacuum persistent database
    {
        sqlite3_exec(DB, "VACUUM;", NULL, NULL, NULL);
    }
}

bool FVoxelDBCacheManager::HasCachedData(uint64 Id)
{
    return CachedIds.Contains(Id);
}

TSharedPtr<FVoxelDBCacheWorker> FVoxelDBCacheManager::CreateWorker(uint32 InWorldId)
{
    return CreateWorker(MakeShareable(this), InWorldId);
}

TSharedPtr<FVoxelDBCacheWorker> FVoxelDBCacheManager::CreateWorker(TPSVoxelDBCacheManager ManagerInstance, uint32 InWorldId)
{
    // Make sure the passed instance is a reference to this
    check(ManagerInstance.IsValid());
    check(ManagerInstance.Get() == this);

    TPSVoxelDBCacheWorker DBWorker( new FVoxelDBCacheWorker(ManagerInstance, InWorldId) );
    DBWorker->Setup();
    DBThread->AddWorker(DBWorker);

    return DBWorker;
}

const FString& FVoxelDBCacheManager::GetPersistentDBPath() const
{
    return DBPath;
}

const FString& FVoxelDBCacheManager::GetTransientDBPath() const
{
    return DBTmpPath;
}

const FString& FVoxelDBCacheManager::GetCacheTableName() const
{
    return TBName;
}
