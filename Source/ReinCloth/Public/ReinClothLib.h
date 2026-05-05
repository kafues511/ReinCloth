// Copyright © 2026 kafues511 All Rights Reserved.

#pragma once

#include <stdint.h>

enum ReinCloth_BufferType
{
    ReinCloth_Buffer_Positions = 0,
    ReinCloth_Buffer_Velocities,
    ReinCloth_Buffer_Predicts,
    ReinCloth_Buffer_Normals,
    ReinCloth_Buffer_Origins,
    ReinCloth_Buffer_Influences,
    ReinCloth_Buffer_SimTriangles,
    ReinCloth_Buffer_Constraints,
    ReinCloth_Buffer_Embeddeds,
    ReinCloth_Buffer_EmbeddedInfluences,
    ReinCloth_Buffer_RenderTriangles,
    ReinCloth_Buffer_Offsets,
    ReinCloth_Buffer_Neighbors,
    ReinCloth_Buffer_NormalOffsets,
    ReinCloth_Buffer_NormalNeighbors,
};

struct ReinCloth_Vec2f
{
    float X, Y;
};

struct ReinCloth_Vec3f
{
    float X, Y, Z;
};

struct ReinCloth_Vec4f
{
    float X, Y, Z, W;
};

struct ReinCloth_Uint4
{
    uint32_t X, Y, Z, W;
};

struct ReinCloth_Color
{
    uint8_t B, G, R, A;
};

struct ReinCloth_Vertex
{
    ReinCloth_Vec3f Position;
    ReinCloth_Vec3f Normal;
    ReinCloth_Vec2f UV;
    ReinCloth_Color Color;
    ReinCloth_Vec4f LinearColor;
    ReinCloth_Uint4 Bones;
    ReinCloth_Uint4 ExtraBones;
    ReinCloth_Vec4f Weights;
    ReinCloth_Vec4f ExtraWeights;
};

#ifdef __cplusplus
extern "C"
{
#endif

    void* ReinCloth_CreateBuilder(void);

    void ReinCloth_DestroyBuilder(void* Builder);

    int32_t ReinCloth_AddSimulationPiece(
        void* Builder,
        int32_t PieceType,
        int32_t NumVertices,
        const void* InVertices,
        int32_t NumIndices,
        const uint32_t* InIndices,
        int32_t NumTriangles,
        const void* InTriangles,
        int32_t bIsWrap);

    int32_t ReinCloth_SetRenderMesh(
        void* Builder,
        int32_t NumVertices,
        const void* InVertices,
        int32_t NumTriangles,
        const void* InTriangles);

    int32_t ReinCloth_Build(void* Builder, int32_t TargetInfluence, int32_t EmbeddedOffsetMode);

    int32_t ReinCloth_GetResultDesc(
        void* Builder,
        int32_t BufferType,
        int32_t* OutNumElements,
        int32_t* OutElementSize);

    int32_t ReinCloth_CopyResult(
        void* Builder,
        int32_t BufferType,
        void* OutBuffer,
        int32_t OutCapacityBytes);

#ifdef __cplusplus
}
#endif
