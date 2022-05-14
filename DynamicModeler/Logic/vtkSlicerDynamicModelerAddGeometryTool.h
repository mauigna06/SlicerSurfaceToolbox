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

#ifndef __vtkSlicerDynamicModelerAddGeometryTool_h
#define __vtkSlicerDynamicModelerAddGeometryTool_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// STD includes
#include <map>
#include <string>
#include <vector>

class vtkAppendPolyData;
class vtkCleanPolyData;
class vtkGeneralTransform;
class vtkPolyData;
class vtkTransformPolyDataFilter;
class vtkArrowSource;
//class vtkBoundedPointSource;
class vtkCapsuleSource;
class vtkConeSource;
class vtkCubeSource;
class vtkCylinderSource;
class vtkDiskSource;
class vtkEllipseArcSource;
//class vtkFrustumSource;
//class vtkLineSource;
class vtkParametricEllipsoid;
//class vtkParametricSuperEllipsoid;
//class vtkParametricSuperToroid;
class vtkParametricTorus;
class vtkPlaneSource;
//class vtkPlatonicSolidSource;
class vtkPointSource;
class vtkRegularPolygonSource;
//class vtkSectorSource;
class vtkSphereSource;
//class vtkSuperquadricSource;
class vtkTextSource;
//class vtkTessellatedBoxSource;

#include "vtkSlicerDynamicModelerTool.h"

/// \brief Dynamic modeler tool for cutting a single surface mesh with planes.
///
/// Has two node inputs (Plane and Surface), and two outputs (Positive/Negative direction surface segments).
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerAddGeometryTool : public vtkSlicerDynamicModelerTool
{
public:
  static vtkSlicerDynamicModelerAddGeometryTool* New();
  vtkSlicerDynamicModelerTool* CreateToolInstance() override;
  vtkTypeMacro(vtkSlicerDynamicModelerAddGeometryTool, vtkSlicerDynamicModelerTool);

  /// Human-readable name of the mesh modification tool
  const char* GetName() override;

  /// Run the plane cut on the input model node
  bool RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode) override;

protected:
  vtkSlicerDynamicModelerAddGeometryTool();
  ~vtkSlicerDynamicModelerAddGeometryTool() override;
  void operator=(const vtkSlicerDynamicModelerAddGeometryTool&);

protected:
  // Cache output meshes to minimize need for memory reallocation.
  vtkSmartPointer<vtkPolyData> modelGeometry;

  // Cache filters that are expensive to initialize
  vtkSmartPointer<vtkPolyDataAlgorithm> polydataSource;

  vtkSmartPointer<vtkGeneralTransform>        OutputWorldToModelTransform;
  vtkSmartPointer<vtkTransformPolyDataFilter> OutputWorldToModelTransformFilter;

private:
  vtkSlicerDynamicModelerAddGeometryTool(const vtkSlicerDynamicModelerAddGeometryTool&) = delete;
};

#endif // __vtkSlicerDynamicModelerAddGeometryTool_h
