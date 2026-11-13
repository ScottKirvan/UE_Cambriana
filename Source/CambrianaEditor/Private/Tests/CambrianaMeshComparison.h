#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

namespace Cambriana::Editor::Tests
{
	// ATS-46, ATS-47: a baked mesh's vertices match the live actor's within 0.001 cm.
	constexpr double VertexTolerance = 0.001;

	struct FMeshTriangle
	{
		FVector3d Corners[3];

		FVector3d Centroid() const { return (Corners[0] + Corners[1] + Corners[2]) / 3.0; }

		/**
		 * Whether two corners are the same point once stored as the static mesh's single-precision positions. The static mesh
		 * build drops such a triangle whatever its build settings (StaticMeshBuilder.cpp, BuildVertexBuffer: "Don't process
		 * degenerate triangles", compared with a zero threshold when bRemoveDegenerates is off); it has no area to render.
		 */
		bool HasCoincidentCorners() const
		{
			const FVector3f A(Corners[0]), B(Corners[1]), C(Corners[2]);
			return A == B || A == C || B == C;
		}
	};

	/** How the triangles of a static mesh's rendered LOD 0 pair off with a live actor's. */
	struct FTriangleMatch
	{
		int32 LiveTriangles = 0;
		/** Live triangles with two corners at one point, which no static mesh renders. */
		int32 LiveCoincident = 0;
		int32 RenderedTriangles = 0;
		/** Other live triangles with no rendered triangle whose corners, in the same winding, are each within the tolerance. */
		int32 Unmatched = 0;
		/** Live vertices with no rendered vertex within the tolerance. */
		int32 UnmatchedVertices = 0;
		/** The first unmatched live triangle, for the test's message. */
		FMeshTriangle FirstUnmatched;

		bool Matches() const
		{
			return LiveTriangles > LiveCoincident && LiveTriangles - LiveCoincident == RenderedTriangles && Unmatched == 0 && UnmatchedVertices == 0;
		}

		FString Describe() const
		{
			FString Text = FString::Printf(TEXT("%d live triangles, %d of them with coincident corners, %d rendered, %d live without a rendered match, %d live vertices without one"),
				LiveTriangles, LiveCoincident, RenderedTriangles, Unmatched, UnmatchedVertices);
			if (Unmatched > 0)
			{
				Text += FString::Printf(TEXT("; first: %s %s %s"), *FirstUnmatched.Corners[0].ToString(), *FirstUnmatched.Corners[1].ToString(),
					*FirstUnmatched.Corners[2].ToString());
			}
			return Text;
		}
	};

	inline TArray<FMeshTriangle> TrianglesOf(const UE::Geometry::FDynamicMesh3& Mesh)
	{
		TArray<FMeshTriangle> Triangles;
		for (const int32 Tid : Mesh.TriangleIndicesItr())
		{
			FMeshTriangle& Triangle = Triangles.AddDefaulted_GetRef();
			Mesh.GetTriVertices(Tid, Triangle.Corners[0], Triangle.Corners[1], Triangle.Corners[2]);
		}
		return Triangles;
	}

	/** The triangles of LOD 0 of the static mesh's built render data: what the engine renders. */
	inline TArray<FMeshTriangle> RenderedTrianglesOf(const UStaticMesh& StaticMesh)
	{
		TArray<FMeshTriangle> Triangles;
		const FStaticMeshRenderData* RenderData = StaticMesh.GetRenderData();
		if (!RenderData || RenderData->LODResources.IsEmpty())
		{
			return Triangles;
		}
		const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
		TArray<uint32> Indices;
		Lod.IndexBuffer.GetCopy(Indices);
		const FPositionVertexBuffer& Positions = Lod.VertexBuffers.PositionVertexBuffer;
		for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
		{
			FMeshTriangle& Triangle = Triangles.AddDefaulted_GetRef();
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				Triangle.Corners[Corner] = FVector3d(Positions.VertexPosition(Indices[Index + Corner]));
			}
		}
		return Triangles;
	}

	inline bool SameTriangle(const FMeshTriangle& A, const FMeshTriangle& B, double Tolerance)
	{
		for (int32 Rotation = 0; Rotation < 3; ++Rotation)
		{
			bool bSame = true;
			for (int32 Corner = 0; Corner < 3 && bSame; ++Corner)
			{
				bSame = FVector3d::Distance(A.Corners[Corner], B.Corners[(Corner + Rotation) % 3]) <= Tolerance;
			}
			if (bSame)
			{
				return true;
			}
		}
		return false;
	}

	/** Finds points within a tolerance of a query point, the tolerance being well under the cell size. */
	class FPointGrid
	{
	public:
		void Add(const FVector3d& Point, int32 Index) { Cells.Add(CellOf(Point), Index); }

		/** Calls Visit with each index added at a point in the cell of Point or a neighbouring cell, until Visit returns true; returns that index. */
		int32 FindNear(const FVector3d& Point, TFunctionRef<bool(int32)> Visit) const
		{
			const FIntVector Cell = CellOf(Point);
			for (int32 X = -1; X <= 1; ++X)
			{
				for (int32 Y = -1; Y <= 1; ++Y)
				{
					for (int32 Z = -1; Z <= 1; ++Z)
					{
						const FIntVector Neighbour = Cell + FIntVector(X, Y, Z);
						for (auto It = Cells.CreateConstKeyIterator(Neighbour); It; ++It)
						{
							if (Visit(It.Value()))
							{
								return It.Value();
							}
						}
					}
				}
			}
			return INDEX_NONE;
		}

	private:
		static FIntVector CellOf(const FVector3d& Point)
		{
			return FIntVector(FMath::FloorToInt32(Point.X), FMath::FloorToInt32(Point.Y), FMath::FloorToInt32(Point.Z));
		}

		TMultiMap<FIntVector, int32> Cells;
	};

	/**
	 * Pairs each live triangle without coincident corners with a distinct rendered triangle having the same corners in the same
	 * winding, within Tolerance, and finds each live vertex among the rendered vertices.
	 */
	inline FTriangleMatch MatchTriangles(const UE::Geometry::FDynamicMesh3& Live, const UStaticMesh& StaticMesh, double Tolerance = VertexTolerance)
	{
		const TArray<FMeshTriangle> LiveTriangles = TrianglesOf(Live);
		const TArray<FMeshTriangle> Rendered = RenderedTrianglesOf(StaticMesh);
		FTriangleMatch Match;
		Match.LiveTriangles = LiveTriangles.Num();
		Match.RenderedTriangles = Rendered.Num();

		FPointGrid Centroids;
		FPointGrid Corners;
		for (int32 Index = 0; Index < Rendered.Num(); ++Index)
		{
			Centroids.Add(Rendered[Index].Centroid(), Index);
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				Corners.Add(Rendered[Index].Corners[Corner], Index * 3 + Corner);
			}
		}

		TBitArray<> Used(false, Rendered.Num());
		for (const FMeshTriangle& Triangle : LiveTriangles)
		{
			if (Triangle.HasCoincidentCorners())
			{
				++Match.LiveCoincident;
				continue;
			}
			const int32 Found = Centroids.FindNear(Triangle.Centroid(),
				[&](int32 Index) { return !Used[Index] && SameTriangle(Triangle, Rendered[Index], Tolerance); });
			if (Found == INDEX_NONE)
			{
				if (Match.Unmatched++ == 0)
				{
					Match.FirstUnmatched = Triangle;
				}
				continue;
			}
			Used[Found] = true;
		}

		for (const int32 Vid : Live.VertexIndicesItr())
		{
			const FVector3d Position = Live.GetVertex(Vid);
			const int32 Found = Corners.FindNear(Position,
				[&](int32 Index) { return FVector3d::Distance(Position, Rendered[Index / 3].Corners[Index % 3]) <= Tolerance; });
			Match.UnmatchedVertices += Found == INDEX_NONE;
		}
		return Match;
	}
}
