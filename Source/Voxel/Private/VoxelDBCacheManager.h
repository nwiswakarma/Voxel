// 

#pragma once

class FVoxelDBCacheManager;
class FVoxelDBCacheWorker;
class FGWTAsyncThread;
struct sqlite3;
struct sqlite3_stmt;

typedef TSharedPtr<FVoxelDBCacheManager> TPSVoxelDBCacheManager;
typedef TWeakPtr<FVoxelDBCacheManager>   TPWVoxelDBCacheManager;

class FVoxelDBCacheManager
{
    FGWTAsyncThread* const DBThread;
    sqlite3* DB;

    FString const DBPath;
    FString const DBTmpPath;
    FString const TBName;
    FString const TBSchema;

    TSet<uint64> CachedIds;
    bool bVacuumOnClose;

    void Setup();

public:

    FVoxelDBCacheManager(float InThreadRestTime, bool bInVacuumOnClose, const FString& InDBPath);
    ~FVoxelDBCacheManager();

    void Commit();
    void Vacuum();
    FORCEINLINE bool HasCachedData(uint64 Id);

    TSharedPtr<FVoxelDBCacheWorker> CreateWorker(uint32 InWorldId);
    TSharedPtr<FVoxelDBCacheWorker> CreateWorker(TPSVoxelDBCacheManager ManagerInstance, uint32 InWorldId);
    FORCEINLINE const FString& GetPersistentDBPath() const;
    FORCEINLINE const FString& GetTransientDBPath() const;
    FORCEINLINE const FString& GetCacheTableName() const;
};
