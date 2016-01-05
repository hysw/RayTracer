#ifndef HYSW_RAYTRACER_IMPL_RECORD_H
#define HYSW_RAYTRACER_IMPL_RECORD_H
#include "raytracer.h"
#include "enums.h"

HYSW_RAYTRACER_NAMESPACE_START

/* passed from cpu to compute unit */
typedef struct Info_s {
	float fov;
	float viewToWorld[16];
} Info;

#ifdef __cplusplus
typedef int structid_t;
typedef unsigned short half_float_t;
#else
typedef int structid_t;
typedef half half_float_t;
#endif

// 32 byte
typedef struct Hit_s {
	structid_t next;
	int deposit; // deposit location for photons
	float px, py, pz; // position
	float fx, fy, fz; // light fraction
} Hit;

typedef struct PPMHit_s {
	structid_t next;
	float px, py, pz; // position
	// possible to reduce the size?
	float lx, ly, lz, r; // flux x,y,z, collection radius
	float fx, fy, fz; // light fraction
	float dx, dy, dz; // direction
	float nx, ny, nz; // normal
	// We probably should keep a good position accuracy
	// Note that direction and normal can be represented by using
	//   spherical coordinate with two half float
	// Also light fraction can be represented with 10*3 bits
	// BRDF Yeah!
} PPMHit;

typedef struct WorkerRay_s {
	// float nx, ny, nz; // normal
	float dx, dy, dz; // direction
	float fx, fy, fz; // fraction of light
} WorkerRay;

typedef struct WorkerUnit_s {
	int flags; int deposit;
	float ox, oy, oz; // origin
	float fx, fy, fz; // fraction of light
	WorkerRay reflectray; // also used as input ray
	WorkerRay refractray;
} WorkerUnit;

enum {
	WorkerUnit_DISCARD = 0x0000,
	WorkerUnit_HITTED  = 0x8000, // the current hit buffer can not be used, set by kernel
	WorkerUnit_SPAWN   = 0x8000, // spawn a new ray, set by kernel
	WorkerUnit_REFLECT = 0x0001,
	WorkerUnit_REFRACT = 0x0002,
};

#ifdef __OPENCL_VERSION__
	// opencl specific defs
	typedef float3 V3;
	typedef float3 P3;

	typedef struct Intersection_s {
		float  t;  // -1 if did not hit
		int    id; // object id of hited object
		int    mt; // material
		float3 at; // the point where ray hits
		float3 n;  // the normal vector
		float2 uv; // texture coordinate
	} Intersection;

	typedef struct Ray_s {
		float3 o, d; // origin and direction
	} Ray;

	typedef struct Env_s {
		int x,y,w,h;
		__global void* m;
		__global int*     spacemap;
		__global Hit*     photons;
	} Env;

	typedef struct Iterator_s {
		int o, i;
	} Iterator;

	struct MI{
		float16 mtx;
		float16 inv;
	};
#endif

HYSW_RAYTRACER_NAMESPACE_END

#endif
