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

#ifndef BotschKobbeltRemesh_RemeshMesh_h
#define BotschKobbeltRemesh_RemeshMesh_h

#include <vtkSmartPointer.h>

class vtkPolyData;

namespace BotschKobbeltRemesh
{

/// Run isotropic remeshing using the Botsch/Kobbelt algorithm.
///
/// The input mesh is remeshed in-place in world coordinates. When
/// \p targetEdgeLength is non-positive the algorithm falls back to the
/// average edge length of the input. The remeshed surface is returned
/// via \p remeshedOutput.
bool Remesh(vtkPolyData* inputPolyData,
                        double targetEdgeLength,
                        int iterationCount,
                        double relaxation,
                        bool preserveBoundary,
                        vtkSmartPointer<vtkPolyData>& remeshedOutput);

} // namespace BotschKobbeltRemesh

#endif // BotschKobbeltRemesh_RemeshMesh_h