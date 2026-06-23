// Copyright © 2026 kafues511 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ReinClothDataAsset.generated.h"

#define REIN_CLOTH_CONSTRAIN_TYPES_STRUCTURAL_VERTICAL (0)		// 構造バネ（垂直）
#define REIN_CLOTH_CONSTRAIN_TYPES_STRUCTURAL_HORIZONTAL (1)	// 構造バネ（水平）
#define REIN_CLOTH_CONSTRAIN_TYPES_SHEAR (2)					// せん断バネ
#define REIN_CLOTH_CONSTRAIN_TYPES_BENDING_VERTICAL (3)			// 曲げバネ（垂直）
#define REIN_CLOTH_CONSTRAIN_TYPES_BENDING_HORIZONTAL (4)		// 曲げバネ（水平）
#define REIN_CLOTH_CONSTRAIN_TYPES_BENDING_EXTRA (5)			// 異なるパーツ間の曲げバネ

class FTextureResource;
class USkeletalMesh;

class UReinClothDataAsset;

struct FReinClothMatrix3x4
{
	FVector4f M00;
	FVector4f M10;
	FVector4f M20;
};

struct FReinClothCollision
{
	FVector3f CapsuleStart;
	FVector3f CapsuleEnd;
	float Radius;
	float Friction;
};

/**
 * @brief コリジョンの反映方法
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EReinClothCollisionMode : uint8
{
	None = 0x0 UMETA(Hidden),
	/**
	 * @brief コリジョン使わない
	 * コリジョンを使用しない。
	 * 最も軽量です。
	 */
	Disable = 1 << 0 UMETA(DisplayName = "Disable\nコリジョン使わない"),
	/**
	 * @brief ソフトコリジョン
	 * 衝突した頂点をカプセルの表面に押し出します。
	 * すり抜け防止は弱いですが、微動が抑えられます。
	 */
	SoftCollision = 1 << 1 UMETA(DisplayName = "Soft Collision"),
	/**
	 * @brief ハードコリジョン
	 * 衝突した頂点をカプセルの表面に強制的に移動します。
	 * すり抜け防止は強いですが、微動することがあります。
	 */
	HardCollision = 1 << 2 UMETA(DisplayName = "Hard Collision"),
};
ENUM_CLASS_FLAGS(EReinClothCollisionMode);

struct FReinClothSettings
{
	/**
	 * @brief シミュレーションの有効性
	 */
	bool bIsSimulation = false;

	/**
	 * @brief シミュレーションをリセットするか
	 */
	bool bIsResetSimulation = false;

	/**
	 * @brief シミュレーションメッシュを可視化するか
	 */
	bool bIsVisualizeSimulationMesh = false;

	/**
	 * @brief レンダーメッシュを可視化するか
	 */
	bool bIsVisualizeRenderMesh = false;

	/**
	 * @brief サブステップ数
	 */
	int32 NumSubsteps = 5;

	/**
	 * @brief 空回しの回数
	 */
	int32 NumSettleIterations = 0;

	/**
	 * @brief 速度減衰
	 */
	float VelocityDamping = 0.99f;

	/**
	 * @brief アニメーション変位の反映率
	 */
	float AnimDeltaScale = 0.0f;

	/**
	 * @brief 重力
	 */
	FVector3f Gravity = FVector3f(0.0f, 0.0f, -100.0f);

	/**
	 * @brief サブステップあたりに許容する変位量
	 */
	float MaxDisplacement = 100.0f;

	/**
	 * @brief 構造バネのコンプライアンス
	 */
	float StructuralVerticalCompliance = 1.0f;

	/**
	 * @brief 構造バネのコンプライアンス
	 */
	float StructuralHorizontalCompliance = 1.0f;

	/**
	 * @brief せん断バネのコンプライアンス
	 */
	float ShearCompliance = 1.0f;

	/**
	 * @brief 曲げバネのコンプライアンス
	 */
	float BendingVerticalCompliance = 1.0f;

	/**
	 * @brief 曲げバネのコンプライアンス
	 */
	float BendingHorizontalCompliance = 1.0f;

	/**
	 * @brief LocalToWorld行列
	 */
	FMatrix44f RenderMatrix = FMatrix44f::Identity;

	/**
	 * @brief シミュレーション空間の原点
	 */
	FVector3f SimulationOrigin = FVector3f::ZeroVector;

	/**
	 * @brief ボーン行列
	 */
	TArray<FReinClothMatrix3x4> BoneMatrices;

	/**
	 * @brief コリジョンの反映方法
	 */
	EReinClothCollisionMode CollisionMode = EReinClothCollisionMode::None;

	/**
	 * @brief 有効なコリジョン数
	 */
	uint32 NumCollisions = 0u;
	/**
	 * @brief コリジョン
	 */
	TStaticArray<FReinClothCollision, 512u> Collisions;

	/**
	 * @brief シミュレーション結果の頂点変位
	 */
	FTextureResource* PositionResource = nullptr;

	/**
	 * @brief シミュレーション結果の法線変位
	 */
	FTextureResource* NormalResource = nullptr;

	/**
	 * @brief クロスのユニーク名と一致する設定が見つかった位置・インデックス
	 */
	int32 SectionIndex = INDEX_NONE;

	/**
	 * @brief クロスのアセット
	 */
	TWeakObjectPtr<UReinClothDataAsset> WeakClothAsset = nullptr;

	/**
	 * @brief ボーンを参照するためのキーとして利用するスケルタルメッシュコンポーネント
	 */
	TWeakObjectPtr<USkeletalMeshComponent> WeakSkeletalMeshComponent = nullptr;
};

#pragma region Serialize
USTRUCT()
struct FReinClothPosition
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FVector3f Position = FVector3f::Zero();
	UPROPERTY()
	float InvMass = 0.0f;
};

USTRUCT()
struct FReinClothVelocity
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FVector3f Velocity = FVector3f::Zero();
	UPROPERTY()
	float Pad = 0.0f;
};

USTRUCT()
struct FReinClothPredict
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FVector3f Predict = FVector3f::Zero();
	UPROPERTY()
	float InvMass = 0.0f;
};

USTRUCT()
struct FReinClothNormal
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FVector3f Normal = FVector3f::Zero();
	UPROPERTY()
	float Pad = 0.0f;
};

USTRUCT()
struct FReinClothOrigin
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FVector3f Origin = FVector3f::Zero();
	UPROPERTY()
	float Pad = 0.0f;
};

USTRUCT()
struct FReinClothInfluence
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FUintVector4 Bones = FUintVector4::ZeroValue;
	UPROPERTY()
	FVector4f Weights = FVector4f::Zero();
	UPROPERTY()
	FUintVector4 ExtraBones = FUintVector4::ZeroValue;
	UPROPERTY()
	FVector4f ExtraWeights = FVector4f::Zero();
};

USTRUCT()
struct FReinClothConstraint
{
	GENERATED_BODY()

public:
	UPROPERTY()
	uint32 Type = 0u;
	UPROPERTY()
	FUintVector2 Ids = FUintVector2::ZeroValue;
	UPROPERTY()
	float EdgeLength = 0.0f;

public:
	bool operator == (const FReinClothConstraint& InOther) const
	{
		return (Ids[0] == InOther.Ids[0] && Ids[1] == InOther.Ids[1]) || (Ids[1] == InOther.Ids[0] && Ids[0] == InOther.Ids[1]);
	}
};

USTRUCT()
struct FReinClothEmbedded
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FUintVector Ids = FUintVector::ZeroValue;
	UPROPERTY()
	FVector3f Weights = FVector3f::Zero();
	UPROPERTY()
	FVector3f Offset = FVector3f::Zero();
	UPROPERTY()
	FUintVector2 UV = FUintVector2::ZeroValue;
	UPROPERTY()
	float Pad = 0.0f;
};

enum class EReinClothSetupFlags : uint8
{
	None = 0x0,
	/**
	 * @brief セットアップに失敗
	 */
	Failed = 0x1,
	/**
	 * @brief 新しくセットアップした
	 */
	Newly = 0x2,
	/**
	 * @brief セットアップ済み
	 */
	Already = 0x4,
};
ENUM_CLASS_FLAGS(EReinClothSetupFlags);
#pragma endregion

UENUM(BlueprintType, Category = "ReinCloth")
enum class FReinClothPieceTypes : uint8
{
	/**
	 * @brief メインのパーツ
	 */
	Main UMETA(DisplayName = "Main\nメイン"),
	/**
	 * @brief メインのパーツに付ける袖パーツ
	 * 例として衣類の腕、袖部分
	 * あんまり使ってないから動作確認できてないから一旦使用禁止
	 */
	Sleeve UMETA(Hidden, DisplayName = "Sleeve\n袖"),
};

USTRUCT(BlueprintType, Category = "ReinCloth")
struct FReinClothPiece
{
	GENERATED_BODY()

public:
	/**
	 * @brief パーツの種類
	 */
	UPROPERTY(EditAnywhere, Category = "Piece", meta = (DisplayName = "Type\nパーツの種類"))
	FReinClothPieceTypes Type = FReinClothPieceTypes::Main;

	/**
	 * @brief クロスシミュを計算するスロット名
	 * SkeletalSimulationMeshAssetから指定します。
	 */
	UPROPERTY(EditAnywhere, Category = "Piece", meta = (DisplayName = "Simulation Slot Name\nクロスシミュを計算するスロット名"))
	FName SimulationSlotName = NAME_None;

public:
	UPROPERTY()
	int32 SimulationSection = INDEX_NONE;
};

UENUM(BlueprintType, Category = "ReinCloth")
enum class EReinClothTargetInfluence : uint8
{
	RenderMesh UMETA(DisplayName = "Render Mesh\n描画メッシュのボーンの影響を受ける"),
	SimulationMesh UMETA(DisplayName = "Simulation Mesh\nシミュレーションメッシュのボーンの影響を受ける"),
};

UENUM(BlueprintType, Category = "ReinCloth")
enum class EReinClothEmbeddedOffsetMode : uint8
{
	/**
	 * @brief 厚みのないメッシュの復元に強いです。
	 * 厚みがあるメッシュの場合は、TBNを使わないとペッタンコになりやすいです。
	 */
	Rest UMETA(DisplayName = "Rest\nシンプルなオフセット方式"),
	/**
	 * @brief 厚みのあるメッシュの復元に強いです。
	 * シミュレーションメッシュの作り方が雑（例えばレンダーメッシュから離れている）だと復元精度が極端に悪化します。
	 * 大雑把に髪の毛を揺らしたい場合はRestの方が適しています。
	 */
	TBN UMETA(DisplayName = "TBN\nTBN方式"),
};

USTRUCT(BlueprintType, Category = "ReinCloth")
struct FReinClothSimMeshSection
{
	GENERATED_BODY()

public:
	/**
	 * @brief クロスの有効性
	 * セットアップに失敗するとチェックが付きません。
	 */
	UPROPERTY(VisibleAnywhere, Category = "ClothSection", meta = (DisplayName = "Is Valid\nクロスの有効性"))
	bool bIsValid = false;

	/**
	 * @brief クロスのユニーク名
	 */
	UPROPERTY(EditAnywhere, Category = "ClothSection", meta = (DisplayName = "Unique Name\nクロスのユニーク名"))
	FName UniqueName = NAME_None;

	/**
	 * @brief クロスシミュを適用するスロット名
	 * SkeletalRenderMeshAssetから指定します。
	 */
	UPROPERTY(EditAnywhere, Category = "ClothSection", meta = (DisplayName = "Render Slot Name\nクロスシミュを適用するスロット名"))
	FName RenderSlotName = NAME_None;

	/**
	 * @brief 採用するボーンの影響を選択
	 */
	UPROPERTY(EditAnywhere, Category = "ClothSection", meta = (DisplayName = "Target Influence\n採用するボーンの種類"))
	EReinClothTargetInfluence TargetInfluence = EReinClothTargetInfluence::RenderMesh;

	/**
	 * @brief 埋め込み方式
	 */
	UPROPERTY(EditAnywhere, Category = "ClothSection", meta = (DisplayName = "Embedded Mode\n埋め込み方式"))
	EReinClothEmbeddedOffsetMode EmbeddedOffsetMode = EReinClothEmbeddedOffsetMode::Rest;

	/**
	 * @brief UVの端っこを接続して1枚の布として扱うか
	 * 端っこの頂点が近い場合のみ接続されるため、離れていると繋がらないことがありますが設計仕様です。
	 * スカートは必須です。
	 * 制服の前を開けておく場合はチェックを外した方がいいです。
	 */
	UPROPERTY(EditAnywhere, Category = "ClothSection", meta = (DisplayName = "Is Wrap\n端っこを繋げて1枚の布として扱うか"))
	bool bIsWrap = true;

	/**
	 * @brief クロスメッシュの組み合わせ・構成
	 * 例:
	 * シャツなら、メインと左腕と右腕の3つのパーツ構成
	 * スカートなら、メインの1つのパーツで構成
	 * バスタオルなら、メインの1つのパーツ構成
	 */
	UPROPERTY(EditAnywhere, Category = "ClothSection", meta = (DisplayName = "Pieces\n構成パーツ", TitleProperty = "SimulationSlotName"))
	TArray<FReinClothPiece> Pieces;

#if WITH_EDITORONLY_DATA
	/**
	 * @brief クロスメッシュの頂点数
	 * 1,024未満に収めてください。
	 */
	UPROPERTY(VisibleAnywhere, Category = "ClothSection", meta = (DisplayName = "Num Positions\nクロスメッシュの頂点数"))
	int32 NumPositions = INDEX_NONE;
#endif  // WITH_EDITORONLY_DATA

	/**
	 * @brief クロスの変位を書き込むテクスチャのサイズ
	 */
	UPROPERTY(VisibleAnywhere, Category = "ClothSection")
	int32 GridSize = INDEX_NONE;

	/**
	 * @brief 識別
	 */
	UPROPERTY(VisibleAnywhere, Category = "ClothSection")
	FString Guid;

public:
	UPROPERTY()
	TArray<FReinClothPosition> Positions;
	UPROPERTY()
	TArray<FReinClothVelocity> Velocities;
	UPROPERTY()
	TArray<FReinClothPredict> Predicts;
	UPROPERTY()
	TArray<FReinClothNormal> Normals;
	UPROPERTY()
	TArray<FReinClothOrigin> Origins;
	UPROPERTY()
	TArray<FReinClothInfluence> Influences;
	UPROPERTY()
	TArray<uint32> NormalOffsets;
	UPROPERTY()
	TArray<uint32> NormalNeighbors;
	UPROPERTY()
	TArray<FReinClothEmbedded> Embeddeds;
	UPROPERTY()
	TArray<FReinClothInfluence> EmbeddedInfluences;
	UPROPERTY()
	TArray<uint32> Offsets;
	UPROPERTY()
	TArray<uint32> Neighbors;
	UPROPERTY()
	TArray<FReinClothConstraint> Constraints;
	UPROPERTY()
	TArray<FUintVector4> SimTriangles;
	UPROPERTY()
	TArray<FUintVector4> RenderTriangles;
	UPROPERTY()
	int32 BaseVertex = INDEX_NONE;
	UPROPERTY()
	int32 RenderSection = INDEX_NONE;

public:
	EReinClothSetupFlags Setup(const USkeletalMesh* SkeletalRenderMeshAsset, const USkeletalMesh* SkeletalSimulationMeshAsset, int32 ClothUVChannel, float InGridSize, bool bIsForce, FString& OutMessage);
};

UCLASS(BlueprintType, Category = "ReinCloth", meta = (DisplayName = "Rein Cloth Asset"))
class REINCLOTH_API UReinClothDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	/**
	 * @brief クロスを適用するスケルタルメッシュ
	 */
	UPROPERTY(EditAnywhere, Category = "ClothAsset", meta = (DisplayName = "Skeletal Render Mesh\nクロスを適用するスケルタルメッシュ"))
	USkeletalMesh* SkeletalRenderMeshAsset;

	/**
	 * @brief クロスを計算するスケルタルメッシュ
	 */
	UPROPERTY(EditAnywhere, Category = "ClothAsset", meta = (DisplayName = "Skeletal Simulation Mesh\nクロスを計算するスケルタルメッシュ"))
	USkeletalMesh* SkeletalSimulationMeshAsset;
#endif  // WITH_EDITORONLY_DATA

	/**
	 * @brief クロスの計算結果の格納に利用するUVチャンネル
	 * クロスの計算結果をテクスチャに書き込む都合上、頂点ごとにユニークなUVを要求します。
	 */
	UPROPERTY(EditAnywhere, Category = "ClothAsset", meta = (DisplayName = "Cloth UV Channel\nクロスの計算結果の格納に利用するUVチャンネル", UIMin = "0", UIMax = "7", ClampMin = "0", ClampMax = "7"))
	int32 ClothUVChannel = 1;

#if WITH_EDITORONLY_DATA
	/**
	 * @brief エラーメッセージなど
	 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "ClothAsset", meta = (DisplayName = "Message\nエラーメッセージ等"))
	FString Message;
#endif  // WITH_EDITORONLY_DATA

	/**
	 * @brief クロスの設定
	 */
	UPROPERTY(EditAnywhere, Category = "ClothAsset", meta = (DisplayName = "Sections\nクロスの設定", TitleProperty = "UniqueName"))
	TArray<FReinClothSimMeshSection> Sections;

#if WITH_EDITORONLY_DATA
	/**
	 * @brief 識別
	 */
	UPROPERTY(VisibleAnywhere, Category = "ClothAsset")
	FString Guid;
#endif  // WITH_EDITORONLY_DATA

public:
	/**
	 * @brief 更新が必要なものだけセットアップ
	 */
	UFUNCTION(BlueprintCallable, Category = "ReinCloth", meta = (DisplayName = "Setup\nセットアップ", CallInEditor = "true"))
	void Setup();

	/**
	 * @brief 更新済みでも強引にすべてをセットアップ
	 */
	UFUNCTION(BlueprintCallable, Category = "ReinCloth", meta = (DisplayName = "Setup Force\n強制セットアップ", CallInEditor = "true"))
	void SetupForce();

public:
	/**
	 * @brief セットアップ処理
	 * @param bIsForce 入力情報が最後のSetupから変わっていなくてもセットアップ処理を強制的に行うか
	 */
	void Setup_Impl(bool bIsForce);

	/**
	 * @brief クロスの書き込み先のUVをセットアップ
	 * @param bIsForce 強制的にセットアップするか
	 * @param OutMessage メッセージ出力
	 * @return セットアップの結果
	 */
	EReinClothSetupFlags SetupClothUV(bool bIsForce, FString& OutMessage);

	/**
	 * @brief クロスのユニーク名と一致する設定を探して、見つかったらその位置・インデックスを返す
	 * @param UniqueName クロスのユニーク名
	 * @return 見つからない場合は INDEX_NONE を返します。
	 */
	int32 FindSectionIndex(FName UniqueName) const
	{
		for (int32 Index = 0; Index < Sections.Num(); ++Index)
		{
			const auto& Section = Sections[Index];
			if (Section.bIsValid && Section.UniqueName == UniqueName)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
};
