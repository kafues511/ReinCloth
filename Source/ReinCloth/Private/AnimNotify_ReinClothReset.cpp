// Copyright © 2026 kafues511 All Rights Reserved.

#include "AnimNotify_ReinClothReset.h"
#include "ReinClothSimulationComponent.h"

void UAnimNotify_ReinClothReset::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	auto Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	auto SimulationComponent = Owner->FindComponentByClass<UReinClothSimulationComponent>();
	if (!IsValid(SimulationComponent))
	{
		return;
	}

	// シミュレーションをリセット
	SimulationComponent->ResetSimulation();
}
