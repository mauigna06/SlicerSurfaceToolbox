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

#ifndef __vtkSlicerDynamicModelerRemeshTool_h
#define __vtkSlicerDynamicModelerRemeshTool_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// DynamicModeler logic includes
#include "vtkSlicerDynamicModelerTool.h"

class vtkGeneralTransform;
class vtkMRMLDynamicModelerNode;
class vtkTransformPolyDataFilter;

// MRML includes
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

/// \ingroup Slicer_QtModules_DynamicModeler
/// \brief Remesh tool based on the Botsch isotropic remeshing algorithm.
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerRemeshTool : public vtkSlicerDynamicModelerTool
{
public:
  static vtkSlicerDynamicModelerRemeshTool* New();
  vtkTypeMacro(vtkSlicerDynamicModelerRemeshTool, vtkSlicerDynamicModelerTool);

  vtkSlicerDynamicModelerTool* CreateToolInstance() override;
  const char* GetName() override;

protected:
  vtkSlicerDynamicModelerRemeshTool();
  ~vtkSlicerDynamicModelerRemeshTool() override;

  bool RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode) override;

protected:
  vtkSmartPointer<vtkTransformPolyDataFilter> InputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform> InputModelNodeToWorldTransform;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputWorldToModelTransform;

private:
  vtkSlicerDynamicModelerRemeshTool(const vtkSlicerDynamicModelerRemeshTool&) = delete;
  void operator=(const vtkSlicerDynamicModelerRemeshTool&) = delete;
};

#endif
