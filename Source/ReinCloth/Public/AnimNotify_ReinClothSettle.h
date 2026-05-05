// Copyright © 2026 kafues511 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ReinClothSettle.generated.h"

UCLASS(meta = (DisplayName = "ReinCloth Settle"))
class REINCLOTH_API UAnimNotify_ReinClothSettle : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

public:
	/**
	 * @brief 空回しの回数
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReinCloth", meta = (ClampMin = "0"))
	int32 NumIterations = 10;
};
