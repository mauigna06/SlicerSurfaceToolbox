/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through CANARIE's Research Software Program, Cancer
  Care Ontario, OpenAnatomy, and Brigham and Women's Hospital through NIH grant R01MH112748.

==============================================================================*/

#include "vtkSlicerDynamicModelerCreateCubeTool.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLMarkupsClosedCurveNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLLinearTransformNode.h>

// VTK includes
#include <vtkCubeSource.h>
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransformPolyDataFilter.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerCreateCubeTool);

//const char* CUBE_INPUT_MODEL_REFERENCE_ROLE = "Cube.InputModel";
const char* CUBE_OUTPUT_MODEL_REFERENCE_ROLE = "Cube.OutputModel";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerCreateCubeTool::vtkSlicerDynamicModelerCreateCubeTool()
{
  /////////
  // Outputs
  vtkNew<vtkStringArray> inputModelClassNames;
  inputModelClassNames->InsertNextValue("vtkMRMLModelNode");

  NodeInfo outputModel(
    "Cube model",
    "Output model of an cube according to parameters.",
    inputModelClassNames,
    CUBE_OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputModel);

  /////////
  // Parameters
  ParameterInfo parameterXLength(
    "Set X length",
    "Set the length of the cube in the x-direction.",
    "XLength",
    PARAMETER_DOUBLE,
    10.0);
  this->InputParameterInfo.push_back(parameterXLength);

  ParameterInfo parameterYLength(
    "Set Y length",
    "Set the length of the cube in the y-direction.",
    "YLength",
    PARAMETER_DOUBLE,
    25.0);
  this->InputParameterInfo.push_back(parameterYLength);

    ParameterInfo parameterZLength(
    "Set Z length",
    "Set the length of the cube in the z-direction.",
    "ZLength",
    PARAMETER_DOUBLE,
    50.0);
  this->InputParameterInfo.push_back(parameterZLength);

  this->CubeSourceFilter = vtkSmartPointer<vtkCubeSource>::New();

  //this->CubeModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  //this->CubeModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  //this->CubeModelTransformFilter->SetInputConnection(this->CubeSourceFilter->GetOutputPort());
  //this->CubeModelTransformFilter->SetTransform(this->CubeModelTransform);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerCreateCubeTool::~vtkSlicerDynamicModelerCreateCubeTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerCreateCubeTool::GetName()
{
  return "Create Cube";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerCreateCubeTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(CUBE_OUTPUT_MODEL_REFERENCE_ROLE));
  if (!outputModelNode)
    {
    // Nothing to output
    return true;
    }

  double XLength = this->GetNthInputParameterValue(0, surfaceEditorNode).ToDouble();
  double YLength = this->GetNthInputParameterValue(1, surfaceEditorNode).ToDouble();
  double ZLength = this->GetNthInputParameterValue(2, surfaceEditorNode).ToDouble();

  this->CubeSourceFilter->SetXLength(XLength);
  this->CubeSourceFilter->SetYLength(YLength);
  this->CubeSourceFilter->SetZLength(ZLength);
  //this->CubeSourceFilter->SetBounds(bounds);
  this->CubeSourceFilter->Update();

  /*
  if (outputModelNode->GetParentTransformNode())
    {
    outputModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
    }
  else
    {
    this->OutputWorldToModelTransform->Identity();
    }
  this->OutputWorldToModelTransformFilter->Update();
  */

  vtkNew<vtkPolyData> outputPolyData;
  outputPolyData->DeepCopy(this->CubeSourceFilter->GetOutput());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObservePolyData(outputPolyData);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}

