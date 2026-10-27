/*==============================================================================

  Program: 3D Slicer

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#include "vtkACVDRemeshFilter.h"

// VTK includes
#include <vtkCellArray.h>
#include <vtkCellArrayIterator.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>

// STD includes
#include <cstdlib>
#include <iostream>

namespace
{
//----------------------------------------------------------------------------
// Check that the output contains only triangles and, if requested, that all of
// them face away from the origin (the center of the input sphere).
bool CheckTriangles(vtkPolyData* mesh, const char* testName, bool checkOutward)
{
  if (mesh->GetNumberOfVerts() || mesh->GetNumberOfLines() || mesh->GetNumberOfStrips())
  {
    std::cerr << testName << ": output must only contain triangles" << std::endl;
    return false;
  }
  vtkSmartPointer<vtkCellArrayIterator> cellIterator = vtk::TakeSmartPointer(mesh->GetPolys()->NewIterator());
  for (cellIterator->GoToFirstCell(); !cellIterator->IsDoneWithTraversal(); cellIterator->GoToNextCell())
  {
    vtkIdType numberOfCellPoints = 0;
    const vtkIdType* cellPointIds = nullptr;
    cellIterator->GetCurrentCell(numberOfCellPoints, cellPointIds);
    if (numberOfCellPoints != 3)
    {
      std::cerr << testName << ": found a cell with " << numberOfCellPoints << " points" << std::endl;
      return false;
    }
    if (!checkOutward)
    {
      continue;
    }
    double p0[3], p1[3], p2[3];
    mesh->GetPoint(cellPointIds[0], p0);
    mesh->GetPoint(cellPointIds[1], p1);
    mesh->GetPoint(cellPointIds[2], p2);
    double e0[3], e1[3], normal[3];
    vtkMath::Subtract(p1, p0, e0);
    vtkMath::Subtract(p2, p0, e1);
    vtkMath::Cross(e0, e1, normal);
    const double centroid[3] = { p0[0] + p1[0] + p2[0], p0[1] + p1[1] + p2[1], p0[2] + p1[2] + p2[2] };
    if (vtkMath::Dot(normal, centroid) <= 0)
    {
      std::cerr << testName << ": triangle " << cellIterator->GetCurrentCellId() << " faces inward" << std::endl;
      return false;
    }
  }
  return true;
}

//----------------------------------------------------------------------------
bool CheckNumberOfPoints(vtkPolyData* mesh, const char* testName, vtkIdType expected)
{
  if (mesh->GetNumberOfPoints() != expected)
  {
    std::cerr << testName << ": expected " << expected << " points, got " << mesh->GetNumberOfPoints() << std::endl;
    return false;
  }
  return true;
}

} // namespace

//----------------------------------------------------------------------------
int vtkACVDRemeshFilterTest(int vtkNotUsed(argc), char* vtkNotUsed(argv)[])
{
  vtkNew<vtkSphereSource> sphere;
  sphere->SetThetaResolution(100);
  sphere->SetPhiResolution(100);
  sphere->Update();
  const vtkIdType numberOfSpherePoints = sphere->GetOutput()->GetNumberOfPoints();

  vtkNew<vtkACVDRemeshFilter> remesher;
  remesher->SetInputConnection(sphere->GetOutputPort());

  // Default clustering gives one output point per cluster
  remesher->SetNumberOfClusters(2000);
  remesher->Update();
  if (!CheckNumberOfPoints(remesher->GetOutput(), "Default", 2000) || !CheckTriangles(remesher->GetOutput(), "Default", true))
  {
    return EXIT_FAILURE;
  }

  // Subdividing allows more clusters than input points
  remesher->SetNumberOfSubdivisions(1);
  remesher->SetNumberOfClusters(20000);
  remesher->Update();
  if (!CheckNumberOfPoints(remesher->GetOutput(), "Subdivided", 20000) || !CheckTriangles(remesher->GetOutput(), "Subdivided", true))
  {
    return EXIT_FAILURE;
  }

  // With more clusters than points every point is a cluster
  remesher->SetNumberOfSubdivisions(0);
  remesher->SetNumberOfClusters(100000);
  remesher->Update();
  if (!CheckNumberOfPoints(remesher->GetOutput(), "More clusters than points", numberOfSpherePoints)
      || !CheckTriangles(remesher->GetOutput(), "More clusters than points", true))
  {
    return EXIT_FAILURE;
  }

  // Fast clustering
  remesher->SetNumberOfClusters(2000);
  remesher->FastClusteringOn();
  remesher->Update();
  if (remesher->GetOutput()->GetNumberOfPoints() == 0)
  {
    std::cerr << "Fast: output must not be empty" << std::endl;
    return EXIT_FAILURE;
  }
  if (!CheckTriangles(remesher->GetOutput(), "Fast", false))
  {
    return EXIT_FAILURE;
  }
  remesher->FastClusteringOff();

  // Empty input gives empty output
  vtkNew<vtkPolyData> emptyPolyData;
  remesher->SetInputData(emptyPolyData);
  remesher->Update();
  if (!CheckNumberOfPoints(remesher->GetOutput(), "Empty", 0))
  {
    return EXIT_FAILURE;
  }

  std::cout << "vtkACVDRemeshFilterTest passed" << std::endl;
  return EXIT_SUCCESS;
}
