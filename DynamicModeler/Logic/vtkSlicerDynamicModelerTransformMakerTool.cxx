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

  This file was originally developed by Mauro I. Dominguez.

==============================================================================*/

#include "vtkSlicerDynamicModelerTransformMakerTool.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
//#include <vtkMRMLMarkupsFiducialNode.h>
//#include <vtkMRMLModelNode.h>
//#include <vtkMRMLTransformNode.h>
//#include <vtkMRMLModelDisplayNode.h>
//#include <vtkMRMLCrosshairNode.h>

// VTK includes
#include <vtkAssignAttribute.h>
#include <vtkCommand.h>
//#include <vtkGeneralTransform.h>
#include <vtkTransform.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerTransformMakerTool);

const char* TRANSFORM_MAKER_INPUT_TRANSFORM_SOURCE_REFERENCE_ROLE = "TransformMaker.TransformSource";
const char* TRANSFORM_MAKER_OUTPUT_FIDUCIAL_REFERENCE_ROLE = "TransformMaker.OutputFiducial";
const char* TRANSFORM_MAKER_OUTPUT_ANGLE_REFERENCE_ROLE = "TransformMaker.OutputAngle";
const char* TRANSFORM_MAKER_OUTPUT_PLANE_REFERENCE_ROLE = "TransformMaker.OutputPlane";
const char* TRANSFORM_MAKER_OUTPUT_LINEAR_TRANSFORM_ROLE = "TransformMaker.OutputLinearTransform";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerTransformMakerTool::vtkSlicerDynamicModelerTransformMakerTool()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputTransformSourcesEvents;
  inputTransformSourcesEvents->InsertNextValue(vtkCommand::ModifiedEvent);
  inputTransformSourcesEvents->InsertNextValue(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputTransformSourcesEvents->InsertNextValue(vtkMRMLLinearTransformNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputTransformSourcesClassNames;
  inputTransformSourcesClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  inputTransformSourcesClassNames->InsertNextValue("vtkMRMLMarkupsAngleNode");
  inputTransformSourcesClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  inputTransformSourcesClassNames->InsertNextValue("vtkMRMLLinearTransformNode");
  NodeInfo inputModel(
    "Input nodes",
    "The final transform will be calculated according to the inputs in post-multiply order.",
    inputTransformSourcesClassNames,
    TRANSFORM_MAKER_INPUT_TRANSFORM_SOURCE_REFERENCE_ROLE,
    true,
    true,
    inputTransformSourcesEvents
  );
  this->InputNodeInfo.push_back(inputModel);

  /////////
  // Outputs
  vtkNew<vtkStringArray> outputTransformFiducialClassNames;
  outputTransformSourcesClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  vtkNew<vtkStringArray> outputTransformAngleClassNames;
  outputTransformAngleClassNames->InsertNextValue("vtkMRMLMarkupsAngleNode");
  vtkNew<vtkStringArray> outputTransformPlaneClassNames;
  outputTransformPlaneClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  vtkNew<vtkStringArray> outputTransformLinearTransformClassNames;
  outputTransformLinearTransformClassNames->InsertNextValue("vtkMRMLLinearTransformNode");

  NodeInfo outputFiducial(
    "Final transform position.",
    "Fiducial list with only one node corresponding to the translation part of the final transform.",
    outputTransformSourcesClassNames,
    TRANSFORM_MAKER_OUTPUT_FIDUCIAL_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputFiducial);

  NodeInfo outputAngle(
    "Full final transform.",
    "Final transform represented as angle, the axis of rotation goes throw second point of the angle and it's normal to it.",
    outputTransformAngleClassNames,
    TRANSFORM_MAKER_OUTPUT_ANGLE_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputAngle);

  NodeInfo outputPlane(
    "Full final transform.",
    "Final transform represented as plane/frame. The axes are aligned with the final rotation transform part and the origin coincides with the final translation transform part.",
    outputTransformPlaneClassNames,
    TRANSFORM_MAKER_OUTPUT_PLANE_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputPlane);

  NodeInfo outputLinearTransform(
    "Full final transform.",
    "Final transform represented as a matrix4x4 inside this node.",
    outputTransformLinearTransformClassNames,
    TRANSFORM_MAKER_OUTPUT_LINEAR_TRANSFORM_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputLinearTransform);

  /////////
  // Parameters
  ParameterInfo parameterSelectionDistance(
    "Selection distance",
    "Selection distance of model's points to input fiducials.",
    "SelectionDistance",
    PARAMETER_DOUBLE,
    5.0);
  this->InputParameterInfo.push_back(parameterSelectionDistance);

  ParameterInfo parameterUseParentTranforms(
    "Use parentTransforms?",
    "Choose if you want parent transforms to be ignored.",
    "UseParentTranforms",
    PARAMETER_STRING_ENUM,
    "Ignore ParentTransforms");

  vtkNew<vtkStringArray> possibleValues;
  parameterSelectionAlgorithm.PossibleValues = possibleValues;
  parameterSelectionAlgorithm.PossibleValues->InsertNextValue("Ignore ParentTransforms");
  parameterSelectionAlgorithm.PossibleValues->InsertNextValue("Use ParentTransforms");
  this->InputParameterInfo.push_back(parameterSelectionAlgorithm);

  this->OutputTransform = vtkSmartPointer<vtkTransform>::New();
  this->OutputTransform->PostMultiply();
  this->OutputMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerTransformMakerTool::~vtkSlicerDynamicModelerTransformMakerTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerTransformMakerTool::GetName()
{
  return "Transform maker";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerTransformMakerTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLMarkupsFiducialNode* outputFiducialNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(surfaceEditorNode->GetNodeReference(TRANSFORM_MAKER_OUTPUT_FIDUCIAL_REFERENCE_ROLE));
  vtkMRMLMarkupsAngleNode* outputAngleNode = vtkMRMLMarkupsAngleNode::SafeDownCast(surfaceEditorNode->GetNodeReference(TRANSFORM_MAKER_OUTPUT_ANGLE_REFERENCE_ROLE));
  vtkMRMLMarkupsPlaneNode* outputPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(surfaceEditorNode->GetNodeReference(TRANSFORM_MAKER_OUTPUT_PLANE_REFERENCE_ROLE));
  vtkMRMLLinearTransformNode* outputLinearTransformNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(TRANSFORM_MAKER_OUTPUT_LINEAR_TRANSFORM_ROLE));
  
  if (!outputFiducialNode && !outputAngleNode && !outputPlaneNode && !outputLinearTransformNode)
    {
    // Nothing to output
    return true;
    }

  int numberOfInputNodes = surfaceEditorNode->GetNumberOfNodeReferences(TRANSFORM_MAKER_INPUT_TRANSFORM_SOURCE_REFERENCE_ROLE);
  if (numberOfInputNodes < 1)
    {
    // Nothing to do
    return true;
    }

  std::string useParentTransforms = this->GetNthInputParameterValue(0, surfaceEditorNode).ToString();

  bool useParentTransformsBool = useParentTransforms == "Use ParentTransforms";

  //this->AppendFilter->RemoveAllInputs();
  this->OutputTransform->Identity()
  for (int i = 0; i < numberOfInputNodes; ++i)
    {
    vtkMRMLNode* mrmlNode = surfaceEditorNode->GetNthNodeReference(TRANSFORM_MAKER_INPUT_TRANSFORM_SOURCE_REFERENCE_ROLE, i);
    
    if (mrmlNode && mrmlNode->IsA("vtkMRMLMarkupsFiducialNode"))
        {
        vtkMRMLMarkupsFiducialNode* inputNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(mrmlNode);
        if (inputNode->GetNumberOfControlPoints() == 0)
        {
        // Nothing to output
        continue;
        }
        double position[3] = { 0.0, 0.0, 0.0 };
        if (useParentTransformsBool)
            {
            inputNode->GetNthControlPointPositionWorld(0,position);
            }
        else
            {
            inputNode->GetNthControlPointPosition(0,position);
            }
        this->OutputTransform->Translate(position)
        }
    elif (mrmlNode && mrmlNode->IsA("vtkMRMLMarkupsAngleNode"))
        {
        vtkMRMLMarkupsAngleNode* inputNode = vtkMRMLMarkupsAngleNode::SafeDownCast(mrmlNode);
        if (inputNode->GetNumberOfControlPoints() != 3)
        {
        // Nothing to output
        continue;
        }
        double rotationAxis[3] = { 0.0, 0.0, 0.0 };
        inputNode->GetOrientationRotationAxis(rotationAxis);
        double angleDegrees = inputNode->GetAngleDegrees();
        this->OutputTransform->RotateWXYZ(angleDegrees,rotationAxis)
        if (useParentTransformsBool)
            {
                vtkMRMLLinearTransformNode* parentTransformNode = inputNode->GetParentTransformNode()
                if (parentTransformNode)
                    {
                    vtkMatrix4x4 *parentTransformMatrix = vtkMatrix4x4::New();
                    parentTransformNode->GetMatrixTransformToWorld(parentTransformMatrix);
                    this->OutputTransform->Concatenate(parentTransformMatrix)
                    }
            }
        }
    elif (mrmlNode && mrmlNode->IsA("vtkMRMLMarkupsPlaneNode"))
        {
        vtkMRMLMarkupsPlaneNode* inputNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(mrmlNode);
        if (inputNode->GetNumberOfControlPoints() != 3)
        {
        // Nothing to output
        continue;
        }
        vtkMatrix4x4 *planeMatrix = vtkMatrix4x4::New();
        if (useParentTransformsBool)
            {
            inputNode->GetObjectToWorldMatrix(planeMatrix)
            }
        else
            {
            inputNode->GetObjectToNodeMatrix(planeMatrix)
            }
        this->OutputTransform->Concatenate(planeMatrix)
        }
    elif (mrmlNode && mrmlNode->IsA("vtkMRMLLinearTransformNode"))
        {
        vtkMRMLLinearTransformNode* inputNode = vtkMRMLLinearTransformNode::SafeDownCast(mrmlNode);
        //if (inputNode->GetNumberOfControlPoints() != 3)
        //{
        // Nothing to output
        //continue;
        //}
        vtkMatrix4x4 *linearTransformMatrix = vtkMatrix4x4::New();
        if (useParentTransformsBool)
            {
            inputNode->GetMatrixTransformToWorld(linearTransformMatrix);
            }
        else
            {
            inputNode->GetMatrixTransformToParent(linearTransformMatrix);
            }
        this->OutputTransform->Concatenate(linearTransformMatrix)
        }
    else
        {
        continue;
        }
    }


  //bool success = false;
  //if (!success)
  //  {
  //  return false;
  //  }
  
  bool computeFiducialPoint = (outputFiducialNode != nullptr);
  bool computeAngle = (outputAngleNode != nullptr);
  bool computePlane = (outputPlaneNode != nullptr);
  bool computeLinearTransform = (outputLinearTransformNode != nullptr);

  this->OutputTransform->GetMatrix(this->OutputMatrix)
  if (computeFiducialPoint)
    {
    double ctp[3] = { 0, 0, 0};
    ctp[0] = this->OutputMatrix->GetElement(0,3);
    ctp[1] = this->OutputMatrix->GetElement(1,3);
    ctp[2] = this->OutputMatrix->GetElement(2,3);
    MRMLNodeModifyBlocker blocker(outputFiducialNode);
    outputFiducialNode->RemoveAllControlPoints();
    outputFiducialNode->AddControlPoint(ctp);
    outputFiducialNode->InvokeCustomModifiedEvent(vtkMRMLMarkupsNode::PointModifiedEvent);
    }

  if (computeAngle)
    {
    double ctp0[3] = { 50, 0, 0};
    double ctp1[3] = { 0, 0, 0};
    double ctp2[3] = { 0, 50, 0};
    double xVector[3] = { 0, 0, 0};

    this->OutputTransform->TransformPoint(ctp0,ctp0);
    this->OutputTransform->TransformPoint(ctp1,ctp1);

    xVector[0] = ctp0[0] - ctp1[0];
    xVector[1] = ctp0[1] - ctp1[1];
    xVector[2] = ctp0[2] - ctp1[2];

    this->OutputTransform->TransformVector(xVector,xVector);

    ctp2[0] = xVector[0] + ctp0[0];
    ctp2[1] = xVector[1] + ctp0[1];
    ctp2[2] = xVector[2] + ctp0[2];

    MRMLNodeModifyBlocker blocker(outputAngleNode);
    outputAngleNode->RemoveAllControlPoints();
    outputAngleNode->AddControlPoint(ctp0);
    outputAngleNode->AddControlPoint(ctp1);
    outputAngleNode->AddControlPoint(ctp2);
    outputAngleNode->InvokeCustomModifiedEvent(vtkMRMLMarkupsNode::PointModifiedEvent);
    }

  if (computePlane)
    {
    double ctp0[3] = { 0, 0, 0};
    double ctp1[3] = { 50, 0, 0};
    double ctp2[3] = { 0, 50, 0};
    
    this->OutputTransform->TransformPoint(ctp0,ctp0);
    this->OutputTransform->TransformPoint(ctp1,ctp1);
    this->OutputTransform->TransformPoint(ctp2,ctp2);

    MRMLNodeModifyBlocker blocker(outputPlaneNode);
    outputPlaneNode->SetPlaneType(vtkMRMLMarkupsPlaneNode::PlaneType3Points)
    outputPlaneNode->RemoveAllControlPoints();
    outputPlaneNode->AddControlPoint(ctp0);
    outputPlaneNode->AddControlPoint(ctp1);
    outputPlaneNode->AddControlPoint(ctp2);
    outputPlaneNode->InvokeCustomModifiedEvent(vtkMRMLMarkupsNode::PointModifiedEvent);
    }

  if (computeLinearTransform)
    {
    MRMLNodeModifyBlocker blocker(outputLinearTransformNode);
    outputLinearTransformNode->SetMatrixTransformToParent(this->OutputMatrix);
    outputAngleNode->InvokeCustomModifiedEvent(vtkMRMLLinearTransformNode::TransformModifiedEvent);
    }

    return true;
}
