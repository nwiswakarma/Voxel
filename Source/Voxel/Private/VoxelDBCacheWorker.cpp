// 

#include "VoxelDBCacheWorker.h"
#include "VoxelDBCacheManager.h"
#include "VoxelProceduralMeshTypes.h"
#include "GWTAsyncThread.h"

#include "SQLiteTypes.h"
#include "ZSTDTypes.h"

DECLARE_CYCLE_STAT(TEXT("VoxelDBWorker ~ DB SETUP"), STAT_DBW_Setup, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelDBWorker ~ DB SHUTDOWN"), STAT_DBW_Shutdown, STATGROUP_Voxel);

DECLARE_CYCLE_STAT(TEXT("VoxelDBWorker ~ DB CREATE"), STAT_DBW_Create, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelDBWorker ~ DB RESET"), STAT_DBW_Reset, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelDBWorker ~ DB MEMCOMMIT"), STAT_DBW_MemCommit, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelDBWorker ~ DB COMMIT"), STAT_DBW_Commit, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelDBWorker ~ DB FETCH"), STAT_DBW_Fetch, STATGROUP_Voxel);

FVoxelDBCacheWorker::FVoxelDBCacheWorker(TSharedPtr<FVoxelDBCacheManager> InDBCacheManager, uint32 InWorldId)
    : DBCacheManager(InDBCacheManager)
    , DBPath(InDBCacheManager->GetTransientDBPath())
    , TBName(InDBCacheManager->GetCacheTableName())
    , WorldId(InWorldId)
    , bCachedTransient(false)
    , bCachedPersistent(false)
{
}

FVoxelDBCacheWorker::~FVoxelDBCacheWorker()
{
    Shutdown();
}

void FVoxelDBCacheWorker::Setup()
{
    if (! DB)
    {
        SCOPE_CYCLE_COUNTER(STAT_DBW_Setup);

        UE_LOG(LogTemp,Warning, TEXT("DBPath.Table: %s.%s"), *DBPath, *TBName);

        // Open database
        int32 DBOpenRetVal = sqlite3_open_v2(TCHAR_TO_UTF8(*DBPath), &DB, SQLITE_OPEN_READWRITE, NULL);
        check(DBOpenRetVal == SQLITE_OK);

        // Set database pragmas
        sqlite3_exec(DB, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
        sqlite3_exec(DB, "PRAGMA journal_mode = OFF", NULL, NULL, NULL);
    }

    bCachedPersistent = DBCacheManager->HasCachedData(WorldId);
}

void FVoxelDBCacheWorker::Shutdown()
{
    if (DB)
    {
        SCOPE_CYCLE_COUNTER(STAT_DBW_Shutdown);

        // Close cache database
        int32 DBCloseRetVal = sqlite3_close(DB);
        check(DBCloseRetVal == SQLITE_OK);
        DB = nullptr;
    }

    // Clears cache data queue
    CommitTaskQueue.Empty();
    CacheDataQueue.Empty();

    // Clears fetch data queue
    while (! FetchPromiseQueue.IsEmpty())
    {
        TPSVoxelDBFetchPromise FetchPromise;
        FetchPromiseQueue.Dequeue(FetchPromise);
        // Set promise value
        FetchPromise->SetValue(TPSVoxelDBFetchData());
    }

    bCachedTransient = false;
    bCachedPersistent = false;

    check(CommitTaskQueue.IsEmpty());
    check(CacheDataQueue.IsEmpty());
    check(FetchPromiseQueue.IsEmpty());
}

void FVoxelDBCacheWorker::Tick(float DeltaTime)
{
    // Commit operations
    if (! CommitTaskQueue.IsEmpty())
    {
        ExecuteCommitTask();
    }

    // Fetch operations
    if (! FetchPromiseQueue.IsEmpty())
    {
        ExecuteFetchTask();
    }
}

void FVoxelDBCacheWorker::ExecuteCommitTask()
{
    // No task, abort
    if (CommitTaskQueue.IsEmpty())
    {
        return;
    }

    // Make sure there is a valid DB connection
    // and we have no cached data stored in the database
    check(! HasCachedData());
    check(DB);

    ECommitTask Task;
    CommitTaskQueue.Dequeue(Task);

    switch (Task)
    {
        case ECommitTask::RESET_TABLE:
            //{
            //    SCOPE_CYCLE_COUNTER(STAT_DBW_Reset);

            //    // Clears persistent table
            //    FString ResetSQL = FString::Printf(TEXT("DELETE FROM %s;"), *TableName);
            //    sqlite3_exec(DB, TCHAR_TO_UTF8(*ResetSQL), NULL, NULL, NULL);
            //}
            //UE_LOG(LogTemp,Warning, TEXT("ECommitTask::RESET_TABLE"));
            break;

        case ECommitTask::CREATE_TABLE:
            //{
            //    SCOPE_CYCLE_COUNTER(STAT_DBW_Create);

            //    FString TableSchema( TEXT("(Id INTEGER PRIMARY KEY, X INTEGER, Y INTEGER, Z INTEGER, Depth INTEGER, MeshId INTEGER, Data BLOB)") );
            //    FString CreatePersistentSQL( FString::Printf(TEXT("CREATE TABLE IF NOT EXISTS %s %s;"), *TableName, *TableSchema) );
            //    FString CreateTransientSQL( FString::Printf(TEXT("CREATE TABLE MemDB.VoxelWorld %s;"), *TableSchema) );

            //    // Create persistent table
            //    sqlite3_exec(DB, TCHAR_TO_UTF8(*CreatePersistentSQL), NULL, NULL, NULL);

            //    // Create transient table
            //    sqlite3_exec(DB, "ATTACH DATABASE \':memory:\' AS MemDB;", NULL, NULL, NULL);
            //    sqlite3_exec(DB, TCHAR_TO_UTF8(*CreateTransientSQL), NULL, NULL, NULL);
            //}
            //UE_LOG(LogTemp,Warning, TEXT("ECommitTask::CREATE_TABLE"));
            break;

        case ECommitTask::MEMCOMMIT:
            {
                SCOPE_CYCLE_COUNTER(STAT_DBW_MemCommit);

                FString InsertSQL( FString::Printf(TEXT("INSERT OR REPLACE INTO %s (Id, ChunkCount, LODCount, Data) VALUES (?, ?, ?, ?);"), *TBName) );
                sqlite3_stmt* insert_stmt;
                sqlite3_prepare_v2(DB, TCHAR_TO_UTF8(*InsertSQL), -1, &insert_stmt, NULL);

                FBufferArchive ToBinary(true); // Persistent buffer archive
                int32 ChunkCount = 0;
                uint8 LowestLOD = 0;

                // Construct binary cache data
                while (! CacheDataQueue.IsEmpty())
                {
                    TPSVoxelDBCacheData CacheDataPtr;
                    CacheDataQueue.Dequeue(CacheDataPtr);

                    ++ChunkCount;
                    LowestLOD = FMath::Max(LowestLOD, CacheDataPtr->Depth);

                    // Serialize cache data and write to byte buffer
                    ToBinary << *CacheDataPtr.Get();

                    // Reset cache data
                    CacheDataPtr.Reset();
                }

                // Compress byte buffer
                TPSZSTDBufferData CompressedBinary;
                {
                    const SIZE_T DataSize = ToBinary.Num();
                    const void* DataPtr = static_cast<const void*>(ToBinary.GetData());
                    CompressedBinary = MoveTemp( FZSTDUtils::CompressData(DataPtr, DataSize) );
                    check(CompressedBinary.IsValid());
                }
                const SIZE_T DataSize = CompressedBinary->BufferSize;
                const void* DataPtr = static_cast<const void*>(CompressedBinary->Buffer);

                sqlite3_bind_int64(insert_stmt, 1, WorldId);
                sqlite3_bind_int(insert_stmt, 2, ChunkCount);
                sqlite3_bind_int(insert_stmt, 3, LowestLOD+1);
                sqlite3_bind_blob(insert_stmt, 4, DataPtr, DataSize, SQLITE_TRANSIENT);

                sqlite3_step(insert_stmt);
                sqlite3_finalize(insert_stmt);

                // Clear buffers
                ToBinary.Empty();
                CompressedBinary.Reset();

                bCachedTransient = true;
            }
            UE_LOG(LogTemp,Warning, TEXT("ECommitTask::MEMCOMMIT"));
            break;

        case ECommitTask::COMMIT:
            //{
            //    SCOPE_CYCLE_COUNTER(STAT_DBW_Commit);

            //    // Commit data from the transient database
            //    FString CommitSQL( TEXT("INSERT INTO %s (X,Y,Z, Depth, MeshId, Data) SELECT X,Y,Z, Depth, MeshId, Data FROM MemDB.VoxelWorld;") );
            //    sqlite3_exec(DB, TCHAR_TO_UTF8(*FString::Printf(*CommitSQL, *TableName)), NULL, NULL, NULL);

            //    // Detach transient database
            //    sqlite3_exec(DB, "DETACH DATABASE MemDB;", NULL, NULL, NULL);
            //}
            //UE_LOG(LogTemp,Warning, TEXT("ECommitTask::COMMIT"));
            break;
    }
}

void FVoxelDBCacheWorker::ExecuteFetchTask()
{
    // No fetch promise, abort
    if (FetchPromiseQueue.IsEmpty())
    {
        return;
    }

    sqlite3* db = nullptr;

    if (bCachedTransient)
    {
        db = DB;
    }
    else if (bCachedPersistent)
    {
        int32 DBOpenRetVal = sqlite3_open_v2(
            TCHAR_TO_UTF8(*DBCacheManager->GetPersistentDBPath()),
            &db,
            SQLITE_OPEN_READONLY,
            NULL
        );
        check(DBOpenRetVal == SQLITE_OK);
    }

    // Make sure there is a valid database connection
    check(db);

    TPSVoxelDBFetchData FetchData( new FVoxelDBFetchData() );
    FVoxelDBCacheMap& CacheMap( FetchData->CacheMap );

    {
        SCOPE_CYCLE_COUNTER(STAT_DBW_Fetch);

        // Deserialized fetch result container
        TArray<FVoxelDBCacheData> CacheArray;

        // Fetch database cache and reserve fetch data result containers
        {
            FString FetchSQL;
            FetchSQL = TEXT("SELECT ChunkCount, LODCount, Data FROM %s WHERE Id = %llu;");
            FetchSQL = FString::Printf(*FetchSQL, *TBName, WorldId);
            sqlite3_stmt* fetch_stmt;
            sqlite3_prepare_v2(db, TCHAR_TO_UTF8(*FetchSQL), -1, &fetch_stmt, NULL);

            if (sqlite3_step(fetch_stmt) == SQLITE_ROW)
            {
                const int32 ChunkCount = sqlite3_column_int(fetch_stmt, 0);
                const int32 LODCount = sqlite3_column_int(fetch_stmt, 1);

                // Decompress byte buffer
                TPSZSTDBufferData DecompressedBinary;
                {
                    const void* DataPtr = sqlite3_column_blob(fetch_stmt, 2);
                    int32 DataSize = sqlite3_column_bytes(fetch_stmt, 2);
                    DecompressedBinary = MoveTemp( FZSTDUtils::DecompressData(DataPtr, DataSize) );
                    check(DecompressedBinary.IsValid());
                }
                const SIZE_T DataSize = DecompressedBinary->BufferSize;
                const void* DataPtr = static_cast<const void*>(DecompressedBinary->Buffer);

                // Copy data to byte array
                TArray<uint8> ByteArray;
                ByteArray.SetNumZeroed(DataSize);
                FMemory::Memcpy(ByteArray.GetData(), DataPtr, DataSize);

                // Create memory reader
                FMemoryReader FromBinary(ByteArray, true); // Persistent memory reader

                // Deserialize cache data
                CacheArray.SetNum(ChunkCount);
                FromBinary.Seek(0);
                for (int32 i=0; i<ChunkCount; ++i)
                {
                    // Deserialize cache data
                    FVoxelDBCacheData& CacheData( CacheArray[i] );
                    FromBinary << CacheData;

                    // Map cache data
                    if (! CacheMap.Contains(CacheData.MeshId))
                    {
                        // Emplace new LOG groups
                        CacheMap.Emplace(CacheData.MeshId, FVoxelDBCacheLOD());
                        FVoxelDBCacheLOD& CacheLODs( CacheMap.FindChecked(CacheData.MeshId) );

                        // Reserves LOD group containers
                        CacheLODs.SetNum(LODCount);
                        for (FVoxelDBCacheGroup& CacheGroup : CacheLODs)
                        {
                            CacheGroup.Reserve(ChunkCount);
                        }
                    }
                }
                check(FromBinary.AtEnd());

                // Clear buffers
                ByteArray.Empty();
                DecompressedBinary.Reset();

                // Sort by MeshId
                CacheArray.Sort([&](const FVoxelDBCacheData& v0, const FVoxelDBCacheData& v1) {
                    return v0.MeshId < v1.MeshId;
                } );
            }

            sqlite3_finalize(fetch_stmt);
        }

        // Map fetch result
        {
            uint64 PrevMeshId = 0;
            FVoxelDBCacheLOD* CacheLODs = nullptr;

            for (const FVoxelDBCacheData& CacheData : CacheArray)
            {
                if (PrevMeshId != CacheData.MeshId)
                {
                    PrevMeshId = CacheData.MeshId;
                    CacheLODs = &CacheMap.FindChecked(PrevMeshId);
                }

                (*CacheLODs)[CacheData.Depth].Emplace(CacheData);
            }

            // Clears fetch result
            CacheArray.Empty();
        }

        // Shrink cache data containers
        for (auto& CacheMapPair : CacheMap)
        {
            FVoxelDBCacheLOD& CacheLODs( CacheMapPair.Value );

            for (FVoxelDBCacheGroup& CacheGroup : CacheLODs)
            {
                CacheGroup.Shrink();
            }
        }
    }

    TPSVoxelDBFetchPromise FetchPromise;
    FetchPromiseQueue.Dequeue(FetchPromise);
    // Set promise value
    FetchPromise->SetValue( MoveTemp(FetchData) );

    // If we open a new persistent database connection, close it
    if (db != DB)
    {
        int32 DBCloseRetVal = sqlite3_close(db);
        check(DBCloseRetVal == SQLITE_OK);
    }

    UE_LOG(LogTemp,Warning, TEXT("FETCH TASK COMPLETED"));
}

void FVoxelDBCacheWorker::CommitCacheData()
{
    check(DB);

    if (! HasCachedData() && ! CacheDataQueue.IsEmpty())
    {
        //CommitTaskQueue.Enqueue(ECommitTask::RESET_TABLE);
        //CommitTaskQueue.Enqueue(ECommitTask::CREATE_TABLE);
        CommitTaskQueue.Enqueue(ECommitTask::MEMCOMMIT);
        //CommitTaskQueue.Enqueue(ECommitTask::COMMIT);
    }
}

void FVoxelDBCacheWorker::FetchCacheData(FVoxelDBFetchFuture& FetchFuture)
{
    if (HasCachedData())
    {
        TPSVoxelDBFetchPromise FetchPromise( new FVoxelDBFetchPromise() );
        FetchPromiseQueue.Enqueue(FetchPromise);
        FetchFuture = FetchPromise->GetFuture();
    }
    else
    {
        // Data table does not exist, assign invalidate input future
        FetchFuture = FVoxelDBFetchFuture();
    }
}

void FVoxelDBCacheWorker::EnqueueCacheData(TPSVoxelDBCacheData& CacheData)
{
    if (! HasCachedData())
    {
        CacheDataQueue.Enqueue(MoveTemp(CacheData));
    }
}

bool FVoxelDBCacheWorker::HasCachedData()
{
    return bCachedTransient || bCachedPersistent;
}
