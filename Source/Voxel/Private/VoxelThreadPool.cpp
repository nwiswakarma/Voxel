// Copyright 2017 Phyronnaz

#include "VoxelThreadPool.h"

FVoxelThreadPool::FVoxelThreadPool(int32 InThreadCount, int32 InMaxTaskReserve)
    : ThreadPool(FQueuedThreadPool::Allocate())
    , MaxTaskReserve(InMaxTaskReserve)
    , SlotTaskReserve(InMaxTaskReserve)
{
    check(InThreadCount > 0);
    check(MaxTaskReserve > 0);
    ThreadPool->Create(InThreadCount, 64 * 1024);
    //UE_LOG(LogTemp,Warning, TEXT("FVoxelThreadPool()"));
}

FVoxelThreadPool::~FVoxelThreadPool()
{
    ThreadPool->Destroy();
    SlotSet.Empty();
    //UE_LOG(LogTemp,Warning, TEXT("~FVoxelThreadPool()"));
}

FVoxelThreadPool::FSlotData FVoxelThreadPool::CreateThreadSlot()
{
    FSlotData SlotData(this, UNIQUE_SLOT_ID++);
    SlotSet.Emplace(SlotData.SlotID);

    UpdateSlotTaskReserve();
    //UE_LOG(LogTemp,Warning, TEXT("CreateThreadSlot() %d"), SlotData.SlotID);

    return MoveTemp(SlotData);
}

void FVoxelThreadPool::DestroyThreadSlot(const FVoxelThreadPool::FSlotData& SlotData)
{
    if (SlotSet.Contains(SlotData.SlotID))
    {
        SlotSet.Remove(SlotData.SlotID);
        UpdateSlotTaskReserve();
        //UE_LOG(LogTemp,Warning, TEXT("DestroyThreadSlot() %d"), SlotData.SlotID);
    }
}

void FVoxelThreadPool::UpdateSlotTaskReserve()
{
    const int32 SlotCount = FMath::Max(SlotSet.Num(), 1);
    SlotTaskReserve = MaxTaskReserve>SlotCount ? MaxTaskReserve/SlotCount : 1;
}
