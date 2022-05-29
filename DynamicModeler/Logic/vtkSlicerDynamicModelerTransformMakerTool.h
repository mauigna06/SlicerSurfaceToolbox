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

#ifndef __vtkSlicerDynamicModelerTransformMakerTool_h
#define __vtkSlicerDynamicModelerTransformMakerTool_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// VTK includes
#include <vtkSmartPointer.h>

class vtkGeneralTransform;
class vtkMRMLMarkupsNode;
class vtkMRMLMarkupsFiducialNode;
class vtkMRMLMarkupsAngleNode;
class vtkMRMLMarkupsPlaneNode;
class vtkMRMLLinearTransformNode;
class vtkPolyData;
class vtkTransform;
class vtkMatrix4x4;

#include "vtkSlicerDynamicModelerTool.h"

/// \brief Dynamic modelling tool to create transform from markups or other transforms.
///
/// The tool can concatenate any number of transforms created from the inputs.
/// The outputs can be a fiducialNode, a planeNode, an angleNode or a linear transformNode.
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerTransformMakerTool : public vtkSlicerDynamicModelerTool
{
public:
  static vtkSlicerDynamicModelerTransformMakerTool* New();
  vtkSlicerDynamicModelerTool* CreateToolInstance() override;
  vtkTypeMacro(vtkSlicerDynamicModelerTransformMakerTool, vtkSlicerDynamicModelerTool);

  /// Human-readable name of the mesh modification tool
  const char* GetName() override;

  /// Run the faces selection on the input model node
  bool RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode) override;

protected:
  vtkSlicerDynamicModelerTransformMakerTool();
  ~vtkSlicerDynamicModelerTransformMakerTool() override;
  void operator=(const vtkSlicerDynamicModelerTransformMakerTool&);

protected:
  // Cache output meshes to minimize need for memory reallocation.
  vtkSmartPointer<vtkTransform> OutputTransform;
  vtkSmartPointer<vtkMatrix4x4> OutputMatrix;

private:
  vtkSlicerDynamicModelerTransformMakerTool(const vtkSlicerDynamicModelerTransformMakerTool&) = delete;
};

#endif // __vtkSlicerDynamicModelerTransformMakerTool_h
