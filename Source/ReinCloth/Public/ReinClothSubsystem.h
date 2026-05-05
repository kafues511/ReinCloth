// Copyright © 2026 kafues511 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ReinClothDataAsset.h"
#include "ReinClothSubsystem.generated.h"

class FReinClothViewExtension;

UCLASS()
class REINCLOTH_API UReinClothSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	static UReinClothSubsystem* GetCurrent(const UWorld* World)
	{
		if (IsValid(World))
		{
			return UGameInstance::GetSubsystem<UReinClothSubsystem>(World->GetGameInstance());
		}
		return nullptr;
	}

public:
	/**
	 * @brief GlobalSectionIndexの新規割り当て
	 * @return これ以上シミュレーションするモノを増やせない・登録できない場合は INDEX_NONE を返します。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	int32 AllocationGlobalSectionIndex();

	/**
	 * @brief GlobalSectionIndexの解放
	 * @param GlobalSectionIndex 解放・再利用可能にする GlobalSectionIndex を指定
	 */
	UFUNCTION(BlueprintCallable)
	void ReleaseGlobalSectionIndex(int32 GlobalSectionIndex);

public:
	/**
	 * @brief 共有ボーンキャッシュの登録
	 * @param SkeletalMeshComponent 対象のSkeletalMeshComponent
	 */
	void RegisterSharedBoneCache(USkeletalMeshComponent* SkeletalMeshComponent);

	/**
	 * @brief 共有ボーンキャッシュの解除
	 * @param SkeletalMeshComponent 対象のSkeletalMeshComponent
	 */
	void UnregisterSharedBoneCache(USkeletalMeshComponent* SkeletalMeshComponent);

	/**
	 * @brief ボーンの更新が終わったら呼ばれる
	 */
	void OnBoneTransformsUpdated(TWeakObjectPtr<USkeletalMeshComponent> WeakSkeletalMeshComponent);

public:
	TSharedPtr<FReinClothViewExtension, ESPMode::ThreadSafe> ViewExtension;

	TArray<FReinClothSettings> ClothSettings;

	/**
	 * @brief セクションの管理
	 */
	int32 SectionCounter;

	/**
	 * @brief 空いている・再利用可能なセクション
	 */
	TArray<int32> VacantSections;

	struct FSharedBoneCache
	{
		/**
		 * @brief いくつのクロスがSkeletalMeshComponentを参照しているか
		 */
		int32 RefCount = 0;
		/**
		 * @brief RegisterOnBoneTransformsFinalizedDelegateのHandle
		 */
		FDelegateHandle BoneTransformsFinalizedHandle;
		/**
		 * @brief ボーン行列
		 */
		TArray<FReinClothMatrix3x4> BoneMatrices;
	};

	/**
	 * @brief ボーンの情報
	 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FSharedBoneCache> SharedBoneCacheMap;
};
