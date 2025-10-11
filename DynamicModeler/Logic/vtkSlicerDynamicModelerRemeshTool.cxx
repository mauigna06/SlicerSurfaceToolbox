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

//==============================================================================

#include "vtkSlicerDynamicModelerRemeshTool.h"

#include "remesh_botsch.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformableNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkCellArray.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>

// STD includes
#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

// Eigen includes
#include <Eigen/Core>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerDynamicModelerRemeshTool);

namespace
{
const char* REMESH_INPUT_MODEL_REFERENCE_ROLE = "Remesh.InputModel";
const char* REMESH_OUTPUT_MODEL_REFERENCE_ROLE = "Remesh.OutputModel";
const char* REMESH_ITERATION_COUNT_ATTRIBUTE = "Remesh.IterationCount";
const char* REMESH_TARGET_EDGE_LENGTH_ATTRIBUTE = "Remesh.TargetEdgeLength";
const char* REMESH_PROJECT_ATTRIBUTE = "Remesh.Project";
} // namespace

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRemeshTool::vtkSlicerDynamicModelerRemeshTool()
{
  vtkNew<vtkIntArray> inputModelEvents;
  inputModelEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLModelNode::MeshModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);

  vtkNew<vtkStringArray> inputModelClassNames;
  inputModelClassNames->InsertNextValue("vtkMRMLModelNode");

  NodeInfo inputModel(
    "Model",
    "Model to be remeshed using the Botsch isotropic remeshing algorithm.",
    inputModelClassNames,
    REMESH_INPUT_MODEL_REFERENCE_ROLE,
    true,
    false,
    inputModelEvents);
  this->InputNodeInfo.push_back(inputModel);

  NodeInfo outputModel(
    "Remeshed model",
    "Remeshed output model.",
    inputModelClassNames,
    REMESH_OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false);
  this->OutputNodeInfo.push_back(outputModel);

  ParameterInfo iterationCount(
    "Iterations",
    "Number of remeshing iterations to perform.",
    REMESH_ITERATION_COUNT_ATTRIBUTE,
    PARAMETER_INT,
    10,
    0,
    1.0);
  iterationCount.NumbersRange->SetValue(0, 1.0);
  iterationCount.NumbersRange->SetValue(1, 100.0);
  this->InputParameterInfo.push_back(iterationCount);

  ParameterInfo targetEdgeLength(
    "Target edge length",
    "Desired edge length for the isotropic remeshing. Set to 0 to use the average edge length of the input.",
    REMESH_TARGET_EDGE_LENGTH_ATTRIBUTE,
    PARAMETER_DOUBLE,
    0.1,
    3,
    0.1);
  targetEdgeLength.NumbersRange->SetValue(0, 0.0);
  targetEdgeLength.NumbersRange->SetValue(1, 1000.0);
  this->InputParameterInfo.push_back(targetEdgeLength);

  ParameterInfo projectToInput(
    "Project to input",
    "Project vertices back to the input surface after each iteration to keep the shape from drifting.",
    REMESH_PROJECT_ATTRIBUTE,
    PARAMETER_BOOL,
    true);
  this->InputParameterInfo.push_back(projectToInput);

  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->OutputWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputModelToWorldTransformFilter->SetTransform(this->OutputWorldToModelTransform);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRemeshTool::~vtkSlicerDynamicModelerRemeshTool()
= default;

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerTool* vtkSlicerDynamicModelerRemeshTool::CreateToolInstance()
{
  return vtkSlicerDynamicModelerRemeshTool::New();
}

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerRemeshTool::GetName()
{
  return "Remesh";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerRemeshTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid parameter node!");
    return false;
    }

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REMESH_INPUT_MODEL_REFERENCE_ROLE));
  vtkMRMLModelNode* outputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REMESH_OUTPUT_MODEL_REFERENCE_ROLE));

  if (!inputModelNode)
    {
    vtkErrorMacro("Invalid input model node!");
    return false;
    }

  if (!outputModelNode)
    {
    // Nothing to output but still considered success
    return true;
    }

  vtkPolyData* inputPolyData = inputModelNode->GetPolyData();
  if (!inputPolyData || inputPolyData->GetNumberOfPoints() == 0)
    {
    vtkNew<vtkPolyData> emptyPolyData;
    outputModelNode->SetAndObserveMesh(emptyPolyData.GetPointer());
    return true;
    }

  if (inputModelNode->GetParentTransformNode())
    {
    inputModelNode->GetParentTransformNode()->GetTransformToWorld(this->InputModelNodeToWorldTransform);
    }
  else
    {
    this->InputModelNodeToWorldTransform->Identity();
    }

  this->InputModelToWorldTransformFilter->SetInputData(inputPolyData);
  this->InputModelToWorldTransformFilter->Update();

  vtkPolyData* transformedInput = this->InputModelToWorldTransformFilter->GetOutput();
  if (!transformedInput || transformedInput->GetNumberOfPoints() == 0)
    {
    vtkNew<vtkPolyData> emptyPolyData;
    outputModelNode->SetAndObserveMesh(emptyPolyData.GetPointer());
    return true;
    }

  vtkNew<vtkTriangleFilter> triangleFilter;
  triangleFilter->SetInputData(transformedInput);
  triangleFilter->PassVertsOn();
  triangleFilter->PassLinesOff();
  triangleFilter->Update();

  vtkPolyData* triangleMesh = triangleFilter->GetOutput();
  if (!triangleMesh || triangleMesh->GetNumberOfPoints() == 0 || triangleMesh->GetNumberOfCells() == 0)
    {
    vtkNew<vtkPolyData> emptyPolyData;
    outputModelNode->SetAndObserveMesh(emptyPolyData.GetPointer());
    return true;
    }

  if (triangleMesh->GetNumberOfPoints() > static_cast<vtkIdType>(std::numeric_limits<int>::max())
    || triangleMesh->GetNumberOfCells() > static_cast<vtkIdType>(std::numeric_limits<int>::max()))
    {
    vtkErrorMacro("Remeshing input is too large to process.");
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
    vtkErrorMacro("Triangulated mesh does not contain polygonal faces.");
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
        vtkErrorMacro("Invalid triangle connectivity encountered during remeshing.");
        return false;
        }
      faces(faceIndex, cornerIndex) = static_cast<int>(pointId);
      }
    ++faceIndex;
    }

  faces.conservativeResize(faceIndex, Eigen::NoChange);

  if (faces.rows() == 0)
    {
    vtkNew<vtkPolyData> emptyPolyData;
    outputModelNode->SetAndObserveMesh(emptyPolyData.GetPointer());
    return true;
    }

  int iterationCount = this->GetNthInputParameterValue(0, surfaceEditorNode).ToInt();
  double targetEdgeLength = this->GetNthInputParameterValue(1, surfaceEditorNode).ToDouble();
  bool projectToInput = this->GetNthInputParameterValue(2, surfaceEditorNode).ToInt() != 0;

  iterationCount = std::max(iterationCount, 1);
  targetEdgeLength = std::max(targetEdgeLength, 0.0);

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

  if (targetEdgeLength <= 0.0)
    {
    targetEdgeLength = averageEdgeLength;
    }
  if (targetEdgeLength <= 0.0)
    {
    targetEdgeLength = 1.0;
    }

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
  remesh_botsch(remeshedVertices, remeshedFaces, targetEdgeLength, iterationCount, featureVertices, projectToInput);
    }
  catch (const std::exception& exc)
    {
    vtkErrorMacro("Remeshing failed: " << exc.what());
    return false;
    }
  catch (...)
    {
    vtkErrorMacro("Remeshing failed due to an unknown error.");
    return false;
    }

  if (remeshedVertices.rows() == 0 || remeshedFaces.rows() == 0)
    {
    vtkNew<vtkPolyData> emptyPolyData;
    outputModelNode->SetAndObserveMesh(emptyPolyData.GetPointer());
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
        vtkErrorMacro("Remeshing produced invalid triangle connectivity.");
        return false;
        }
      triangle[corner] = static_cast<vtkIdType>(vertexIndex);
      }
    remeshedTriangles->InsertNextCell(3, triangle);
    }

  vtkNew<vtkPolyData> remeshedPolyData;
  remeshedPolyData->SetPoints(remeshedPoints);
  remeshedPolyData->SetPolys(remeshedTriangles);

  vtkNew<vtkPolyDataNormals> normals;
  normals->SetInputData(remeshedPolyData);
  normals->SplittingOff();
  normals->ComputePointNormalsOn();
  normals->Update();

  if (outputModelNode->GetParentTransformNode())
    {
    outputModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
    }
  else
    {
    this->OutputWorldToModelTransform->Identity();
    }

  this->OutputModelToWorldTransformFilter->SetInputConnection(normals->GetOutputPort());
  this->OutputModelToWorldTransformFilter->Update();

  vtkNew<vtkPolyData> outputMesh;
  outputMesh->DeepCopy(this->OutputModelToWorldTransformFilter->GetOutput());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObserveMesh(outputMesh.GetPointer());
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}
