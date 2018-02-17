// Copyright 2017 Phyronnaz

#pragma once

#include "CoreMinimal.h"

class FVoxelThreadPool
{
    int32 UNIQUE_SLOT_ID = 0;
    FQueuedThreadPool* const ThreadPool;
    TSet<int32> SlotSet;

    // Task reserve count
    const int32 MaxTaskReserve;
    int32 SlotTaskReserve;

    friend class IVoxel;
    friend struct FSlotData;

public:
    struct FSlotData
    {
        const int32 SlotID;
        int32 CurrentActiveTask;
        int32 RemainingTaskCount;
        int32 MarkedRemainingTaskCount;

        FSlotData(FVoxelThreadPool* InThreadPool, int32 SlotID)
            : ThreadPool(InThreadPool)
            , SlotID(SlotID)
        {
            Reset();
        }

        FORCEINLINE void Reset()
        {
            CurrentActiveTask = 0;
            RemainingTaskCount = 0;
            MarkedRemainingTaskCount = 0;
        }

        FORCEINLINE bool HasRemainingTaskSlot() const
        {
            return CurrentActiveTask < ThreadPool->SlotTaskReserve;
        }

        FORCEINLINE int32 GetRemainingTaskCount() const
        {
            return RemainingTaskCount;
        }

        FORCEINLINE bool HasRemainingTask() const
        {
            return RemainingTaskCount > 0;
        }

        FORCEINLINE bool HasJustFinishedRemainingTask() const
        {
            return MarkedRemainingTaskCount > 0 && RemainingTaskCount == 0;
        }

        FORCEINLINE void MarkTaskUpdate()
        {
            MarkedRemainingTaskCount = RemainingTaskCount;
        }

        FORCEINLINE void UnmarkTaskUpdate()
        {
            MarkedRemainingTaskCount = 0;
        }

        FORCEINLINE void IncrementTaskCount()
        {
            check(MarkedRemainingTaskCount == 0);
            ++RemainingTaskCount;
        }

        FORCEINLINE void IncrementActiveTask()
        {
            check(RemainingTaskCount > 0);

            if (RemainingTaskCount > 0)
            {
                ++CurrentActiveTask;
            }
        }

        FORCEINLINE void DecrementActiveTask()
        {
            check(RemainingTaskCount > 0);

            if (RemainingTaskCount > 0)
            {
                --CurrentActiveTask;
                --RemainingTaskCount;
            }
        }

    private:
        const FVoxelThreadPool* const ThreadPool;
    };

    FVoxelThreadPool(int32 InThreadCount, int32 InMaxTaskReserve);
    ~FVoxelThreadPool();

    FORCEINLINE FQueuedThreadPool* GetThreadPool()
    {
        return ThreadPool;
    }

    FSlotData CreateThreadSlot();
    void DestroyThreadSlot(const FSlotData& SlotData);
    void UpdateSlotTaskReserve();
};
