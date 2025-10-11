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

#ifndef vtkSlicerIsotropicRemesher_h
#define vtkSlicerIsotropicRemesher_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

#include <vtkObject.h>

#include <string>

class vtkPolyData;

/// \ingroup Slicer_QtModules_DynamicModeler
/// \brief Helper class that performs Botsch isotropic remeshing on vtkPolyData inputs.
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerIsotropicRemesher : public vtkObject
{
public:
  static vtkSlicerIsotropicRemesher* New();
  vtkTypeMacro(vtkSlicerIsotropicRemesher, vtkObject);

  /// Run isotropic remeshing on \a inputMesh and store the result in \a outputMesh.
  /// If \a targetEdgeLength is set to 0 or negative then the average edge length
  /// of the input mesh is used. Returns true on success. On failure, call
  /// GetLastErrorMessage() for details.
  bool Remesh(
    vtkPolyData* inputMesh,
    double targetEdgeLength,
    int iterationCount,
    bool projectToInput,
    vtkPolyData* outputMesh);

  /// Retrieve the last error message produced by Remesh()
  const std::string& GetLastErrorMessage() const;

protected:
  vtkSlicerIsotropicRemesher();
  ~vtkSlicerIsotropicRemesher() override;

private:
  vtkSlicerIsotropicRemesher(const vtkSlicerIsotropicRemesher&) = delete;
  void operator=(const vtkSlicerIsotropicRemesher&) = delete;

  void SetLastErrorMessage(const std::string& errorMessage);

  std::string LastErrorMessage;
};

#endif
