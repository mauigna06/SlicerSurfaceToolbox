#ifndef REMESH_MESH_H
#define REMESH_MESH_H

#include <vector>
#include <array>

class RemeshMesh
{
public:
    using Point = std::array<double, 3>;
    using Face = std::array<int, 3>;

    RemeshMesh();
    ~RemeshMesh();

    void AddVertex(const Point& vertex);
    void AddFace(const Face& face);

    const std::vector<Point>& GetVertices() const;
    const std::vector<Face>& GetFaces() const;

    void Clear();

private:
    std::vector<Point> Vertices;
    std::vector<Face> Faces;
};

#endif // REMESH_MESH_H