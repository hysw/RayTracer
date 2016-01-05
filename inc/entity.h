#ifndef HYSW_RAYTRACER_IMPL_ENTITY_H
#define HYSW_RAYTRACER_IMPL_ENTITY_H
#include "raytracer.h"
#include "enums.h"

#ifdef __cplusplus
#include <set>
#include <map>
#include <vector>
#include <cstdlib>
#include <cstring>
#endif

HYSW_RAYTRACER_NAMESPACE_START

#ifdef __cplusplus
namespace S {
#endif

typedef int Sptr;
typedef int Vptr;
typedef int SVptr;
typedef struct EStruct EStruct;
typedef struct EReturn EReturn;
typedef struct EGLobalInfo EGLobalInfo;

typedef struct EGlobal    EGlobal   ;
typedef struct EMaterial  EMaterial ;
typedef struct ETexture   ETexture  ;
typedef struct ECircle    ECircle   ;
typedef struct ESquare    ESquare   ;
typedef struct EPlanar    EPlanar   ;
typedef struct EPolygon   EPolygon  ;
typedef struct ESphere    ESphere   ;
typedef struct ECube      ECube     ;
typedef struct EHalfPlane EHalfPlane;
typedef struct ECuboid    ECuboid   ;
typedef struct ECompounde ECompounde;
typedef struct EList      EList     ;
typedef struct ETrMatrix  ETrMatrix ;
typedef struct EAABBTest  EAABBTest ;
typedef struct ESMaterial ESMaterial;

// the global information
struct EGLobalInfo {
	float fov;
	float viewToWorld[16];
	int photons_start;
};

// basic type
struct EStruct     { char tag; char reserved[3]; };
struct EScalar     { int flags; int align; int size; void* data; };
struct EReturn     { Sptr s; int ip; };

// root of the tree
struct EGlobal     { EStruct s; Sptr root; EGLobalInfo info; };

// primitive structure
struct EMaterial   { EStruct s; Vptr v[MATERIAL_END]; Sptr t[MATERIAL_END]; };
struct ETexture    { EStruct s; int w, h, t; Vptr v; }; // t = type

// primitive objects - 2d
struct EPlanar     { EStruct s; Vptr v[3], n[3], t[3]; };
struct EPolygon    { EStruct s; Vptr v[3], n[3], t[3]; int c; Vptr a; }; // c = count of vertices

// primitive objects - 3d
struct ESphere     { EStruct s; Vptr v; };
struct ECube       { EStruct s; Vptr v; }; // ?
struct EHalfPlane  { EStruct s; Vptr v; }; // ?
struct ECuboid     { EStruct s; Vptr v[2]; };

// compounded object
struct ECompounded { EStruct s; EReturn p; Sptr a, b; };

// hierarchical structure
struct EList       { EStruct s; EReturn p; int c; }; // c = count of structs
struct ETrMatrix   { EStruct s; EReturn p; Vptr m; Sptr c; }; // there are two matrices -_-
struct EAABBTest   { EStruct s; EReturn p; Vptr b; Sptr c; }; // bounding box test
struct ESMaterial  { EStruct s; EReturn p; Sptr m; Sptr c; }; // set material

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus



	class Entity;
	class SerializationMap {
	public:
		std::map<Entity*, int> entities;
		SerializationMap& add(Entity* e);
		int at(Entity* e) const;
		int compute_serialization(int base);
		static int serialize(std::vector<char>& buf, Entity* e);
	};


	class Entity {
	public:
		virtual void inject(SerializationMap& m) {}
		virtual void write(const SerializationMap& m, void* p) = 0;
		virtual int size() = 0;
		virtual int align() { return 4; }
	};

	class EStruct : public Entity {};
	class EScalar : public Entity {};

	class EBuffer : public EScalar {
	public:
		std::vector<char> v;
		virtual void write(const SerializationMap& m, void* p) {
			memcpy(p, &v[0], v.size());
		}
		virtual int size() { return v.size(); }
	};

	template<typename T, int N>
	class EVector : public EScalar {
	public:
		EVector(std::array<T, N> v_): v(v_){}
		std::array<T, N> v;
		virtual void write(const SerializationMap& m, void* p) {
			memcpy(p, &v[0], sizeof(T)*N);
		}
		virtual int size() { return sizeof(T)*N; }
	};



	struct EReturn { EStruct* s; int ip; };

	typedef EVector<std::array<float, 16>, 2> TrMtx;
	typedef EVector<float, 3> V3;
	typedef EVector<float, 2> V2;

	struct EList : public EStruct {
		std::vector<EStruct*> v;
		virtual void inject(SerializationMap& m) {
			for(auto i:v)
				m.add(i);
		}
		virtual void write(const SerializationMap& m, void* p) {
			S::EList tmp = {{ENTITY_List}, {}, (int)v.size()};
			*static_cast<S::EList*>(p) = tmp;
			p = static_cast<S::EList*>(p) + 1;
			for(auto i:v) {
				*static_cast<int*>(p) = m.at(i);
				p = static_cast<int*>(p) + 1;
			}
		}
		virtual int size() {
			return sizeof(S::EList) + 4*v.size();
		}
	};

	struct EPlanar : public EStruct {
	public:
		EPlanar(int tag_): tag(tag_), v(), n(), t() {}
		char tag;
		V3 *v[3], *n[3];
		V2 *t[3];
		virtual void inject(SerializationMap& m) {
			m.add(v[0]).add(v[1]).add(v[2])
			 .add(n[0]).add(n[1]).add(n[2])
			 .add(t[0]).add(t[1]).add(t[2]);
		}
		virtual void write(const SerializationMap& m, void* p) {
			S::EPlanar tmp = {{tag},
				m.at(v[0]), m.at(v[1]), m.at(v[2]),
				m.at(n[0]), m.at(n[1]), m.at(n[2]),
				m.at(t[0]), m.at(t[1]), m.at(t[2]),
			};
			*static_cast<S::EPlanar*>(p) = tmp;
		}
		virtual int size() {
			return sizeof(S::EPlanar);
		}
	};

	struct ETexture : public EStruct {
		int w, h, t; EBuffer* v;
		virtual void inject(SerializationMap& m) {
			m.add(v);
		}
		virtual void write(const SerializationMap& m, void* p) {
			S::ETexture tmp = {{ENTITY_Texture}, w, h, t, m.at(v)};
			*static_cast<S::ETexture*>(p) = tmp;
		}
		virtual int size() {
			return sizeof(S::ETexture);
		}
	};

	struct EMaterial : public EStruct {
		std::array<EScalar*, MATERIAL_END> v;
		std::array<ETexture*, MATERIAL_END> t;
		EMaterial(): v(), t() {}
		virtual void inject(SerializationMap& m) {
			for(EScalar* i:v) m.add(i);
			for(ETexture* i:t) m.add(i);
		}
		virtual void write(const SerializationMap& m, void* p) {
			S::EMaterial tmp = {{ENTITY_Material}};
			for(int i=0;i!=MATERIAL_END;++i)
				tmp.v[i] = m.at(v[i]);
			for(int i=0;i!=MATERIAL_END;++i)
				tmp.t[i] = m.at(t[i]);
			*static_cast<S::EMaterial*>(p) = tmp;
		}
		virtual int size() {
			return sizeof(S::EMaterial);
		}
	};

	struct ESMaterial : public EStruct {
		ESMaterial(EMaterial* mat_=0, EStruct* c_=0): mat(mat_), c(c_){}
		EMaterial* mat;
		EStruct* c;
		virtual void inject(SerializationMap& m) {
			m.add(mat).add(c);
		}
		virtual void write(const SerializationMap& m, void* p) {
			S::ESMaterial tmp = {{ENTITY_SMaterial}, {}, m.at(mat), m.at(c)};
			*static_cast<S::ESMaterial*>(p) = tmp;
		}
		virtual int size() {
			return sizeof(S::ESMaterial);
		}

	};

	struct ETrMatrix : public EStruct {
		ETrMatrix(TrMtx* mat_=0, EStruct* c_=0): mat(mat_), c(c_){}
		TrMtx* mat;
		EStruct* c;
		virtual void inject(SerializationMap& m) {
			m.add(mat).add(c);
		}
		virtual void write(const SerializationMap& m, void* p) {
			S::ETrMatrix tmp = {{ENTITY_TrMatrix}, {}, m.at(mat), m.at(c)};
			*static_cast<S::ETrMatrix*>(p) = tmp;
		}
		virtual int size() {
			return sizeof(S::ETrMatrix);
		}
	};

	struct EPolygon : public EStruct { V3 *v[3], *n[3], *t[3]; int c; std::vector<V2*> a; };
#endif


// TODO FIXME
// struct ETrgMesh    { Struct s; EReturn p; ... }; - full binary tree of triangles
// struct EHierarchy  { Struct s; EReturn p; ... };
// struct BVH         { Struct s; EReturn p; ... };
// struct Light       { Struct s; ... };

HYSW_RAYTRACER_NAMESPACE_END
#endif