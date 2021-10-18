/*==============================================================================

==============================================================================*/

#ifndef __vtkSlicerDynamicModelerSelectionTool_h
#define __vtkSlicerDynamicModelerSelectionTool_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// STD includes
#include <map>
#include <string>
#include <vector>

class vtkClipPolyData;
class vtkDataObject;
class vtkGeneralTransform;
class vtkGeometryFilter;
class vtkImplicitBoolean;
class vtkImplicitFunction;
class vtkMRMLDynamicModelerNode;
class vtkPlane;
class vtkPlaneCollection;
class vtkPolyData;
class vtkThreshold;
class vtkTransformPolyDataFilter;
class vtkDistancePolyDataFilter;

#include "vtkSlicerDynamicModelerTool.h"

/// \brief Dynamic modelling tool to select model's faces near fiducials 
///
/// Has two node inputs (Fiducials and Surface), and two outputs (surface with selectionScalars or cropped surface according to selection)
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerSelectionCutTool : public vtkSlicerDynamicModelerTool
{
public:
  static vtkSlicerDynamicModelerSelectionCutTool* New();
  vtkSlicerDynamicModelerTool* CreateToolInstance() override;
  vtkTypeMacro(vtkSlicerDynamicModelerSelectionCutTool, vtkSlicerDynamicModelerTool);

  /// Human-readable name of the mesh modification tool
  const char* GetName() override;

  /// Run the faces selection on the input model node
  bool RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode) override;

  /// Create an end cap on the clipped surface
  static void CreateEndCap(vtkPlaneCollection* planes, vtkPolyData* originalPolyData, vtkImplicitBoolean* cutFunction, vtkPolyData* outputEndCap);

protected:
  vtkSlicerDynamicModelerSelectionCutTool();
  ~vtkSlicerDynamicModelerSelectionCutTool() override;
  void operator=(const vtkSlicerDynamicModelerSelectionCutTool&);

protected:
  vtkSmartPointer<vtkTransformPolyDataFilter> InputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        InputModelNodeToWorldTransform;

  vtkSmartPointer<vtkClipPolyData>            PlaneClipper;

  vtkSmartPointer<vtkDistancePolyDataFilter>  ModelDistanceToFiducialsFilter;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputPositiveWorldToModelTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputPositiveWorldToModelTransform;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputNegativeWorldToModelTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputNegativeWorldToModelTransform;

private:
  vtkSlicerDynamicModelerSelectionCutTool(const vtkSlicerDynamicModelerSelectionCutTool&) = delete;
};

#endif // __vtkSlicerDynamicModelerSelectionCutTool_h