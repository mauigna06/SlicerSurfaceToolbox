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
#include <vtkMRMLMarkupsNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLMarkupsAngleNode.h>
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLLinearTransformNode.h>
//#include <vtkMRMLModelDisplayNode.h>
//#include <vtkMRMLCrosshairNode.h>

// VTK includes
#include <vtkMath.h>
#include <vtkAssignAttribute.h>
#include <vtkCommand.h>
//#include <vtkGeneralTransform.h>
#include <vtkTransform.h>
#include <vtkMatrix3x3.h>

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
  outputTransformFiducialClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  vtkNew<vtkStringArray> outputTransformAngleClassNames;
  outputTransformAngleClassNames->InsertNextValue("vtkMRMLMarkupsAngleNode");
  vtkNew<vtkStringArray> outputTransformPlaneClassNames;
  outputTransformPlaneClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  vtkNew<vtkStringArray> outputTransformLinearTransformClassNames;
  outputTransformLinearTransformClassNames->InsertNextValue("vtkMRMLLinearTransformNode");

  NodeInfo outputFiducial(
    "Final transform position",
    "Fiducial list with only one node corresponding to the translation part of the final transform.",
      outputTransformFiducialClassNames,
    TRANSFORM_MAKER_OUTPUT_FIDUCIAL_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputFiducial);

  NodeInfo outputAngle(
    "Final transform angle",
    "Final transform represented as angle, the axis of rotation goes throw second point of the angle and it's normal to it.",
    outputTransformAngleClassNames,
    TRANSFORM_MAKER_OUTPUT_ANGLE_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputAngle);

  NodeInfo outputPlane(
    "Final transform frame",
    "Final transform represented as plane/frame. The axes are aligned with the final rotation transform part and the origin coincides with the final translation transform part.",
    outputTransformPlaneClassNames,
    TRANSFORM_MAKER_OUTPUT_PLANE_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputPlane);

  NodeInfo outputLinearTransform(
    "Full final transform",
    "Final transform represented as a matrix4x4 inside this node.",
    outputTransformLinearTransformClassNames,
    TRANSFORM_MAKER_OUTPUT_LINEAR_TRANSFORM_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputLinearTransform);

  /////////
  // Parameters
  ParameterInfo parameterUseParentTranforms(
    "Use parentTransforms?",
    "Choose if you want parent transforms to be ignored.",
    "UseParentTranforms",
    PARAMETER_STRING_ENUM,
    "Ignore ParentTransforms");

  vtkNew<vtkStringArray> possibleValues;
  parameterUseParentTranforms.PossibleValues = possibleValues;
  parameterUseParentTranforms.PossibleValues->InsertNextValue("Ignore ParentTransforms");
  parameterUseParentTranforms.PossibleValues->InsertNextValue("Use ParentTransforms");
  this->InputParameterInfo.push_back(parameterUseParentTranforms);

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
  vtkMRMLLinearTransformNode* outputLinearTransformNode = vtkMRMLLinearTransformNode::SafeDownCast(surfaceEditorNode->GetNodeReference(TRANSFORM_MAKER_OUTPUT_LINEAR_TRANSFORM_ROLE));
  
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
  this->OutputTransform->Identity();
  for (int i = 0; i < numberOfInputNodes; i++)
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
        this->OutputTransform->Translate(position);
        }
    else if (mrmlNode && mrmlNode->IsA("vtkMRMLMarkupsAngleNode"))
    {
        vtkMRMLMarkupsAngleNode* inputNode = vtkMRMLMarkupsAngleNode::SafeDownCast(mrmlNode);
        if (inputNode->GetNumberOfControlPoints() != 3)
        {
            // Nothing to output
            continue;
        }
        double pt0[3] = { 0.0, 0.0, 0.0 };
        double pt1[3] = { 0.0, 0.0, 0.0 };
        double pt2[3] = { 0.0, 0.0, 0.0 };
        double rotationAxis[3] = { 0.0, 0.0, 0.0 };
        
        if (useParentTransformsBool)
            {
            inputNode->GetNthControlPointPositionWorld(0, pt0);
            inputNode->GetNthControlPointPositionWorld(1, pt1);
            inputNode->GetNthControlPointPositionWorld(2, pt2);
            }
        else
            {
            inputNode->GetNthControlPointPosition(0, pt0);
            inputNode->GetNthControlPointPosition(1, pt1);
            inputNode->GetNthControlPointPosition(2, pt2);
            }

        double pt2_1[3] = { 0.0, 0.0, 0.0 };
        pt2_1[0] = pt2[0] - pt1[0];
        pt2_1[1] = pt2[1] - pt1[1];
        pt2_1[2] = pt2[2] - pt1[2];

        double pt0_1[3] = { 0.0, 0.0, 0.0 };
        pt0_1[0] = pt0[0] - pt1[0];
        pt0_1[1] = pt0[1] - pt1[1];
        pt0_1[2] = pt0[2] - pt1[2];

        vtkMath::Cross(pt0_1, pt2_1, rotationAxis);
        vtkMath::Normalize(rotationAxis);

        double angleRadians = vtkMath::SignedAngleBetweenVectors(pt0_1, pt2_1, rotationAxis);
        double angleDegrees = vtkMath::DegreesFromRadians(angleRadians);
        this->OutputTransform->RotateWXYZ(angleDegrees, rotationAxis);
        }
    else if (mrmlNode && mrmlNode->IsA("vtkMRMLMarkupsPlaneNode"))
        {
        vtkMRMLMarkupsPlaneNode* inputNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(mrmlNode);
        if (!inputNode->GetIsPlaneValid())
        {
        // Nothing to output
        continue;
        }
        vtkMatrix4x4 *planeMatrix = vtkMatrix4x4::New();
        if (useParentTransformsBool)
            {
            inputNode->GetObjectToWorldMatrix(planeMatrix);
            }
        else
            {
            inputNode->GetObjectToNodeMatrix(planeMatrix);
            }
        this->OutputTransform->Concatenate(planeMatrix);
        }
    else if (mrmlNode && mrmlNode->IsA("vtkMRMLLinearTransformNode"))
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
        this->OutputTransform->Concatenate(linearTransformMatrix);
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

  this->OutputTransform->GetMatrix(this->OutputMatrix);
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
    double addd[4];
    this->OutputTransform->GetOrientationWXYZ(addd);

    double ctp1[3] = { 0, 0, 0 };
    if (outputAngleNode->GetNumberOfControlPoints() > 1)
        {
        outputAngleNode->GetNthControlPointPosition(1,ctp1);
        }

    double axis[3] = { 0.0, 0.0, 0.0 };
    double angle = addd[0];
    axis[0] = addd[1];
    axis[1] = addd[2];
    axis[2] = addd[3];
    
    double ctp0[3] = { 0, 0, 0};
    vtkMath::Perpendiculars(axis,ctp0,nullptr,0);
    vtkMath::Normalize(ctp0);
    vtkMath::MultiplyScalar(ctp0,50);

    double ctp0_aux[4] = { 0, 0, 0, 0};
    for(int i=0; i<3; i++)
      {
      ctp0_aux[i] = ctp0[i];
      }

    double ctp2_aux[4] = { 0, 0, 0, 0};
    this->OutputMatrix->MultiplyPoint(ctp0_aux,ctp2_aux);

    double ctp2[3] = { 0, 0, 0};
    for(int i=0; i<3; i++)
      {
      ctp2[i] = ctp2_aux[i];
      }

    vtkMath::Add(ctp1,ctp0,ctp0);
    vtkMath::Add(ctp1,ctp2,ctp2);

    MRMLNodeModifyBlocker blocker(outputAngleNode);
    //outputAngleNode->SetAngleMeasurementModeToOrientedSigned();
    outputAngleNode->RemoveAllControlPoints();
    //if (abs(abs(addd[0]) - abs(angle)) < 1)
    //    {
    outputAngleNode->AddControlPoint(ctp0);
    outputAngleNode->AddControlPoint(ctp1);
    outputAngleNode->AddControlPoint(ctp2);
    //    }
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
    outputPlaneNode->SetPlaneType(vtkMRMLMarkupsPlaneNode::PlaneType3Points);
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
    outputLinearTransformNode->InvokeCustomModifiedEvent(vtkMRMLLinearTransformNode::TransformModifiedEvent);
    }

    return true;
}

/*
double vtkSlicerDynamicModelerTransformMakerTool::rotation_from_matrix(vtkMatrix4x4 *matrix,double *axis, double *point)
{
    /*
    Return rotation angle and axis from rotation matrix.

    >>> angle = (random.random() - 0.5) * (2*math.pi)
    >>> direc = np.random.random(3) - 0.5
    >>> point = np.random.random(3) - 0.5
    >>> R0 = rotation_matrix(angle, direc, point)
    >>> angle, direc, point = rotation_from_matrix(R0)
    >>> R1 = rotation_matrix(angle, direc, point)
    >>> is_same_transform(R0, R1)
    True

    double R[3][3];
    double I[3][3];
    for(int i=0; i<3; i++)
      {
      for(int j=0; j<3; j++)
        {
        R[i][j] = matrix->GetElement(i,j);
        if (i==j)
          {
          I[i][j] = 1;
          }
        else
          {
          I[i][j] = 0;
          }
        }
      }

    //R = vtkMatrix3x3;
    //R = np.array(matrix, dtype=np.float64, copy=False)
    //R33 = R[:3, :3]

    // direction: unit eigenvector of R33 corresponding to eigenvalue of 1
    double eigvals[3];
    double eigvecs[3][3];
    vtkMath::Diagonalize3x3(R,eigvals,eigvecs);
    int minimumIndex = 0;
    double minimumDifference = 1e20;
    double currentDifference = 0;
    for(int i=0; i<3; i++)
      {
      currentDifference = abs(eigvals[i]-1);
      if (currentDifference < minimumDifference)
        {
        minimumIndex = i;
        minimumDifference = currentDifference;
        }
      }

    for(int i=0; i<3; i++)
      {
      axis[i] = eigvecs[i][minimumIndex];
      } 

    //l, W = np.linalg.eig(R33.T)
    //i = np.where(abs(np.real(l) - 1.0) < 1e-8)[0]
    //if not len(i):
    //    raise ValueError("no unit eigenvector corresponding to eigenvalue 1")
    //direction = np.real(W[:, i[-1]]).squeeze()
    
    // point: unit eigenvector of R33 corresponding to eigenvalue of 1

    double translation[3];
    vtkMatrix3x3 *ImR_inv = vtkMatrix3x3::New();
    for(int i=0; i<3; i++)
      {
      for(int j=0; j<3; j++)
        {
        ImR_inv->SetElement(i,j,I[i][j] - R[i][j]);
        }
      translation[i] = matrix->GetElement(i,3);
      }
    
    ImR_inv->Invert();
    ImR_inv->MultiplyPoint(translation, point);

    /*
    double diffTranslationPoint[3];
    vtkMath::Subtract(translation,point,diffTranslationPoint);

    double normalVector[3];
    vtkMath::Cross(diffTranslationPoint,axis,normalVector);
    double quadratureComponent = vtkMath::Norm(normalVector,3);
    vtkMath::Normalize(diffTranslationPoint);
    vtkMath::MultiplyScalar(diffTranslationPoint,quadratureComponent);
    vtkMath::Subtract(translation,diffTranslationPoint,point);
 

    //l, Q = np.linalg.eig(R)
    //i = np.where(abs(np.real(l) - 1.0) < 1e-8)[0]
    //if not len(i):
    //    raise ValueError("no unit eigenvector corresponding to eigenvalue 1")
    //point = np.real(Q[:, i[-1]]).squeeze()
    //point /= point[3]
    //# rotation angle depending on direction

    double cosa = (R[0][0]+R[1][1]+R[2][2]-1)/2.0;
    double sina;
    if (abs(axis[2]) > 1e-8)
      {
      sina = (R[1][0] + (cosa - 1.0) * axis[0] * axis[1]) / axis[2];
      }
    else if (abs(axis[1]) > 1e-8)
      {
      sina = (R[0][2] + (cosa - 1.0) * axis[0] * axis[2]) / axis[1];
      }
    else
      {
      sina = (R[2][1] + (cosa - 1.0) * axis[1] * axis[2]) / axis[0];
      }
    
    double angle = atan2(sina, cosa);
    
    //cosa = (np.trace(R33) - 1.0) / 2.0
    //if abs(direction[2]) > 1e-8:
    //    sina = (R[1, 0] + (cosa - 1.0) * direction[0] * direction[1]) / direction[2]
    //elif abs(direction[1]) > 1e-8:
    //    sina = (R[0, 2] + (cosa - 1.0) * direction[0] * direction[2]) / direction[1]
    //else:
    //    sina = (R[2, 1] + (cosa - 1.0) * direction[1] * direction[2]) / direction[0]
    //angle = math.atan2(sina, cosa)
    
    return angle;
}

*/