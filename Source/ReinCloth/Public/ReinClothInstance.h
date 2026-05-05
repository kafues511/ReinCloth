// Copyright © 2026 kafues511 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReinClothDataAsset.h"
#include "ReinClothInstance.generated.h"

class USkeletalMeshComponent;
class UTextureRenderTarget2D;

class UReinClothSubsystem;

USTRUCT(BlueprintType, Category = "ReinCloth")
struct FReinClothInstance
{
	GENERATED_BODY()

public:
	/**
	 * @brief メッシュのタグ名
	 * クロスシミュを適用するスケルタルメッシュコンポーネントに付与されているタグ名を指定
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	FName MeshTag = NAME_None;

	/**
	 * @brief コリジョンのタグ名
	 * タグ名を元にレベルから UReinClothCollisionComponent を探索します。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	FName CollisionTag = TEXT("rein collision");

	/**
	 * @brief クロスのユニーク名
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	FName UniqueName = NAME_None;

	/**
	 * @brief クロスのアセット
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (DisplayThumbnail = true))
	UReinClothDataAsset* ClothAsset = nullptr;

public:
	/**
	 * @brief シミュレーションの有効性
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	bool bIsSimulation = true;

	/**
	 * @brief シミュレーションメッシュを可視化するか
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	bool bIsVisualizeSimulationMesh = false;

	/**
	 * @brief レンダーメッシュを可視化するか
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	bool bIsVisualizeRenderMesh = false;

	/**
	 * @brief サブステップ数
	 * イテレーションの1回で固定です。
	 * 回数を上げ過ぎると固くなりますが、PBDの計算仕様のため正常です。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	int32 NumSubsteps = 5;

	/**
	 * @brief 重力
	 * ワールド空間で指定してください。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	FVector3f Gravity = FVector3f(0.0f, 0.0f, -100.0f);

	/**
	 * @brief 速度減衰
	 * 微動が起きる場合は強度を少し削って減衰させた方が安定する
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityDamping = 0.99f;

	/**
	 * @brief サブステップあたりに許容する変位量
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float MaxDisplacement = 100.0f;

	/**
	 * @brief 構成バネの垂直方向の硬さ
	 * 値が小さいほど柔らかい、値が大きいほど硬い
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (ClampMin = "1"))
	int32 StructuralVerticalStiffness = 1000000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_StructuralHorizontalStiffness : 1;

	/**
	 * @brief 構成バネの水平方向の硬さ
	 * 値が小さいほど柔らかい、値が大きいほど硬い
	 * 左のチェックを入れない場合・Overrideしない場合は StructuralVerticalStiffness を採用します。
	 * スカートは水平方向の硬さを和らげると「ふわっと」するかも。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (ClampMin = "1", EditCondition = "bOverride_StructuralHorizontalStiffness"))
	int32 StructuralHorizontalStiffness = 1000000;

	/**
	 * @brief せん断バネの硬さ
	 * 値が小さいほど柔らかい、値が大きいほど硬い
	 * 斜め方向に結ぶから垂直と水平の区分は無し
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (ClampMin = "1"))
	int32 ShearStiffness = 1000000;

	/**
	 * @brief 曲げバネの垂直方向の硬さ
	 * 値が小さいほど柔らかい、値が大きいほど硬い
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (ClampMin = "1"))
	int32 BendingVerticalStiffness = 1000000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_BendingHorizontalStiffness : 1;

	/**
	 * @brief 曲げバネの水平方向の硬さ
	 * 値が小さいほど柔らかい、値が大きいほど硬い
	 * 左のチェックを入れない場合・Overrideしない場合は BendingVerticalStiffness を採用します。
	 * スカートは水平方向の硬さを和らげると「ふわっと」するかも。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (ClampMin = "1", EditCondition = "bOverride_BendingHorizontalStiffness"))
	int32 BendingHorizontalStiffness = 1000000;

	/**
	 * @brief コリジョンのモード
	 * ソフトとハードを両方選択した場合は、両方計算されます。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance", meta = (Bitmask, BitmaskEnum = "/Script/ReinCloth.EReinClothCollisionMode"))
	int32 CollisionMode = static_cast<int32>(EReinClothCollisionMode::HardCollision);

public:
	/**
	 * @brief 有効性
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Instance")
	bool bIsValid = false;

	/**
	 * @brief GlobalSectionIndex
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Instance")
	int32 GlobalSectionIndex = INDEX_NONE;

public:
	/**
	 * @brief RegisterSharedBoneCacheが済んでいるか
	 */
	bool bIsRegisteredSharedBoneCache = false;

	/**
	 * @brief MIDのリソース更新が要求されているか
	 */
	bool bIsRequestMID = false;

	/**
	 * @brief シミュレーションをリセットするか
	 * 初回フレームやテレポートした場合など
	 */
	bool bIsResetSimulation = false;

	/**
	 * @brief 空回しの回数
	 * 初回フレームやテレポート後に違和感を緩和させるために空回しをする場合など
	 */
	int32 NumSettleIterations = 0;

	/**
	 * @brief クロスのユニーク名と一致する設定が見つかった位置・インデックス
	 */
	int32 SectionIndex = INDEX_NONE;

	/**
	 * @brief クロスの変位を書き込むテクスチャのサイズ
	 */
	int32 GridSize = INDEX_NONE;

	/**
	 * @brief シミュレーション結果を反映する対象のスケルタルメッシュコンポーネント
	 * note: Transientを付与するとDetailsからパラメタ編集する際に消失して面倒だから意図的に外してる
	 */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = nullptr;

	/**
	 * @brief アニメーションを参照する対象のスケルタルメッシュコンポーネント
	 */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> RootSkeletalMeshComponent = nullptr;

	/**
	 * @brief シミュレーション結果の頂点変位
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UTextureRenderTarget2D> RT_Position = nullptr;

	/**
	 * @brief シミュレーション結果の法線変位
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UTextureRenderTarget2D> RT_Normal = nullptr;

public:
	/**
	 * @brief 初期化処理
	 * @param ReinClothSubsystem クロスのサブシステム
	 * @param Owner クロスの所有者
	 * @return 初期化に失敗した場合は false を返します。
	 */
	bool Initialize(UReinClothSubsystem* ReinClothSubsystem, const AActor* Owner);

	/**
	 * @brief 解放処理
	 * @param ReinClothSubsystem クロスのサブシステム
	 * @return 解放に失敗した場合は false を返します。
	 */
	bool Release(UReinClothSubsystem* ReinClothSubsystem);

	/**
	 * @brief 基本的な更新処理
	 * @param ReinClothSubsystem クロスのサブシステム
	 * @return 更新に失敗した場合は false を返します。
	 */
	bool Update(UReinClothSubsystem* ReinClothSubsystem);

	/**
	 * @brief 構造バネ（垂直）のコンプライアンスを取得
	 * @return コンプライアンス
	 */
	float GetStructuralVerticalCompliance() const
	{
		return 1.0f / static_cast<float>(FMath::Max(1, StructuralVerticalStiffness));
	}

	/**
	 * @brief 構造バネ（水平）のコンプライアンスを取得
	 * @return コンプライアンス
	 */
	float GetStructuralHorizontalCompliance() const
	{
		return 1.0f / static_cast<float>(FMath::Max(1, bOverride_StructuralHorizontalStiffness ? StructuralHorizontalStiffness : StructuralVerticalStiffness));
	}

	/**
	 * @brief せん断バネのコンプライアンスを取得
	 * @return コンプライアンス
	 */
	float GetShearCompliance() const
	{
		return 1.0f / static_cast<float>(FMath::Max(1, ShearStiffness));
	}

	/**
	 * @brief 曲げバネ（垂直）のコンプライアンスを取得
	 * @return コンプライアンス
	 */
	float GetBendingVerticalCompliance() const
	{
		return 1.0f / static_cast<float>(FMath::Max(1, BendingVerticalStiffness));
	}

	/**
	 * @brief 曲げバネ（水平）のコンプライアンスを取得
	 * @return コンプライアンス
	 */
	float GetBendingHorizontalCompliance() const
	{
		return 1.0f / static_cast<float>(FMath::Max(1, bOverride_BendingHorizontalStiffness ? BendingHorizontalStiffness : BendingVerticalStiffness));
	}
};
