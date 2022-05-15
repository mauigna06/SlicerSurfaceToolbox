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

#include "vtkSlicerDynamicModelerArrowTool.h"

#include "vtkMRMLDynamicModelerNode.h"

#include "vtkMRMLCrosshairDisplayableManager.h"

// MRML includes
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLMarkupsClosedCurveNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkArrowSource.h>
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransformPolyDataFilter.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerArrowTool);

//const char* ARROW_INPUT_MODEL_REFERENCE_ROLE = "Arrow.InputModel";
const char* ARROW_OUTPUT_MODEL_REFERENCE_ROLE = "Arrow.OutputModel";
const char* ARROW_OUTPUT_TRANSFORM_REFERENCE_ROLE = "Arrow.OutputTransform";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerArrowTool::vtkSlicerDynamicModelerArrowTool()
{
  /////////
  // Outputs
  vtkNew<vtkStringArray> outputModelClassNames;
  outputModelClassNames->InsertNextValue("vtkMRMLModelNode");
  NodeInfo outputModel(
    "Arrow model",
    "Output model of an arrow according to parameters.",
    outputModelClassNames,
    ARROW_OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputModel);

  vtkNew<vtkStringArray> outputTransformClassNames;
  outputTransformClassNames->InsertNextValue("vtkMRMLLinearTransformNode");
  NodeInfo outputTransform(
    "Arrow Transform",
    "Output transform of an arrow according to crosshair.",
    outputTransformClassNames,
    ARROW_OUTPUT_TRANSFORM_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputTransform);

  /////////
  // Parameters
  ParameterInfo parameterTipLength(
    "Set tip length",
    "Set the length, and radius of the tip.",
    "TipLength",
    PARAMETER_DOUBLE,
    10.0);
  this->InputParameterInfo.push_back(parameterTipLength);

  ParameterInfo parameterTipRadius(
    "Set tip radius",
    "Set the length, and radius of the tip.",
    "TipRadius",
    PARAMETER_DOUBLE,
    3.0);
  this->InputParameterInfo.push_back(parameterTipRadius);

  ParameterInfo parameterTipResolution(
    "Set tip resolution",
    "Set the resolution of the tip. The tip behaves the same as a cone.",
    "TipResolution",
    PARAMETER_INT,
    2);
  this->InputParameterInfo.push_back(parameterTipResolution);

  ParameterInfo parameterShaftRadius(
    "Set shaft radius",
    "Set the length, and radius of the tip.",
    "ShaftRadius",
    PARAMETER_DOUBLE,
    1.0);
  this->InputParameterInfo.push_back(parameterShaftRadius);

  ParameterInfo parameterShaftResolution(
    "Set shaft resolution",
    "Set the resolution of the shaft. Minimum is 3 for a triangular shaft.",
    "ShaftResolution",
    PARAMETER_INT,
    5);
  this->InputParameterInfo.push_back(parameterShaftResolution);

  //this->ArrowMesh = vtkSmartPointer<vtkPolyData>::New();

  this->ArrowSourceFilter = vtkSmartPointer<vtkArrowPolyData>::New();

  //this->ArrowModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  //this->ArrowModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  //this->ArrowModelTransformFilter->SetInputConnection(this->ArrowSourceFilter->GetOutputPort());
  //this->ArrowModelTransformFilter->SetTransform(this->ArrowModelTransform);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerArrowTool::~vtkSlicerDynamicModelerArrowTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerArrowTool::GetName()
{
  return "Create Arrow";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerArrowTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(ARROW_OUTPUT_MODEL_REFERENCE_ROLE));
  if (!outputModelNode)
    {
    // Nothing to output
    return true;
    }

  double tipLength = this->GetNthInputParameterValue(0, surfaceEditorNode).ToDouble();
  double tipRadius = this->GetNthInputParameterValue(1, surfaceEditorNode).ToDouble();
  double tipResolution = this->GetNthInputParameterValue(2, surfaceEditorNode).ToInt();
  double shaftRadius = this->GetNthInputParameterValue(3, surfaceEditorNode).ToDouble();
  double shaftResolution = this->GetNthInputParameterValue(4, surfaceEditorNode).ToInt();

  this->ArrowSourceFilter->SetTipLength(tipLength);
  this->ArrowSourceFilter->SetTipRadius(tipRadius);
  this->ArrowSourceFilter->SetTipResolution(tipResolution);
  this->ArrowSourceFilter->SetShaftRadius(shaftRadius);
  this->ArrowSourceFilter->SetShaftResolution(shaftResolution);
  this->ArrowSourceFilter->Update();

  """
  if (outputModelNode->GetParentTransformNode())
    {
    outputModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
    }
  else
    {
    this->OutputWorldToModelTransform->Identity();
    }
  this->OutputWorldToModelTransformFilter->Update();
  """

  vtkNew<vtkPolyData> outputPolyData;
  outputPolyData->DeepCopy(this->ArrowSourceFilter->GetOutput());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObservePolyData(outputPolyData);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  vtkMRMLLinearTransformNode* outputTransformNode = vtkMRMLLinearTransformNode::SafeDownCast(surfaceEditorNode->GetNodeReference(ARROW_OUTPUT_TRANSFORM_REFERENCE_ROLE));
  if (outputTransformNode)
    {
    vtkMRMLScene* scene = this->GetScene();
    vtkMRMLCrosshairNode* crosshairNode = vtkMRMLCrosshairDisplayableManager::FindCrosshairNode(scene);
    if (crosshairNode)
      {
      MRMLNodeModifyBlocker blocker(outputModelNode);
      vtkMRMLLinearTransformNode* parentTransformNode = outputModelNode->GetParentTransformNode();
      if ((outputTransformNode != parentTransformNode) || (not parentTransformNode))
        {
        outputModelNode->SetAndObserveTransformNodeID(outputTransformNode->GetID());
        }
      double *crosshairPosition = crosshairNode->GetCrosshairRAS();
      vtkNew<vtkMatrix4x4> positionTransformMatrix;
      positionTransformMatrix->SetElement(0,3, crosshairPosition[0]);
      positionTransformMatrix->SetElement(1,3, crosshairPosition[1]);
      positionTransformMatrix->SetElement(2,3, crosshairPosition[2]);
      outputTransformNode->SetMatrixTransformToParent(positionTransformMatrix.GetPointer());
      outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::TransformModifiedEvent);
      }

  return true;
}

