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

#include "vtkSlicerDynamicModelerAddGeometryTool.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLMarkupsClosedCurveNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkArrowSource.h>
#include <vtkCapsuleSource.h>
#include <vtkConeSource.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkDiskSource.h>
#include <vtkEllipseArcSource.h>
#include <vtkParametricEllipsoid.h>
#include <vtkParametricTorus.h>
#include <vtkPlaneSource.h>
#include <vtkPointSource.h>
#include <vtkRegularPolygonSource.h>
#include <vtkSphereSource.h>
#include <vtkTextSource.h>
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransformPolyDataFilter.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerAddGeometryTool);

const char* APPEND_INPUT_MODEL_REFERENCE_ROLE = "Append.InputModel";
const char* APPEND_OUTPUT_MODEL_REFERENCE_ROLE = "Append.OutputModel";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerAddGeometryTool::vtkSlicerDynamicModelerAddGeometryTool()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputModelEvents;
  inputModelEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLModelNode::MeshModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputModelClassNames;
  inputModelClassNames->InsertNextValue("vtkMRMLModelNode");
  NodeInfo inputModel(
    "Model",
    "Model to be appended to the output.",
    inputModelClassNames,
    APPEND_INPUT_MODEL_REFERENCE_ROLE,
    true,
    true,
    inputModelEvents
    );
  this->InputNodeInfo.push_back(inputModel);

  /////////
  // Outputs
  NodeInfo outputModel(
    "Appended model",
    "Output model combining the input models.",
    inputModelClassNames,
    APPEND_OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputModel);

  /////////
  // Parameters
  ParameterInfo GeometrySource(
    "Geometry Source",
    "Method used to create the geometry.",
    "GeometrySource",
    PARAMETER_STRING_ENUM,
    "CubeSource",
    "");

  vtkNew<vtkStringArray> possibleValues;
  GeometrySource.PossibleValues = possibleValues;
  GeometrySource.PossibleValues->InsertNextValue("ArrowSource");
  GeometrySource.PossibleValues->InsertNextValue("CapsuleSource");
  GeometrySource.PossibleValues->InsertNextValue("ConeSource");
  GeometrySource.PossibleValues->InsertNextValue("CubeSource");
  GeometrySource.PossibleValues->InsertNextValue("CylinderSource");
  GeometrySource.PossibleValues->InsertNextValue("DiskSource");
  GeometrySource.PossibleValues->InsertNextValue("EllipseArcSource");
  GeometrySource.PossibleValues->InsertNextValue("ParametricEllipsoid");
  GeometrySource.PossibleValues->InsertNextValue("ParametricTorus");
  GeometrySource.PossibleValues->InsertNextValue("PlaneSource");
  GeometrySource.PossibleValues->InsertNextValue("PointSource");
  GeometrySource.PossibleValues->InsertNextValue("RegularPolygonSource");
  GeometrySource.PossibleValues->InsertNextValue("SphereSource");
  GeometrySource.PossibleValues->InsertNextValue("TextSource");
  this->InputParameterInfo.push_back(GeometrySource);
  
  //ArrowSource
  ParameterInfo parameterSelectionDistance(
    "Tip Length",
    "Set the length, and radius of the tip.",
    "ArrowSourceTipLength",
    PARAMETER_DOUBLE,
    5.0,
    "");
  this->InputParameterInfo.push_back(parameterSelectionDistance);


  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->SelectionScalarsOutputMesh = vtkSmartPointer<vtkPolyData>::New();
  this->SelectedFacesOutputMesh = vtkSmartPointer<vtkPolyData>::New();

  this->InputMeshLocator_World = vtkSmartPointer<vtkPointLocator>::New();

  this->GeodesicDistance = vtkSmartPointer<vtkFastMarchingGeodesicDistance>::New();

  this->OutputSelectionScalarsModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputSelectionScalarsModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputSelectionScalarsModelTransformFilter->SetTransform(this->OutputSelectionScalarsModelTransform);

  this->OutputSelectedFacesModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputSelectedFacesModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputSelectedFacesModelTransformFilter->SetTransform(this->OutputSelectedFacesModelTransform);

  this->SelectionArray = vtkSmartPointer<vtkUnsignedCharArray>::New();
  this->SelectionArray->SetName(SELECTION_ARRAY_NAME);

  this->AppendFilter = vtkSmartPointer<vtkAppendPolyData>::New();

  this->CleanFilter = vtkSmartPointer<vtkCleanPolyData>::New();
  this->CleanFilter->SetInputConnection(this->AppendFilter->GetOutputPort());

  this->OutputWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputWorldToModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputWorldToModelTransformFilter->SetInputConnection(this->CleanFilter->GetOutputPort());
  this->OutputWorldToModelTransformFilter->SetTransform(this->OutputWorldToModelTransform);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerAddGeometryTool::~vtkSlicerDynamicModelerAddGeometryTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerAddGeometryTool::GetName()
{
  return "Append";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerAddGeometryTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(APPEND_OUTPUT_MODEL_REFERENCE_ROLE));
  if (!outputModelNode)
    {
    // Nothing to output
    return true;
    }

  int numberOfInputNodes = surfaceEditorNode->GetNumberOfNodeReferences(APPEND_INPUT_MODEL_REFERENCE_ROLE);
  if (numberOfInputNodes < 1)
    {
    // Nothing to append
    return true;
    }

  this->AppendFilter->RemoveAllInputs();
  for (int i = 0; i < numberOfInputNodes; ++i)
    {
    vtkMRMLNode* inputNode = surfaceEditorNode->GetNthNodeReference(APPEND_INPUT_MODEL_REFERENCE_ROLE, i);
    vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(inputNode);
    if (!modelNode)
      {
      continue;
      }

    vtkNew<vtkGeneralTransform> modelToWorldTransform;
    if (modelNode->GetParentTransformNode())
      {
      modelNode->GetParentTransformNode()->GetTransformToWorld(modelToWorldTransform);
      }

    vtkNew<vtkTransformPolyDataFilter> modelToWorldTransformFilter;
    modelToWorldTransformFilter->SetInputData(modelNode->GetPolyData());
    modelToWorldTransformFilter->SetTransform(modelToWorldTransform);
    this->AppendFilter->AddInputConnection(modelToWorldTransformFilter->GetOutputPort());
    }

  if (outputModelNode->GetParentTransformNode())
    {
    outputModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
    }
  else
    {
    this->OutputWorldToModelTransform->Identity();
    }
  this->OutputWorldToModelTransformFilter->Update();

  vtkNew<vtkPolyData> outputPolyData;
  outputPolyData->DeepCopy(this->OutputWorldToModelTransformFilter->GetOutput());
  this->RemoveDuplicateCells(outputPolyData);

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObservePolyData(outputPolyData);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}