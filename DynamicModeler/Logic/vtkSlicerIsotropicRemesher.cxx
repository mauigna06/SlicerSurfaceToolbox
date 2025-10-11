/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#include "vtkSlicerIsotropicRemesher.h"

#include "remesh_botsch.h"

#include <vtkCellArray.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkTriangleFilter.h>

#include <Eigen/Core>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

vtkStandardNewMacro(vtkSlicerIsotropicRemesher);

//----------------------------------------------------------------------------
vtkSlicerIsotropicRemesher::vtkSlicerIsotropicRemesher() = default;

//----------------------------------------------------------------------------
vtkSlicerIsotropicRemesher::~vtkSlicerIsotropicRemesher() = default;

//----------------------------------------------------------------------------
void vtkSlicerIsotropicRemesher::SetLastErrorMessage(const std::string& errorMessage)
{
  this->LastErrorMessage = errorMessage;
}

//----------------------------------------------------------------------------
const std::string& vtkSlicerIsotropicRemesher::GetLastErrorMessage() const
{
  return this->LastErrorMessage;
}

//----------------------------------------------------------------------------
bool vtkSlicerIsotropicRemesher::Remesh(
  vtkPolyData* inputMesh,
  double targetEdgeLength,
  int iterationCount,
  bool projectToInput,
  vtkPolyData* outputMesh)
{
  this->LastErrorMessage.clear();

  if (!inputMesh)
    {
    this->SetLastErrorMessage("Invalid input mesh");
    return false;
    }
  if (!outputMesh)
    {
    this->SetLastErrorMessage("Invalid output mesh");
    return false;
    }

  vtkNew<vtkTriangleFilter> triangleFilter;
  triangleFilter->SetInputData(inputMesh);
  triangleFilter->PassVertsOn();
  triangleFilter->PassLinesOff();
  triangleFilter->Update();

  vtkPolyData* triangleMesh = triangleFilter->GetOutput();
  if (!triangleMesh || triangleMesh->GetNumberOfPoints() == 0 || triangleMesh->GetNumberOfCells() == 0)
    {
    outputMesh->Initialize();
    return true;
    }

  if (triangleMesh->GetNumberOfPoints() > static_cast<vtkIdType>(std::numeric_limits<int>::max())
    || triangleMesh->GetNumberOfCells() > static_cast<vtkIdType>(std::numeric_limits<int>::max()))
    {
    this->SetLastErrorMessage("Input mesh is too large to remesh.");
    return false;
    }

  const vtkIdType numberOfPoints = triangleMesh->GetNumberOfPoints();
  const vtkIdType numberOfCells = triangleMesh->GetNumberOfCells();

  Eigen::MatrixXd vertices(numberOfPoints, 3);
  for (vtkIdType pointId = 0; pointId < numberOfPoints; ++pointId)
    {
    double point[3] = { 0.0, 0.0, 0.0 };
    triangleMesh->GetPoint(pointId, point);
    vertices(pointId, 0) = point[0];
    vertices(pointId, 1) = point[1];
    vertices(pointId, 2) = point[2];
    }

  Eigen::MatrixXi faces(static_cast<int>(numberOfCells), 3);
  vtkCellArray* polys = triangleMesh->GetPolys();
  if (!polys)
    {
    this->SetLastErrorMessage("Triangulated mesh does not contain polygonal faces.");
    return false;
    }
  polys->InitTraversal();
  vtkIdType numberOfCellPoints = 0;
  const vtkIdType* cellPoints = nullptr;
  int faceIndex = 0;
  while (polys->GetNextCell(numberOfCellPoints, cellPoints))
    {
    if (numberOfCellPoints != 3)
      {
      continue;
      }
    for (int cornerIndex = 0; cornerIndex < 3; ++cornerIndex)
      {
      vtkIdType pointId = cellPoints[cornerIndex];
      if (pointId < 0 || pointId >= numberOfPoints)
        {
        this->SetLastErrorMessage("Invalid triangle connectivity encountered during remeshing.");
        return false;
        }
      faces(faceIndex, cornerIndex) = static_cast<int>(pointId);
      }
    ++faceIndex;
    }
  faces.conservativeResize(faceIndex, Eigen::NoChange);

  if (faces.rows() == 0)
    {
    outputMesh->Initialize();
    return true;
    }

  auto encodeEdge = [](int a, int b) -> std::uint64_t
    {
    const int v0 = std::min(a, b);
    const int v1 = std::max(a, b);
    return (static_cast<std::uint64_t>(v0) << 32) | static_cast<std::uint32_t>(v1);
    };

  std::unordered_map<std::uint64_t, int> edgeUseCounts;
  edgeUseCounts.reserve(static_cast<std::size_t>(faces.rows()) * 3);

  for (int row = 0; row < faces.rows(); ++row)
    {
    const int v0 = faces(row, 0);
    const int v1 = faces(row, 1);
    const int v2 = faces(row, 2);
    ++edgeUseCounts[encodeEdge(v0, v1)];
    ++edgeUseCounts[encodeEdge(v1, v2)];
    ++edgeUseCounts[encodeEdge(v2, v0)];
    }

  double totalEdgeLength = 0.0;
  std::size_t uniqueEdgeCount = 0;
  std::set<int> boundaryVertices;

  for (const auto& edgeEntry : edgeUseCounts)
    {
    const std::uint64_t key = edgeEntry.first;
    const int count = edgeEntry.second;
    const int vertexA = static_cast<int>(key >> 32);
    const int vertexB = static_cast<int>(key & 0xffffffffu);

    const double length = (vertices.row(vertexA) - vertices.row(vertexB)).norm();
    totalEdgeLength += length;
    ++uniqueEdgeCount;

    if (count == 1)
      {
      boundaryVertices.insert(vertexA);
      boundaryVertices.insert(vertexB);
      }
    }

  const double averageEdgeLength = uniqueEdgeCount > 0
    ? totalEdgeLength / static_cast<double>(uniqueEdgeCount)
    : 0.0;

  double effectiveTargetEdgeLength = std::max(targetEdgeLength, 0.0);
  if (effectiveTargetEdgeLength <= 0.0)
    {
    effectiveTargetEdgeLength = averageEdgeLength;
    }
  if (effectiveTargetEdgeLength <= 0.0)
    {
    effectiveTargetEdgeLength = 1.0;
    }

  int effectiveIterationCount = std::max(iterationCount, 1);

  Eigen::VectorXi featureVertices;
  if (!boundaryVertices.empty())
    {
    featureVertices.resize(static_cast<int>(boundaryVertices.size()));
    int featureIndex = 0;
    for (int vertexId : boundaryVertices)
      {
      featureVertices(featureIndex++) = vertexId;
      }
    }
  else
    {
    featureVertices.resize(0);
    }

  Eigen::MatrixXd remeshedVertices = vertices;
  Eigen::MatrixXi remeshedFaces = faces;

  try
    {
    remesh_botsch(remeshedVertices, remeshedFaces, effectiveTargetEdgeLength, effectiveIterationCount, featureVertices, projectToInput);
    }
  catch (const std::exception& exc)
    {
    this->SetLastErrorMessage(exc.what());
    return false;
    }
  catch (...)
    {
    this->SetLastErrorMessage("Remeshing failed due to an unknown error.");
    return false;
    }

  if (remeshedVertices.rows() == 0 || remeshedFaces.rows() == 0)
    {
    outputMesh->Initialize();
    return true;
    }

  vtkNew<vtkPoints> remeshedPoints;
  const Eigen::Index remeshedPointCount = remeshedVertices.rows();
  remeshedPoints->SetNumberOfPoints(static_cast<vtkIdType>(remeshedPointCount));
  for (Eigen::Index pointIndex = 0; pointIndex < remeshedPointCount; ++pointIndex)
    {
    remeshedPoints->SetPoint(
      static_cast<vtkIdType>(pointIndex),
      remeshedVertices(pointIndex, 0),
      remeshedVertices(pointIndex, 1),
      remeshedVertices(pointIndex, 2));
    }

  vtkNew<vtkCellArray> remeshedTriangles;
  const Eigen::Index remeshedFaceCount = remeshedFaces.rows();
  for (Eigen::Index faceIndexOut = 0; faceIndexOut < remeshedFaceCount; ++faceIndexOut)
    {
    vtkIdType triangle[3];
    for (int corner = 0; corner < 3; ++corner)
      {
      const int vertexIndex = remeshedFaces(faceIndexOut, corner);
      if (vertexIndex < 0 || vertexIndex >= remeshedPointCount)
        {
        this->SetLastErrorMessage("Remeshing produced invalid triangle connectivity.");
        return false;
        }
      triangle[corner] = static_cast<vtkIdType>(vertexIndex);
      }
    remeshedTriangles->InsertNextCell(3, triangle);
    }

  vtkNew<vtkPolyData> resultPolyData;
  resultPolyData->SetPoints(remeshedPoints);
  resultPolyData->SetPolys(remeshedTriangles);

  outputMesh->ShallowCopy(resultPolyData);
  return true;
}
