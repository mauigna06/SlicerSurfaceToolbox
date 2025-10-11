// VTK includes
#include <vtkCellArray.h>
#include <vtkCleanPolyData.h>
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkGeneralTransform.h>
#include <vtkIdList.h>
#include <vtkIntArray.h>
#include <vtkMath.h>
#include <vtkMRMLNode.h>
#include <vtkMRMLTransformableNode.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkStaticCellLocator.h>
#include <vtkStringArray.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>

// STD includes
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

constexpr double REMESH_SPLIT_FACTOR = 4.0 / 3.0;
constexpr double REMESH_COLLAPSE_FACTOR = 4.0 / 5.0;

struct EdgeKey
{
  EdgeKey() = default;
  EdgeKey(vtkIdType a, vtkIdType b)
  {
    if (a < b)
      {
      this->V0 = a;
      this->V1 = b;
      }
    else
      {
      this->V0 = b;
      this->V1 = a;
      }
  }
  vtkIdType V0{-1};
  vtkIdType V1{-1};
  bool operator==(const EdgeKey& other) const
  {
    return this->V0 == other.V0 && this->V1 == other.V1;
  }
  struct Hash
  {
    std::size_t operator()(const EdgeKey& key) const
    {
      return std::hash<vtkIdType>()(key.V0) ^ (std::hash<vtkIdType>()(key.V1) << 1);
    }
  };
};

struct EdgeFaceConnectivity
{
  vtkIdType FaceId{-1};
  vtkIdType OppositeVertex{-1};
  bool Forward{true};
};

struct Vertex
{
  std::array<double, 3> Position{ { 0.0, 0.0, 0.0 } };
  bool Active{ true };
  bool Boundary{ false };
};

struct Face
{
  std::array<vtkIdType, 3> Vertices{ { -1, -1, -1 } };
  bool Active{ true };
};

class RemeshMesh
{
public:
  bool Initialize(vtkPolyData* polyData);
  double AverageEdgeLength() const;
  void SplitLongEdges(double maximumLength, bool preserveBoundary);
  void CollapseShortEdges(double minimumLength, bool preserveBoundary);
  void FlipEdges(bool preserveBoundary);
  void TangentialRelaxation(double relaxation, bool preserveBoundary);
  void ProjectToSurface(vtkStaticCellLocator* locator);
  vtkSmartPointer<vtkPolyData> ToPolyData() const;

private:
  void BuildConnectivity();
  void RemoveInactiveElements();
  bool SplitEdge(const EdgeKey& key, const std::vector<EdgeFaceConnectivity>& adjacency, bool preserveBoundary);
  bool CollapseEdge(const EdgeKey& key, bool preserveBoundary);
  bool FlipEdgeInternal(const EdgeKey& key, const std::vector<EdgeFaceConnectivity>& adjacency, bool preserveBoundary);
  double EdgeLength(vtkIdType v0, vtkIdType v1) const;

  std::vector<Vertex> Vertices;
  std::vector<Face> Faces;
  std::unordered_map<EdgeKey, std::vector<EdgeFaceConnectivity>, EdgeKey::Hash> EdgeToFaces;
  std::vector<std::set<vtkIdType> > VertexNeighbors;
};

//----------------------------------------------------------------------------
bool RemeshMesh::Initialize(vtkPolyData* polyData)
{
  if (!polyData)
    {
    return false;
    }

  vtkNew<vtkCleanPolyData> cleaner;
  cleaner->SetInputData(polyData);
  cleaner->Update();

  vtkNew<vtkTriangleFilter> triangulator;
  triangulator->SetInputConnection(cleaner->GetOutputPort());
  triangulator->PassVertsOff();
  triangulator->PassLinesOff();
  triangulator->Update();

  vtkPolyData* cleanedPoly = triangulator->GetOutput();
  if (!cleanedPoly || cleanedPoly->GetNumberOfPolys() == 0)
    {
    return false;
    }

  this->Vertices.clear();
  this->Faces.clear();

  vtkPoints* points = cleanedPoly->GetPoints();
  vtkIdType numberOfPoints = points->GetNumberOfPoints();
  this->Vertices.resize(numberOfPoints);
  for (vtkIdType pointId = 0; pointId < numberOfPoints; ++pointId)
    {
    points->GetPoint(pointId, this->Vertices[pointId].Position.data());
    this->Vertices[pointId].Active = true;
    this->Vertices[pointId].Boundary = false;
    }

  vtkCellArray* polys = cleanedPoly->GetPolys();
  polys->InitTraversal();
  vtkNew<vtkIdList> idList;
  while (polys->GetNextCell(idList.GetPointer()))
    {
    if (idList->GetNumberOfIds() != 3)
      {
      continue;
      }
    Face face;
    face.Vertices[0] = idList->GetId(0);
    face.Vertices[1] = idList->GetId(1);
    face.Vertices[2] = idList->GetId(2);
    face.Active = true;
    this->Faces.push_back(face);
    }

  this->BuildConnectivity();
  return true;
}

//----------------------------------------------------------------------------
double RemeshMesh::AverageEdgeLength() const
{
  double totalLength = 0.0;
  vtkIdType edgeCount = 0;
  for (const auto& edgeEntry : this->EdgeToFaces)
    {
    const EdgeKey& key = edgeEntry.first;
    totalLength += this->EdgeLength(key.V0, key.V1);
    ++edgeCount;
    }
  if (edgeCount == 0)
    {
    return 0.0;
    }
  return totalLength / static_cast<double>(edgeCount);
}

//----------------------------------------------------------------------------
void RemeshMesh::SplitLongEdges(double maximumLength, bool preserveBoundary)
{
  if (maximumLength <= 0.0)
    {
    return;
    }

  std::vector<std::pair<EdgeKey, std::vector<EdgeFaceConnectivity> > > edgesToSplit;
  edgesToSplit.reserve(this->EdgeToFaces.size());
  for (const auto& entry : this->EdgeToFaces)
    {
    const EdgeKey& key = entry.first;
    double length = this->EdgeLength(key.V0, key.V1);
    if (length > maximumLength)
      {
      edgesToSplit.emplace_back(key, entry.second);
      }
    }

  bool splitPerformed = false;
  for (const auto& splitEntry : edgesToSplit)
    {
    splitPerformed |= this->SplitEdge(splitEntry.first, splitEntry.second, preserveBoundary);
    }

  if (splitPerformed)
    {
    this->RemoveInactiveElements();
    this->BuildConnectivity();
    }
}

//----------------------------------------------------------------------------
void RemeshMesh::CollapseShortEdges(double minimumLength, bool preserveBoundary)
{
  if (minimumLength <= 0.0)
    {
    return;
    }

  std::vector<EdgeKey> edgesToCollapse;
  edgesToCollapse.reserve(this->EdgeToFaces.size());
  for (const auto& entry : this->EdgeToFaces)
    {
    const EdgeKey& key = entry.first;
    double length = this->EdgeLength(key.V0, key.V1);
    if (length < minimumLength)
      {
      edgesToCollapse.push_back(key);
      }
    }

  bool collapsePerformed = false;
  for (const EdgeKey& key : edgesToCollapse)
    {
    collapsePerformed |= this->CollapseEdge(key, preserveBoundary);
    }

  if (collapsePerformed)
    {
    this->RemoveInactiveElements();
    this->BuildConnectivity();
    }
}

//----------------------------------------------------------------------------
void RemeshMesh::FlipEdges(bool preserveBoundary)
{
  bool anyFlip = false;
  std::vector<std::pair<EdgeKey, std::vector<EdgeFaceConnectivity> > > interiorEdges;
  interiorEdges.reserve(this->EdgeToFaces.size());
  for (const auto& entry : this->EdgeToFaces)
    {
    if (entry.second.size() != 2)
      {
      continue;
      }
    interiorEdges.emplace_back(entry.first, entry.second);
    }

  for (const auto& edgeEntry : interiorEdges)
    {
    anyFlip |= this->FlipEdgeInternal(edgeEntry.first, edgeEntry.second, preserveBoundary);
    }

  if (anyFlip)
    {
    this->BuildConnectivity();
    }
}

//----------------------------------------------------------------------------
void RemeshMesh::TangentialRelaxation(double relaxation, bool preserveBoundary)
{
  if (relaxation <= 0.0)
    {
    return;
    }

  std::vector<std::array<double, 3> > normals(this->Vertices.size(), { { 0.0, 0.0, 0.0 } });

  for (const Face& face : this->Faces)
    {
    if (!face.Active)
      {
      continue;
      }
    vtkIdType v0 = face.Vertices[0];
    vtkIdType v1 = face.Vertices[1];
    vtkIdType v2 = face.Vertices[2];
    const std::array<double, 3>& p0 = this->Vertices[v0].Position;
    const std::array<double, 3>& p1 = this->Vertices[v1].Position;
    const std::array<double, 3>& p2 = this->Vertices[v2].Position;

    double u[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
    double v[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
    double n[3] = { 0.0, 0.0, 0.0 };
    vtkMath::Cross(u, v, n);

    for (int i = 0; i < 3; ++i)
      {
      normals[face.Vertices[i]][0] += n[0];
      normals[face.Vertices[i]][1] += n[1];
      normals[face.Vertices[i]][2] += n[2];
      }
    }

  std::vector<std::array<double, 3> > newPositions(this->Vertices.size());
  std::vector<bool> shouldMove(this->Vertices.size(), false);

  for (vtkIdType vertexId = 0; vertexId < static_cast<vtkIdType>(this->Vertices.size()); ++vertexId)
    {
    Vertex& vertex = this->Vertices[vertexId];
    if (!vertex.Active)
      {
      continue;
      }
    if (preserveBoundary && vertex.Boundary)
      {
      newPositions[vertexId] = vertex.Position;
      continue;
      }

    const std::set<vtkIdType>& neighbors = this->VertexNeighbors[vertexId];
    if (neighbors.empty())
      {
      newPositions[vertexId] = vertex.Position;
      continue;
      }

    std::array<double, 3> average{ { 0.0, 0.0, 0.0 } };
    for (vtkIdType neighborId : neighbors)
      {
      const std::array<double, 3>& neighborPos = this->Vertices[neighborId].Position;
      average[0] += neighborPos[0];
      average[1] += neighborPos[1];
      average[2] += neighborPos[2];
      }
    double inv = 1.0 / static_cast<double>(neighbors.size());
    average[0] *= inv;
    average[1] *= inv;
    average[2] *= inv;

    std::array<double, 3> displacement{
      average[0] - vertex.Position[0],
      average[1] - vertex.Position[1],
      average[2] - vertex.Position[2]
    };

    double normalVector[3] = { normals[vertexId][0], normals[vertexId][1], normals[vertexId][2] };
    double normalMagnitude = vtkMath::Norm(normalVector);
    if (normalMagnitude > std::numeric_limits<double>::epsilon())
      {
      vtkMath::Normalize(normalVector);
      double projection = displacement[0] * normalVector[0]
        + displacement[1] * normalVector[1]
        + displacement[2] * normalVector[2];
      std::array<double, 3> tangential{
        displacement[0] - projection * normalVector[0],
        displacement[1] - projection * normalVector[1],
        displacement[2] - projection * normalVector[2]
      };
      newPositions[vertexId] = {
        vertex.Position[0] + relaxation * tangential[0],
        vertex.Position[1] + relaxation * tangential[1],
        vertex.Position[2] + relaxation * tangential[2]
      };
      shouldMove[vertexId] = true;
      }
    else
      {
      newPositions[vertexId] = vertex.Position;
      }
    }

  for (vtkIdType vertexId = 0; vertexId < static_cast<vtkIdType>(this->Vertices.size()); ++vertexId)
    {
    if (!shouldMove[vertexId])
      {
      continue;
      }
    this->Vertices[vertexId].Position = newPositions[vertexId];
    }
}

//----------------------------------------------------------------------------
void RemeshMesh::ProjectToSurface(vtkStaticCellLocator* locator)
{
  if (!locator)
    {
    return;
    }

  double closestPoint[3] = { 0.0, 0.0, 0.0 };
  double closestPointDist2 = 0.0;
  vtkIdType cellId = -1;
  int subId = -1;

  for (Vertex& vertex : this->Vertices)
    {
    if (!vertex.Active)
      {
      continue;
      }
    locator->FindClosestPoint(vertex.Position.data(), closestPoint, cellId, subId, closestPointDist2);
    vertex.Position[0] = closestPoint[0];
    vertex.Position[1] = closestPoint[1];
    vertex.Position[2] = closestPoint[2];
    }
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> RemeshMesh::ToPolyData() const
{
  vtkNew<vtkPoints> points;
  vtkNew<vtkCellArray> polys;

  std::vector<vtkIdType> vertexIdMap(this->Vertices.size(), -1);
  vtkIdType nextId = 0;
  for (vtkIdType vertexIndex = 0; vertexIndex < static_cast<vtkIdType>(this->Vertices.size()); ++vertexIndex)
    {
    const Vertex& vertex = this->Vertices[vertexIndex];
    if (!vertex.Active)
      {
      continue;
      }
    vertexIdMap[vertexIndex] = nextId;
    points->InsertNextPoint(vertex.Position.data());
    ++nextId;
    }

  for (const Face& face : this->Faces)
    {
    if (!face.Active)
      {
      continue;
      }
    vtkIdType v0 = vertexIdMap[face.Vertices[0]];
    vtkIdType v1 = vertexIdMap[face.Vertices[1]];
    vtkIdType v2 = vertexIdMap[face.Vertices[2]];
    if (v0 < 0 || v1 < 0 || v2 < 0)
      {
      continue;
      }
    if (v0 == v1 || v1 == v2 || v2 == v0)
      {
      continue;
      }
    polys->InsertNextCell(3);
    polys->InsertCellPoint(v0);
    polys->InsertCellPoint(v1);
    polys->InsertCellPoint(v2);
    }

  vtkSmartPointer<vtkPolyData> output = vtkSmartPointer<vtkPolyData>::New();
  output->SetPoints(points.GetPointer());
  output->SetPolys(polys.GetPointer());
  return output;
}

//----------------------------------------------------------------------------
void RemeshMesh::BuildConnectivity()
{
  this->EdgeToFaces.clear();
  this->VertexNeighbors.clear();
  this->VertexNeighbors.resize(this->Vertices.size());

  for (vtkIdType faceIndex = 0; faceIndex < static_cast<vtkIdType>(this->Faces.size()); ++faceIndex)
    {
    Face& face = this->Faces[faceIndex];
    if (!face.Active)
      {
      continue;
      }

    vtkIdType ids[3] = { face.Vertices[0], face.Vertices[1], face.Vertices[2] };
    if (ids[0] < 0 || ids[1] < 0 || ids[2] < 0)
      {
      face.Active = false;
      continue;
      }
    if (!this->Vertices[ids[0]].Active || !this->Vertices[ids[1]].Active || !this->Vertices[ids[2]].Active)
      {
      face.Active = false;
      continue;
      }

    this->VertexNeighbors[ids[0]].insert(ids[1]);
    this->VertexNeighbors[ids[0]].insert(ids[2]);
    this->VertexNeighbors[ids[1]].insert(ids[0]);
    this->VertexNeighbors[ids[1]].insert(ids[2]);
    this->VertexNeighbors[ids[2]].insert(ids[0]);
    this->VertexNeighbors[ids[2]].insert(ids[1]);

    for (int edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
      {
      vtkIdType a = ids[edgeIndex];
      vtkIdType b = ids[(edgeIndex + 1) % 3];
      vtkIdType c = ids[(edgeIndex + 2) % 3];
      EdgeKey key(a, b);
      EdgeFaceConnectivity connectivity;
      connectivity.FaceId = faceIndex;
      connectivity.OppositeVertex = c;
      connectivity.Forward = (key.V0 == a);
      this->EdgeToFaces[key].push_back(connectivity);
      }
    }

  for (Vertex& vertex : this->Vertices)
    {
    vertex.Boundary = false;
    }

  for (const auto& entry : this->EdgeToFaces)
    {
    const std::vector<EdgeFaceConnectivity>& adjacency = entry.second;
    if (adjacency.size() == 1)
      {
      this->Vertices[entry.first.V0].Boundary = true;
      this->Vertices[entry.first.V1].Boundary = true;
      }
    }
}

//----------------------------------------------------------------------------
void RemeshMesh::RemoveInactiveElements()
{
  std::vector<vtkIdType> indexMap(this->Vertices.size(), -1);
  std::vector<Vertex> compactVertices;
  compactVertices.reserve(this->Vertices.size());
  for (vtkIdType vertexIndex = 0; vertexIndex < static_cast<vtkIdType>(this->Vertices.size()); ++vertexIndex)
    {
    Vertex& vertex = this->Vertices[vertexIndex];
    if (!vertex.Active)
      {
      continue;
      }
    indexMap[vertexIndex] = static_cast<vtkIdType>(compactVertices.size());
    compactVertices.push_back(vertex);
    }

  for (Face& face : this->Faces)
    {
    if (!face.Active)
      {
      continue;
      }
    vtkIdType v0 = indexMap[face.Vertices[0]];
    vtkIdType v1 = indexMap[face.Vertices[1]];
    vtkIdType v2 = indexMap[face.Vertices[2]];
    if (v0 < 0 || v1 < 0 || v2 < 0)
      {
      face.Active = false;
      continue;
      }
    if (v0 == v1 || v1 == v2 || v2 == v0)
      {
      face.Active = false;
      continue;
      }
    face.Vertices[0] = v0;
    face.Vertices[1] = v1;
    face.Vertices[2] = v2;
    }

  std::vector<Face> compactFaces;
  compactFaces.reserve(this->Faces.size());
  for (const Face& face : this->Faces)
    {
    if (!face.Active)
      {
      continue;
      }
    compactFaces.push_back(face);
    }

  this->Vertices.swap(compactVertices);
  this->Faces.swap(compactFaces);
}

//----------------------------------------------------------------------------
bool RemeshMesh::SplitEdge(const EdgeKey& key, const std::vector<EdgeFaceConnectivity>& adjacency, bool preserveBoundary)
{
  if (adjacency.empty())
    {
    return false;
    }

  if (!this->Vertices[key.V0].Active || !this->Vertices[key.V1].Active)
    {
    return false;
    }

  bool isBoundaryEdge = (adjacency.size() == 1);
  if (preserveBoundary && isBoundaryEdge)
    {
    // Allow splitting boundary edges but keep new vertex on boundary
    }

  std::array<double, 3> midpoint{
    0.5 * (this->Vertices[key.V0].Position[0] + this->Vertices[key.V1].Position[0]),
    0.5 * (this->Vertices[key.V0].Position[1] + this->Vertices[key.V1].Position[1]),
    0.5 * (this->Vertices[key.V0].Position[2] + this->Vertices[key.V1].Position[2])
  };

  vtkIdType newVertexId = static_cast<vtkIdType>(this->Vertices.size());
  Vertex newVertex;
  newVertex.Position = midpoint;
  newVertex.Active = true;
  newVertex.Boundary = isBoundaryEdge && preserveBoundary;
  this->Vertices.push_back(newVertex);

  std::vector<Face> newFaces;

  for (const EdgeFaceConnectivity& faceInfo : adjacency)
    {
    if (faceInfo.FaceId < 0 || faceInfo.FaceId >= static_cast<vtkIdType>(this->Faces.size()))
      {
      continue;
      }
    Face& face = this->Faces[faceInfo.FaceId];
    if (!face.Active)
      {
      continue;
      }
    vtkIdType opposite = faceInfo.OppositeVertex;
    bool forward = faceInfo.Forward;

    face.Active = false;

    if (forward)
      {
      Face faceA;
      faceA.Vertices = { { key.V0, newVertexId, opposite } };
      Face faceB;
      faceB.Vertices = { { newVertexId, key.V1, opposite } };
      newFaces.push_back(faceA);
      newFaces.push_back(faceB);
      }
    else
      {
      Face faceA;
      faceA.Vertices = { { key.V1, newVertexId, opposite } };
      Face faceB;
      faceB.Vertices = { { newVertexId, key.V0, opposite } };
      newFaces.push_back(faceA);
      newFaces.push_back(faceB);
      }
    }

  for (const Face& newFace : newFaces)
    {
    this->Faces.push_back(newFace);
    }

  return !newFaces.empty();
}

//----------------------------------------------------------------------------
bool RemeshMesh::CollapseEdge(const EdgeKey& key, bool preserveBoundary)
{
  if (!this->Vertices[key.V0].Active || !this->Vertices[key.V1].Active)
    {
    return false;
    }

  bool isBoundaryEdge = false;
  auto adjacencyIt = this->EdgeToFaces.find(key);
  if (adjacencyIt != this->EdgeToFaces.end())
    {
    isBoundaryEdge = (adjacencyIt->second.size() == 1);
    }

  if (preserveBoundary && isBoundaryEdge)
    {
    return false;
    }

  Vertex& vertex0 = this->Vertices[key.V0];
  Vertex& vertex1 = this->Vertices[key.V1];

  if (preserveBoundary && (vertex0.Boundary != vertex1.Boundary))
    {
    return false;
    }

  std::array<double, 3> newPosition{
    0.5 * (vertex0.Position[0] + vertex1.Position[0]),
    0.5 * (vertex0.Position[1] + vertex1.Position[1]),
    0.5 * (vertex0.Position[2] + vertex1.Position[2])
  };

  vertex0.Position = newPosition;

  for (Face& face : this->Faces)
    {
    if (!face.Active)
      {
      continue;
      }
    for (int i = 0; i < 3; ++i)
      {
      if (face.Vertices[i] == key.V1)
        {
        face.Vertices[i] = key.V0;
        }
      }
    if (face.Vertices[0] == face.Vertices[1]
      || face.Vertices[1] == face.Vertices[2]
      || face.Vertices[2] == face.Vertices[0])
      {
      face.Active = false;
      }
    }

  vertex1.Active = false;
  return true;
}

//----------------------------------------------------------------------------
static int TargetValence(bool isBoundary)
{
  return isBoundary ? 4 : 6;
}

//----------------------------------------------------------------------------
void RemeshMesh::RunRemesh(vtkPolyData* transformedInput, double targetEdgeLength,
  int iterationCount, double relaxation, bool preserveBoundary)
{
  if (!this->Initialize(transformedInput))
    {
    vtkErrorMacro("Failed to prepare input mesh for remeshing.");
    return false;
    }

  targetEdgeLength = std::max(targetEdgeLength, 0.0);
  iterationCount = std::max(iterationCount, 1);
  relaxation = std::min(std::max(relaxation, 0.0), 1.0);

  if (targetEdgeLength <= std::numeric_limits<double>::epsilon())
    {
    targetEdgeLength = mesh.AverageEdgeLength();
    }

  if (targetEdgeLength <= std::numeric_limits<double>::epsilon())
    {
    vtkErrorMacro("Cannot determine target edge length for remeshing.");
    return false;
    }

  vtkNew<vtkStaticCellLocator> locator;
  locator->SetDataSet(transformedInput);
  locator->BuildLocator();

  double maximumEdgeLength = REMESH_SPLIT_FACTOR * targetEdgeLength;
  double minimumEdgeLength = REMESH_COLLAPSE_FACTOR * targetEdgeLength;

  for (int iteration = 0; iteration < iterationCount; ++iteration)
    {
    mesh.SplitLongEdges(maximumEdgeLength, preserveBoundary);
    mesh.CollapseShortEdges(minimumEdgeLength, preserveBoundary);
    mesh.FlipEdges(preserveBoundary);
    mesh.TangentialRelaxation(relaxation, preserveBoundary);
    mesh.ProjectToSurface(locator.GetPointer());
    }
  return true;
}

//----------------------------------------------------------------------------
bool RemeshMesh::FlipEdgeInternal(const EdgeKey& key, const std::vector<EdgeFaceConnectivity>& adjacency, bool preserveBoundary)
{
  if (adjacency.size() != 2)
    {
    return false;
    }

  vtkIdType v0 = key.V0;
  vtkIdType v1 = key.V1;

  vtkIdType faceId0 = adjacency[0].FaceId;
  vtkIdType faceId1 = adjacency[1].FaceId;
  vtkIdType v2 = adjacency[0].OppositeVertex;
  vtkIdType v3 = adjacency[1].OppositeVertex;

  if (!this->Vertices[v0].Active || !this->Vertices[v1].Active
    || !this->Vertices[v2].Active || !this->Vertices[v3].Active)
    {
    return false;
    }

  if (preserveBoundary && (this->Vertices[v0].Boundary || this->Vertices[v1].Boundary))
    {
    return false;
    }

  const std::set<vtkIdType>& neighborsV0 = this->VertexNeighbors[v0];
  const std::set<vtkIdType>& neighborsV1 = this->VertexNeighbors[v1];
  const std::set<vtkIdType>& neighborsV2 = this->VertexNeighbors[v2];
  const std::set<vtkIdType>& neighborsV3 = this->VertexNeighbors[v3];

  auto deviation = [](int valence, bool boundary)
    {
      return std::abs(valence - TargetValence(boundary));
    };

  int beforeScore = deviation(static_cast<int>(neighborsV0.size()), this->Vertices[v0].Boundary)
    + deviation(static_cast<int>(neighborsV1.size()), this->Vertices[v1].Boundary)
    + deviation(static_cast<int>(neighborsV2.size()), this->Vertices[v2].Boundary)
    + deviation(static_cast<int>(neighborsV3.size()), this->Vertices[v3].Boundary);

  std::set<vtkIdType> neighborsV0After = neighborsV0;
  neighborsV0After.erase(v1);
  neighborsV0After.insert(v3);
  std::set<vtkIdType> neighborsV1After = neighborsV1;
  neighborsV1After.erase(v0);
  neighborsV1After.insert(v2);
  std::set<vtkIdType> neighborsV2After = neighborsV2;
  neighborsV2After.insert(v1);
  neighborsV2After.erase(v0);
  std::set<vtkIdType> neighborsV3After = neighborsV3;
  neighborsV3After.insert(v0);
  neighborsV3After.erase(v1);

  int afterScore = deviation(static_cast<int>(neighborsV0After.size()), this->Vertices[v0].Boundary)
    + deviation(static_cast<int>(neighborsV1After.size()), this->Vertices[v1].Boundary)
    + deviation(static_cast<int>(neighborsV2After.size()), this->Vertices[v2].Boundary)
    + deviation(static_cast<int>(neighborsV3After.size()), this->Vertices[v3].Boundary);

  if (afterScore >= beforeScore)
    {
    return false;
    }

  Face& face0 = this->Faces[faceId0];
  Face& face1 = this->Faces[faceId1];
  if (!face0.Active || !face1.Active)
    {
    return false;
    }

  face0.Vertices = { { v2, v3, v1 } };
  face1.Vertices = { { v3, v2, v0 } };

  return true;
}

//----------------------------------------------------------------------------
double RemeshMesh::EdgeLength(vtkIdType v0, vtkIdType v1) const
{
  const std::array<double, 3>& p0 = this->Vertices[v0].Position;
  const std::array<double, 3>& p1 = this->Vertices[v1].Position;
  double diff[3] = { p0[0] - p1[0], p0[1] - p1[1], p0[2] - p1[2] };
  return vtkMath::Norm(diff);
}

} // end anonymous namespace
//----------------------------------------------------------------------------