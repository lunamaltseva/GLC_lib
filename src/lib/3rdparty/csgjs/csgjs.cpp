// Original CSG.JS library by Evan Wallace (http://madebyevan.com), under the MIT license.
// GitHub: https://github.com/evanw/csg.js/
//
// C++ port by Tomasz Dabrowski (http://28byteslater.com), under the MIT license.
// GitHub: https://github.com/dabroz/csgjs-cpp/
//
// Using double instead of float
// Laurent Ribon (http://glc-lib.sourceforge.net)
// GitHub: https://github.com/laumaya/GLC_lib/
//
// Constructive Solid Geometry (CSG) is a modeling technique that uses Boolean
// operations like union and intersection to combine 3D solids. This library
// implements CSG operations on meshes elegantly and concisely using BSP trees,
// and is meant to serve as an easily understandable implementation of the
// algorithm. All edge cases involving overlapping coplanar polygons in both
// solids are correctly handled.
//
// To use this as a header file, define CSGJS_HEADER_ONLY before including this file.
//

#include "csgjs.h"
#include <vector>

// `CSG.Plane.EPSILON` is the tolerance used by `splitPolygon()` to decide if a
// point is on the plane.
static const float csgjs_EPSILON = 0.00001;

struct csgjs_plane;
struct csgjs_polygon;
struct csgjs_node;

// Represents a plane in 3D space.
struct csgjs_plane
{
    csgjs_vector normal;
    double w;

    csgjs_plane();
    csgjs_plane(const csgjs_vector & a, const csgjs_vector & b, const csgjs_vector & c);
    bool ok() const;
    void flip();
    void splitPolygon(const csgjs_polygon & polygon, QVector<csgjs_polygon>& coplanarFront
                      , QVector<csgjs_polygon>& coplanarBack, QVector<csgjs_polygon>& front
                      , QVector<csgjs_polygon>& back) const;
};

// Represents a convex polygon. The vertices used to initialize a polygon must
// be coplanar and form a convex loop. They do not have to be `CSG.Vertex`
// instances but they must behave similarly (duck typing can be used for
// customization).
//
// Each convex polygon has a `shared` property, which is shared between all
// polygons that are clones of each other or were split from the same polygon.
// This can be used to define per-polygon properties (such as surface color).
struct csgjs_polygon
{
    QVector<csgjs_vertex> vertices;
    csgjs_plane plane;
    void flip();

    csgjs_polygon();
    csgjs_polygon(const QVector<csgjs_vertex>& list);
};

// Holds a node in a BSP tree. A BSP tree is built from a collection of polygons
// by picking a polygon to split along. That polygon (and all other coplanar
// polygons) are added directly to that node and the other polygons are added to
// the front and/or back subtrees. This is not a leafy BSP tree since there is
// no distinction between internal and leaf nodes.
struct csgjs_csgnode
{
    QVector<csgjs_polygon> polygons;
    csgjs_csgnode * front;
    csgjs_csgnode * back;
    csgjs_plane plane;

    csgjs_csgnode();
    csgjs_csgnode(const QVector<csgjs_polygon> & polygons);
    ~csgjs_csgnode();

    csgjs_csgnode * clone() const;
    void clipTo(const csgjs_csgnode * other);
    void invert();
    void build(const QVector<csgjs_polygon>& polygon);
    QVector<csgjs_polygon> clipPolygons(const QVector<csgjs_polygon> & polygons) const;
    QVector<csgjs_polygon> allPolygons() const;
};

// Vector implementation

inline static csgjs_vector operator + (const csgjs_vector & a, const csgjs_vector & b) { return csgjs_vector(a.x + b.x, a.y + b.y, a.z + b.z); }
inline static csgjs_vector operator - (const csgjs_vector & a, const csgjs_vector & b) { return csgjs_vector(a.x - b.x, a.y - b.y, a.z - b.z); }
inline static csgjs_vector operator * (const csgjs_vector & a, double b) { return csgjs_vector(a.x * b, a.y * b, a.z * b); }
inline static csgjs_vector operator / (const csgjs_vector & a, double b) { return a * (1.0 / b); }
inline static double dot(const csgjs_vector & a, const csgjs_vector & b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline static csgjs_vector lerp(const csgjs_vector & a, const csgjs_vector & b, double v) { return a + (b - a) * v; }
inline static csgjs_vector negate(const csgjs_vector & a) { return a * -1.0; }
inline static double length(const csgjs_vector & a) { return sqrt(dot(a, a)); }
inline static csgjs_vector unit(const csgjs_vector & a) { return a / length(a); }
inline static csgjs_vector cross(const csgjs_vector & a, const csgjs_vector & b) { return csgjs_vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }

// Vertex implementation

// Create a new vertex between this vertex and `other` by linearly
// interpolating all properties using a parameter of `t`. Subclasses should
// override this to interpolate additional properties.
inline static csgjs_vertex interpolate(const csgjs_vertex & a, const csgjs_vertex & b, double t)
{
    csgjs_vertex ret;
    ret.pos = lerp(a.pos, b.pos, t);
    ret.normal = lerp(a.normal, b.normal, t);
    ret.uv = lerp(a.uv, b.uv, t);
    ret.matId= (a.matId < 0.5) ? a.matId : b.matId;
    return ret;
}

// Plane implementation

csgjs_plane::csgjs_plane() : normal(), w(0.0)
{
}

bool csgjs_plane::ok() const
{
    return length(this->normal) > 0.0;
}

void csgjs_plane::flip()
{
    this->normal = negate(this->normal);
    this->w *= -1.0;
}

csgjs_plane::csgjs_plane(const csgjs_vector & a, const csgjs_vector & b, const csgjs_vector & c)
{
    this->normal = unit(cross(b - a, c - a));
    this->w = dot(this->normal, a);
}

// Split `polygon` by this plane if needed, then put the polygon or polygon
// fragments in the appropriate lists. Coplanar polygons go into either
// `coplanarFront` or `coplanarBack` depending on their orientation with
// respect to this plane. Polygons in front or in back of this plane go into
// either `front` or `back`.
void csgjs_plane::splitPolygon(const csgjs_polygon & polygon, QVector<csgjs_polygon> & coplanarFront, QVector<csgjs_polygon> & coplanarBack, QVector<csgjs_polygon> & front, QVector<csgjs_polygon> & back) const
{
    enum
    {
        COPLANAR = 0,
        FRONT = 1,
        BACK = 2,
        SPANNING = 3
    };

    // Classify each point as well as the entire polygon into one of the above
    // four classes.
    int polygonType = 0;
    QList<int> types;

    for (int i = 0; i < polygon.vertices.size(); i++)
    {
        double t = dot(this->normal, polygon.vertices[i].pos) - this->w;
        int type = (t < -csgjs_EPSILON) ? BACK : ((t > csgjs_EPSILON) ? FRONT : COPLANAR);
        polygonType |= type;
        types.push_back(type);
    }

    // Put the polygon in the correct list, splitting it when necessary.
    switch (polygonType)
    {
    case COPLANAR:
        {
            if (dot(this->normal, polygon.plane.normal) > 0)
                coplanarFront.push_back(polygon);
            else
                coplanarBack.push_back(polygon);
            break;
        }
    case FRONT:
        {
            front.push_back(polygon);
            break;
        }
    case BACK:
        {
            back.push_back(polygon);
            break;
        }
    case SPANNING:
        {
            QVector<csgjs_vertex> f, b;
            for (int i = 0; i < polygon.vertices.size(); i++)
            {
                int j = (i + 1) % polygon.vertices.size();
                int ti = types[i], tj = types[j];
                csgjs_vertex vi = polygon.vertices[i], vj = polygon.vertices[j];
                if (ti != BACK) f.push_back(vi);
                if (ti != FRONT) b.push_back(vi);
                if ((ti | tj) == SPANNING)
                {
                    double t = (this->w - dot(this->normal, vi.pos)) / dot(this->normal, vj.pos - vi.pos);
                    csgjs_vertex v = interpolate(vi, vj, t);
                    f.push_back(v);
                    b.push_back(v);
                }
            }
            
            if (f.size() >= 3) front.push_back(csgjs_polygon(f));
            if (b.size() >= 3) back.push_back(csgjs_polygon(b));
            break;
        }
    }
}

// Polygon implementation

void csgjs_polygon::flip()
{
    std::reverse(vertices.begin(), vertices.end());
    for (int i = 0; i < vertices.size(); i++)
        vertices[i].normal = negate(vertices[i].normal);
    plane.flip();
}

csgjs_polygon::csgjs_polygon()
{
}

csgjs_polygon::csgjs_polygon(const QVector<csgjs_vertex> & list) : vertices(list), plane(vertices[0].pos, vertices[1].pos, vertices[2].pos)
{
}

// Node implementation

// Return a new CSG solid representing space in either this solid or in the
// solid `csg`. Neither this solid nor the solid `csg` are modified.
inline static csgjs_csgnode * csg_union(const csgjs_csgnode * a1, const csgjs_csgnode * b1)
{
    csgjs_csgnode * a = a1->clone();
    csgjs_csgnode * b = b1->clone();
    a->clipTo(b);
    b->clipTo(a);
    b->invert();
    b->clipTo(a);
    b->invert();
    a->build(b->allPolygons());
    csgjs_csgnode * ret = new csgjs_csgnode(a->allPolygons());
    delete a; a = 0;
    delete b; b = 0;
    return ret;
}

// Return a new CSG solid representing space in this solid but not in the
// solid `csg`. Neither this solid nor the solid `csg` are modified.
inline static csgjs_csgnode * csg_subtract(const csgjs_csgnode * a1, const csgjs_csgnode * b1)
{
    csgjs_csgnode * a = a1->clone();
    csgjs_csgnode * b = b1->clone();
    a->invert();
    a->clipTo(b);
    b->clipTo(a);
    b->invert();
    b->clipTo(a);
    b->invert();
    a->build(b->allPolygons());
    a->invert();
    csgjs_csgnode * ret = new csgjs_csgnode(a->allPolygons());
    delete a; a = 0;
    delete b; b = 0;
    return ret;
}

// Return a new CSG solid representing space both this solid and in the
// solid `csg`. Neither this solid nor the solid `csg` are modified.
inline static csgjs_csgnode * csg_intersect(const csgjs_csgnode * a1, const csgjs_csgnode * b1)
{
    csgjs_csgnode * a = a1->clone();
    csgjs_csgnode * b = b1->clone();
    a->invert();
    b->clipTo(a);
    b->invert();
    a->clipTo(b);
    b->clipTo(a);
    a->build(b->allPolygons());
    a->invert();
    csgjs_csgnode * ret = new csgjs_csgnode(a->allPolygons());
    delete a; a = 0;
    delete b; b = 0;
    return ret;
}

// Convert solid space to empty space and empty space to solid space.
void csgjs_csgnode::invert()
{
    for (int i = 0; i < this->polygons.size(); i++)
        this->polygons[i].flip();
    this->plane.flip();
    if (this->front) this->front->invert();
    if (this->back) this->back->invert();
    std::swap(this->front, this->back);
}

// Recursively remove all polygons in `polygons` that are inside this BSP
// tree.
QVector<csgjs_polygon> csgjs_csgnode::clipPolygons(const QVector<csgjs_polygon>& polygons) const
{
    if (!this->plane.ok()) return polygons;
    QVector<csgjs_polygon> front, back;
    for (int i = 0; i < polygons.size(); i++)
    {
        this->plane.splitPolygon(polygons[i], front, back, front, back);
    }
    if (this->front) front = this->front->clipPolygons(front);
    if (this->back) back = this->back->clipPolygons(back);
    else back.clear();

    // concatenate back into front
    front += back;
    return front;
}

// Remove all polygons in this BSP tree that are inside the other BSP tree
// `bsp`.
void csgjs_csgnode::clipTo(const csgjs_csgnode * other)
{
    this->polygons = other->clipPolygons(this->polygons);
    if (this->front) this->front->clipTo(other);
    if (this->back) this->back->clipTo(other);
}

// Return a list of all polygons in this BSP tree.
QVector<csgjs_polygon> csgjs_csgnode::allPolygons() const
{
    QVector<csgjs_polygon> polygons = this->polygons;
    QVector<csgjs_polygon> front, back;
    if (this->front) front = this->front->allPolygons();
    if (this->back) back = this->back->allPolygons();

    polygons += front;
    polygons += back;
    return polygons;
}

csgjs_csgnode * csgjs_csgnode::clone() const
{
    csgjs_csgnode * ret = new csgjs_csgnode();
    ret->polygons = this->polygons;
    ret->plane = this->plane;
    if (this->front) ret->front = this->front->clone();
    if (this->back) ret->back = this->back->clone();
    return ret;
}

// Build a BSP tree out of `polygons`. When called on an existing tree, the
// new polygons are filtered down to the bottom of the tree and become new
// nodes there. Each set of polygons is partitioned using the first polygon
// (no heuristic is used to pick a good split).
void csgjs_csgnode::build(const QVector<csgjs_polygon> & list)
{
    if (!list.size()) return;
    if (!this->plane.ok()) this->plane = list[0].plane;
    QVector<csgjs_polygon> front, back;
    for (int i = 0; i < list.size(); i++)
    {
        this->plane.splitPolygon(list[i], this->polygons, this->polygons, front, back);
    }
    if (front.size())
    {
        if (!this->front) this->front = new csgjs_csgnode;
        this->front->build(front);
    }
    if (back.size())
    {
        if (!this->back) this->back = new csgjs_csgnode;
        this->back->build(back);
    }
}

csgjs_csgnode::csgjs_csgnode() : front(0), back(0)
{
}

csgjs_csgnode::csgjs_csgnode(const QVector<csgjs_polygon>& polygons) : front(0), back(0)
{
    build(polygons);
}

csgjs_csgnode::~csgjs_csgnode()
{
    delete front;
    delete back;
}

// Public interface implementation

inline static QVector<csgjs_polygon> csgjs_modelToPolygons(const csgjs_model & model)
{
    QVector<csgjs_polygon> subject;
    for (int i = 0; i < model.indices.size(); i+= 3)
    {
        QVector<csgjs_vertex> triangle;
        for (int j = 0; j < 3; j++)
        {
            csgjs_vertex v = model.vertices[model.indices[i + j]];
            triangle.push_back(v);
        }
        subject.push_back(csgjs_polygon(triangle));
    }
    return subject;
}

inline static csgjs_model csgjs_modelFromPolygons(const QVector<csgjs_polygon> & polygons)
{
    csgjs_model model;
    int p = 0;
    for (int i = 0; i < polygons.size(); i++)
    {
        const csgjs_polygon & poly = polygons[i];
        for (int j = 2; j < poly.vertices.size(); j++)
        {
            model.vertices.push_back(poly.vertices[0]);		model.indices.push_back(p++);
            model.vertices.push_back(poly.vertices[j - 1]);	model.indices.push_back(p++);
            model.vertices.push_back(poly.vertices[j]);		model.indices.push_back(p++);
        }
    }
    return model;
}

typedef csgjs_csgnode * csg_function(const csgjs_csgnode * a1, const csgjs_csgnode * b1);

inline static csgjs_model csgjs_operation(const csgjs_model & a, const csgjs_model & b, csg_function fun)
{
    csgjs_csgnode * A = new csgjs_csgnode(csgjs_modelToPolygons(a));
    csgjs_csgnode * B = new csgjs_csgnode(csgjs_modelToPolygons(b));
    csgjs_csgnode * AB = fun(A, B);
    QVector<csgjs_polygon> polygons = AB->allPolygons();
    delete A; A = 0;
    delete B; B = 0;
    delete AB; AB = 0;
    return csgjs_modelFromPolygons(polygons);
}

csgjs_model csgjs_union(const csgjs_model & a, const csgjs_model & b)
{
    return csgjs_operation(a, b, csg_union);
}

csgjs_model csgjs_intersection(const csgjs_model & a, const csgjs_model & b)
{
    return csgjs_operation(a, b, csg_intersect);
}

csgjs_model csgjs_difference(const csgjs_model & a, const csgjs_model & b)
{
    return csgjs_operation(a, b, csg_subtract);
}
