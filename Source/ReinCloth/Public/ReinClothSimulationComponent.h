// Copyright © 2026 kafues511 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ReinClothInstance.h"
#include "ReinClothSimulationComponent.generated.h"

UCLASS(BlueprintType, Blueprintable, ClassGroup = "ReinCloth", meta = (BlueprintSpawnableComponent))
class REINCLOTH_API UReinClothSimulationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UReinClothSimulationComponent(const FObjectInitializer& ObjectInitializer);

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	/**
	 * @brief シミュレーションをリセットする
	 * テレポートした場合など、必要に応じて叩いてください。
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ReinCloth")
	void ResetSimulation();

	/**
	 * @brief シミュレーションを空回しする
	 * アニメ再生直後やテレポート直後に安定した挙動を求めたい場合に。
	 * @param NumIterations 回数を指定
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ReinCloth")
	void SettleSimulation(int32 NumIterations = 10);

	/**
	 * @brief コリジョンの更新
	 * GetAllActorsWithTag(..)を呼ばずに最適化したい場合は、このコンポーネントを継承して、この関数をオーバーライドして書き換えて
	 * @param Instance クロスの設定
	 * @param OutNumCollisions 有効なコリジョンの数
	 * @param OutCollisions コリジョンたち
	 */
	virtual void UpdateCollision(const FReinClothInstance& Instance, uint32& OutNumCollisions, TStaticArray<FReinClothCollision, 512u>& OutCollisions);

public:
	/**
	 * @brief クロスシミュレーションの設定
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReinCloth", meta = (TitleProperty = "UniqueName"))
	TArray<FReinClothInstance> Instances;
};
