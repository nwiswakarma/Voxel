// 

#pragma once

#include "CoreMinimal.h"
#include "GWTTaskWorker.h"

class FVoxelDBCacheManager;
class FVoxelDBCacheWorker;
class FVoxelDBFetchData;
struct FVoxelDBCacheData;
struct sqlite3;
struct sqlite3_stmt;

typedef TSharedPtr<FVoxelDBCacheWorker> TPSVoxelDBCacheWorker;
typedef TWeakPtr<FVoxelDBCacheWorker>   TPWVoxelDBCacheWorker;

typedef TSharedPtr<FVoxelDBCacheData>   TPSVoxelDBCacheData;
typedef TWeakPtr<FVoxelDBCacheData>     TPWVoxelDBCacheData;

typedef TArray<FVoxelDBCacheData>       FVoxelDBCacheGroup;
typedef TArray<FVoxelDBCacheGroup>      FVoxelDBCacheLOD;
typedef TMap<uint64, FVoxelDBCacheLOD>  FVoxelDBCacheMap;

typedef TSharedPtr<FVoxelDBFetchData>   TPSVoxelDBFetchData;

typedef TPromise<TPSVoxelDBFetchData>    FVoxelDBFetchPromise;
typedef TFuture<TPSVoxelDBFetchData>     FVoxelDBFetchFuture;
typedef TSharedPtr<FVoxelDBFetchPromise> TPSVoxelDBFetchPromise;

struct FVoxelDBCacheData
{
    uint8 Depth;
    uint64 MeshId;
    FIntVector Offset;
    TArray<uint8> MeshData;

    FVoxelDBCacheData() = default;
    FVoxelDBCacheData(uint8 InDepth, uint64 InMeshId, const FIntVector& InOffset)
        : Depth(InDepth)
        , MeshId(InMeshId)
        , Offset(InOffset)
    {
    }
};

FORCEINLINE FArchive& operator<<(FArchive &Ar, FVoxelDBCacheData& CacheData)
{
	Ar << CacheData.Depth;
	Ar << CacheData.MeshId;
	Ar << CacheData.Offset;
	Ar << CacheData.MeshData;
	return Ar;
}

class FVoxelDBFetchData
{
    friend class FVoxelDBCacheWorker;
    friend class FVoxelRender;

    FVoxelDBCacheMap CacheMap;

    FVoxelDBFetchData() = default;
    FVoxelDBFetchData& operator=(const FVoxelDBFetchData& rhs) = default;

public:

    ~FVoxelDBFetchData()
    {
        Reset();
    }

    FORCEINLINE void Reset()
    {
        CacheMap.Empty();
    }

    FORCEINLINE FVoxelDBCacheMap& GetData()
    {
        return CacheMap;
    }
};

class FVoxelDBCacheWorker : public IGWTTaskWorker
{
    enum class ECommitTask : uint8
    {
        CREATE_TABLE,
        RESET_TABLE,
        MEMCOMMIT,
        COMMIT
    };

    sqlite3* DB = nullptr;

    uint32 const WorldId;
    FString const DBPath;
    FString const TBName;

    TQueue<ECommitTask, EQueueMode::Mpsc> CommitTaskQueue;
    TQueue<TPSVoxelDBCacheData, EQueueMode::Mpsc> CacheDataQueue;
    TQueue<TPSVoxelDBFetchPromise, EQueueMode::Mpsc> FetchPromiseQueue;
    TSharedPtr<FVoxelDBCacheManager> const DBCacheManager;

    bool bCachedTransient;
    bool bCachedPersistent;

    FORCEINLINE bool HasCachedData();
    void ExecuteCommitTask();
    void ExecuteFetchTask();

public:

    FVoxelDBCacheWorker(TSharedPtr<FVoxelDBCacheManager> InDBCacheManager, uint32 InWorldId);
    virtual ~FVoxelDBCacheWorker();

    virtual void Setup();
    virtual void Shutdown();
    virtual void Tick(float DeltaTime);

    void CommitCacheData();
    void FetchCacheData(FVoxelDBFetchFuture& FetchFuture);
    void EnqueueCacheData(TPSVoxelDBCacheData& CacheData);
};
