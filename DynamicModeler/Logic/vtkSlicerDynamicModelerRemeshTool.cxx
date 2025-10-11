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

#include "vtkSlicerIsotropicRemesher.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformableNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransformPolyDataFilter.h>

// STD includes

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
    3,
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
    0.0,
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
    false);
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

  int iterationCount = this->GetNthInputParameterValue(0, surfaceEditorNode).ToInt();
  double targetEdgeLength = this->GetNthInputParameterValue(1, surfaceEditorNode).ToDouble();
  bool projectToInput = this->GetNthInputParameterValue(2, surfaceEditorNode).ToInt() != 0;
  vtkNew<vtkSlicerIsotropicRemesher> remesher;
  vtkNew<vtkPolyData> remeshedPolyData;
  if (!remesher->Remesh(transformedInput, targetEdgeLength, iterationCount, projectToInput, remeshedPolyData))
    {
    vtkErrorMacro("Remeshing failed: " << remesher->GetLastErrorMessage());
    return false;
    }

  if (!remeshedPolyData.GetPointer()
    || remeshedPolyData->GetNumberOfPoints() == 0
    || remeshedPolyData->GetNumberOfPolys() == 0)
    {
    outputModelNode->SetAndObserveMesh(remeshedPolyData.GetPointer());
    return true;
    }

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
