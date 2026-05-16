// Copyright © 2026 kafues511 All Rights Reserved.

#include "ReinClothSubsystem.h"

#include "ReinClothViewExtension.h"

/**
 * @brief シミュレーション可能な最大数
 */
static constexpr int32 MaxGlobalSections = 64;

void UReinClothSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	ClothSettings.Empty();

	SectionCounter = 0;
	VacantSections.Empty();

	if (!ViewExtension.IsValid())
	{
		ViewExtension = FSceneViewExtensions::NewExtension<FReinClothViewExtension>(this);
	}

	Super::Initialize(Collection);
}

void UReinClothSubsystem::Deinitialize()
{
	ClothSettings.Reset();
	VacantSections.Reset();
	SharedBoneCacheMap.Reset();

	ViewExtension->Invalidate();

	Super::Deinitialize();
}

#pragma region GlobalSection
int32 UReinClothSubsystem::AllocationGlobalSectionIndex()
{
	if (VacantSections.Num() > 0)
	{
		// 空きがある場合は優先使用
		return VacantSections.Pop();
	}

	if (SectionCounter < MaxGlobalSections)
	{
		// 新規確保
		ClothSettings.AddDefaulted();
		return SectionCounter++;
	}

	// これ以上シミュレーションできない
	return INDEX_NONE;
}

void UReinClothSubsystem::ReleaseGlobalSectionIndex(int32 GlobalSectionIndex)
{
	if (GlobalSectionIndex < 0 || GlobalSectionIndex >= SectionCounter)
	{
		// 不正な値を解放しないで
		return;
	}

	if (VacantSections.Contains(GlobalSectionIndex))
	{
		// 多重解放禁止
		return;
	}

	if (!ClothSettings.IsValidIndex(GlobalSectionIndex))
	{
		// Allocation失敗してない？
		ensureAlways(false);
		return;
	}

	// 再利用可能
	VacantSections.Add(GlobalSectionIndex);

	ClothSettings[GlobalSectionIndex].bIsSimulation = false;
}
#pragma endregion

#pragma region SharedBone
void UReinClothSubsystem::RegisterSharedBoneCache(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (!IsValid(SkeletalMeshComponent))
	{
		return;
	}

	TWeakObjectPtr<USkeletalMeshComponent> WeakSkeletalMeshComponent(SkeletalMeshComponent);

	auto& SharedBoneCache = SharedBoneCacheMap.FindOrAdd(WeakSkeletalMeshComponent);

	SharedBoneCache.RefCount++;

	// 初回登録時はデリゲートのセットアップ
	if (SharedBoneCache.RefCount == 1)
	{
		SharedBoneCache.BoneTransformsFinalizedHandle = SkeletalMeshComponent->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateWeakLambda(this, [this, WeakSkeletalMeshComponent]()
				{
					OnBoneTransformsUpdated(WeakSkeletalMeshComponent);
				}));
	}
}

void UReinClothSubsystem::UnregisterSharedBoneCache(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (!IsValid(SkeletalMeshComponent))
	{
		return;
	}

	TWeakObjectPtr<USkeletalMeshComponent> WeakSkeletalMeshComponent(SkeletalMeshComponent);

	auto SharedBoneCache = SharedBoneCacheMap.Find(WeakSkeletalMeshComponent);
	if (SharedBoneCache == nullptr)
	{
		return;
	}

	SharedBoneCache->RefCount--;

	// 参照がゼロになったら破棄
	if (SharedBoneCache->RefCount <= 0)
	{
		if (SharedBoneCache->BoneTransformsFinalizedHandle.IsValid())
		{
			SkeletalMeshComponent->UnregisterOnBoneTransformsFinalizedDelegate(SharedBoneCache->BoneTransformsFinalizedHandle);
		}
		SharedBoneCacheMap.Remove(WeakSkeletalMeshComponent);
	}

	// ゼロ未満は想定していない
	ensureAlways(SharedBoneCache->RefCount >= 0);
}

void UReinClothSubsystem::OnBoneTransformsUpdated(TWeakObjectPtr<USkeletalMeshComponent> WeakSkeletalMeshComponent)
{
	const auto SkeletalMeshComponent = WeakSkeletalMeshComponent.Get();
	if (!IsValid(SkeletalMeshComponent))
	{
		return;
	}

	auto SharedBoneCache = SharedBoneCacheMap.Find(WeakSkeletalMeshComponent);
	if (SharedBoneCache == nullptr)
	{
		return;
	}

	auto& Dst = SharedBoneCache->BoneMatrices;

	TArray<FMatrix44f> RefToLocals;
	SkeletalMeshComponent->CacheRefToLocalMatrices(RefToLocals);

	auto NumBoneMatrices = RefToLocals.Num();

	if (Dst.Num() != NumBoneMatrices)
	{
		Dst.SetNumUninitialized(NumBoneMatrices);
	}

	for (int32 Index = 0; Index < NumBoneMatrices; ++Index)
	{
		auto BoneMatrix = RefToLocals[Index].GetTransposed();
		Dst[Index] = *reinterpret_cast<const FReinClothMatrix3x4*>(&BoneMatrix);
	}
}
#pragma endregion
