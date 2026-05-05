// Copyright © 2026 kafues511 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ReinClothCollisionComponent.generated.h"

UCLASS(BlueprintType, Blueprintable, ClassGroup = "ReinCloth", meta = (BlueprintSpawnableComponent))
class REINCLOTH_API UReinClothCollisionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UReinClothCollisionComponent(const FObjectInitializer& ObjectInitializer);

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	/**
	 * @brief コリジョンのSRTをセット
	 * @param Transform SRT
	 */
	UFUNCTION(BlueprintCallable, Category = "ReinCloth")
	void SetTransform(const FTransform& Transform);

public:
	/**
	 * @brief コリジョンのタグ名
	 * プレイ中に変更しても正しく反映されません。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReinCloth")
	TArray<FName> CollisionTags;

	/**
	 * @brief 形状の位置
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReinCloth")
	FVector3f Translation;

	/**
	 * @brief 形状の向き
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReinCloth")
	FQuat4f Rotation;

	/**
	 * @brief 形状の大きさ
	 * Xが半径、Zが高さ
	 * Yは該当なし
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReinCloth")
	FVector3f Scale;

	/**
	 * @brief 摩擦
	 * 0.0はツルツル、1.0は引っ付く
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReinCloth", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Friction = 0.5f;

	/**
	 * @brief 形状の可視化
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReinCloth")
	bool bIsVisualize;

	/**
	 * @brief 形状の色
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReinCloth", meta = (EditCondition = "bIsVisualize"))
	FColor VisualizeColor = FColor::Red;

	/**
	 * @brief 形状の描画優先度
	 * 値が高いほど深度を無視して最前面に描画されます。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReinCloth", meta = (EditCondition = "bIsVisualize", ClampMin = "0", ClampMax = "255"))
	int32 VisualizeDepthPriority;
};
