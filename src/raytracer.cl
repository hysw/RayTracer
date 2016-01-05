// ==< global defs >=======================================================
#include "inc/raytracer.h"
#include "inc/constants.h"
#include "inc/enums.h"
#include "inc/record.h"
#include "inc/entity.h"

// ==< utilities >=========================================================
#include "src/utility/utils.cl"
#include "src/utility/matrix.cl"
#include "src/utility/fractal.cl"
#include "src/optics/optics.cl"
#include "src/intersect/ray.intersect.cl"
#include "src/intersect/planar.cl"
#include "src/intersect/triv.cl"

float3 txtu(float2 uv) {
	return (1+fractal_SierpinskiCarpet(uv.x, uv.y))/2;
}

float3 rgb24(unsigned int x) {
	float r = (x>>0) & 0xff;
	float g = (x>>8) & 0xff;
	float b = (x>>16) & 0xff;
	return (float3)(r,g,b)/255.0;
}

float3 samprgb24(Model m, int offset, int x, int y, int w, int h) {
	x = clamp(x, 0, w-1);
	y = clamp(y, 0, h-1);
	__global unsigned char* p = mptr(m, offset+((w*y)+x)*3);
	return (float3)(p[0], p[1], p[2])/255.0;
}

float3 texturef3(Model m, int offset, float2 uv) {
	ETexture txt = *((__global ETexture*)mptr(m, offset));
	if(txt.t == TEXTURE_PPMP6_RGB)
		uv.y = 1-uv.y;
	uv = uv*(float2)(txt.w-1, txt.h-1);
	int fx = floor(uv.x);
	int fy = floor(uv.y);
	float x = (uv.x-fx);
	float y = (uv.y-fy);
	float3 sff = samprgb24(m, txt.v, fx,   fy,   txt.w, txt.h);
	float3 sfc = samprgb24(m, txt.v, fx,   fy+1, txt.w, txt.h);
	float3 scf = samprgb24(m, txt.v, fx+1, fy,   txt.w, txt.h);
	float3 scc = samprgb24(m, txt.v, fx+1, fy+1, txt.w, txt.h);

	return mix(mix(sff,sfc,y), mix(scf,scc,y), x);
}

float3 materialf3(Model m, int offset, int type, float2 uv) {
	EMaterial mt = *((__global EMaterial*)mptr(m, offset));
	if(mt.t[type] && mt.v[type]) {
		return texturef3(m, mt.t[type], uv)*loadf3(m.m, mt.v[type]);
	} else if(mt.t[type]) {
		return texturef3(m, mt.t[type], uv);
	} else if(mt.v[type]) {
		return loadf3(m.m, mt.v[type]);
	} else {
		return 0;
	}
}

// __kernel void hittest(__global void* m, __global float4* out) {
// 	__global EGlobal* g = offptr(m, 4);
//
// 	Env env = {get_global_id(0), get_global_id(1), get_global_size(0), get_global_size(1), m};
// 	Ray ray = spawnRayDirection(&g->info, 1.0*env.x/env.w, 1.0*env.y/env.h);
// 	float3 c = 0;
// 	Intersection it = { -1 };
// 	if(intersect(env, ray, &it))
// 		c = 1;//txtu(it.uv);
// 	if(it.mt)
// 		c = c * materialf3(env, it.mt, MATERIAL_DIFFUSE, it.uv);
// 	out[env.y*env.w+env.x] = clamp((float4)(c, 0), 0.0, 1.0);
// }


















int getarrayoffset(Env env) {
	return get_global_id(1)*get_global_size(0) + get_global_id(0);
}

__kernel void memzero(__global unsigned char* p, int chunksize) {
	p = p + get_global_id(0)*chunksize;
	for(;chunksize--;++p) *p = 0;
};
// uvmap_TODO(it->uv)*flux;
void putphoton(__global int* spacemap, __global Hit* photons, int off) {
	Hit h = photons[off];
	int shash = spacehash(intloc((float3)(h.px, h.py, h.pz)));
	int link = 0, newlink = 0;
	do {
		link = spacemap[shash]; photons[off].next = link;
	} while(atomic_cmpxchg(spacemap+shash, link, off*2|1) != link);
}

__kernel void hitmap(__global int* spacemap, __global Hit* photons) {
	putphoton(spacemap, photons, get_global_id(0));
}

__kernel void raycast(__global void* m, __global WorkerUnit* workers) {
	__global EGlobal* g = offptr(m, 4);
	Env env = {get_global_id(0), get_global_id(1), get_global_size(0), get_global_size(1), m};
	// photons_start
	int bufferindex = getarrayoffset(env);
	WorkerUnit worker = workers[bufferindex];
	float3 flux = (float3)(worker.reflectray.fx, worker.reflectray.fy, worker.reflectray.fz);
	Ray ray = {
		(float3)(worker.ox, worker.oy, worker.oz),
		(float3)(worker.reflectray.dx, worker.reflectray.dy, worker.reflectray.dz)
	};
	if(worker.flags & WorkerUnit_SPAWN)
		ray = spawnRayDirection(&g->info, ray.d.x, ray.d.y);
	worker.flags = 0;

	int recn = 0;
	Intersection it = {-1};
	intersect(env, ray, &it);
	if(!(it.t > 0)) {
		workers[bufferindex] = worker;
		return;
	}

	worker.ox = it.at.x;
	worker.oy = it.at.y;
	worker.oz = it.at.z;
	// compute the flux for diffused light
	float3 tflux = flux * materialf3(env, it.mt, MATERIAL_DIFFUSE, it.uv);
	if(length(tflux) > LOWESTFLUX_DIFFUSE){ worker.flags |= WorkerUnit_HITTED;
		worker.fx = tflux.x;
		worker.fy = tflux.y;
		worker.fz = tflux.z;
	}
	tflux = flux * materialf3(env, it.mt, MATERIAL_REFLECT, it.uv);
	if(length(tflux) > LOWESTFLUX_REFLECT) { worker.flags |= WorkerUnit_REFLECT;
		float3 d = reflected(ray.d, it.n);
		worker.reflectray.fx = tflux.x;
		worker.reflectray.fy = tflux.y;
		worker.reflectray.fz = tflux.z;
		worker.reflectray.dx = d.x;
		worker.reflectray.dy = d.y;
		worker.reflectray.dz = d.z;
	}
	tflux = flux * materialf3(env, it.mt, MATERIAL_REFRACT, it.uv);
	if(length(tflux) > LOWESTFLUX_REFRACT) { worker.flags |= WorkerUnit_REFRACT;
		float r = 1.52; // FIXME refractive index ...
		float3 d = refracted(ray.d, it.n, r); // FIXME
		worker.refractray.fx = tflux.x;
		worker.refractray.fy = tflux.y;
		worker.refractray.fz = tflux.z;
		worker.refractray.dx = d.x;
		worker.refractray.dy = d.y;
		worker.refractray.dz = d.z;
	}
	workers[bufferindex] = worker;
}



void putflux(Env env, float3 iat, __global float* accumulator, float3 flux) {
	int3 loc = intloc(iat);
	int nl = 1;
	for(int i=-nl;i<=nl;++i)for(int j=-nl;j<=nl;++j)for(int k=-nl;k<=nl;++k) {
		int loct = spacehash(loc + (int3)(i,j,k));
		int p = env.spacemap[loct];
		while(p) {
			Hit h = env.photons[p>>1]; p = h.next;
			float3 at = (float3)(h.px, h.py, h.pz);
			if(length(iat-at)*SPACEPART<1) {
				float3 tflux = (float3)(h.fx, h.fy, h.fz) * flux;
				atomic_float_add(accumulator+h.deposit*4+0, tflux.x);
				atomic_float_add(accumulator+h.deposit*4+1, tflux.y);
				atomic_float_add(accumulator+h.deposit*4+2, tflux.z);
			}
		}
	}
}


void revpmap_impl(Env env, Ray ray, uint* seed, __global float* accumulator) {
	Intersection it = {1};
	int reclimit = REVPMAP_RECLIMIT;
	bool rectrace = true;
	float3 flux = (float3)(1,1,0.7);
	while(it.t > 0 && reclimit--) {
		it.t = -1;
		intersect(env, ray, &it);

		if(it.t > 0) {
			putflux(env, it.at, accumulator, flux);
			float p = getrandf(seed), t;
			float3 tflux = materialf3(env, it.mt, MATERIAL_REFRACT, it.uv);
			t = maxv3(tflux); p-=t;
			if(p<0) {
				flux *= tflux/t;
				float r = 1.52; // FIXME
				ray.d = refracted(ray.d, it.n, r);
				ray.o = it.at;
				continue;
			}
			tflux = materialf3(env, it.mt, MATERIAL_REFLECT, it.uv);
			t = maxv3(tflux); p-=t;
			if(p<0) {
				flux *= tflux/t;
				ray.d = reflected(ray.d, it.n);
				ray.o = it.at;
				continue;
			}
			tflux = materialf3(env, it.mt, MATERIAL_DIFFUSE, it.uv);
			t = maxv3(tflux); p-=t;
			if(p<0) {
				flux *= tflux/t;
				ray.d = diffused(ray.d, it.n, seed);
				ray.o = it.at;
				continue;
			}
			break;
		}
	}
}


__kernel void revpmap( // reverse photon map
	__global void* m,
	__global int* spacemap,
	__global Hit* photons,
	__global uint* randomseed,
	__global float* accumulator
) {
	Env env = {get_global_id(0), get_global_id(1), get_global_size(0), get_global_size(1), m, spacemap, photons};
	int bufferindex = getarrayoffset(env);
	uint seed = randomseed[bufferindex];
	int kw, kh, ki, aw, ah;
	for(ki=0;ki!=REVPMAP_ITERS;++ki)
	for(kw=0;kw!=REVPMAP_KERNEL_W;++kw)
	for(kh=0;kh!=REVPMAP_KERNEL_H;++kh)
	for(aw=0;aw!=REVPMAP_KERNEL_AW;++aw)
	for(ah=0;ah!=REVPMAP_KERNEL_AH;++ah) {
		float2 spr = (float2)(env.x, env.y);
		spr = spr + (float2)(
			(float)(kw+getrandf(&seed))/REVPMAP_KERNEL_W,
			(float)(kh+getrandf(&seed))/REVPMAP_KERNEL_H);
		spr = spr / (float2)(env.w, env.h);

		float xpos = ((aw + getrandf(&seed))/REVPMAP_KERNEL_AW - 0.5)/3;
		float ypos = 0.99;// - getrandf(&seed)/20;
		float zpos = ((ah + getrandf(&seed))/REVPMAP_KERNEL_AH - 0.5)/3;
		spr.y = spr.y < 0.95 ? spr.y/2 : spr.y;
		Ray ray = {(float3)(xpos,ypos,zpos), spherical(spr.x/*/4-0.125*/, spr.y)};
		revpmap_impl(env, ray, &seed, accumulator);
	}
	randomseed[bufferindex] = seed;
}


//// ==< utilities >=========================================================
//#include "impl/utils.cl"
//#include "impl/matrix.cl"
//#include "impl/optics.cl"
//
//// ==< bounding box >======================================================
//#include "impl/aabb.test.cl"
//
//// ==< texture mapping >===================================================
//
//// interesting fractals, for u-v texture testing perpose
//#include "impl/fractal.cl"
//#include "impl/texture.cl"
//
//// ==< intersection >======================================================
//#include "impl/ray.intersect.cl"
//#include "impl/intersection.cl"
//
//// ==< lighting >==========================================================
//#include "impl/shading.cl"
//
//// ==< rendering >=========================================================
//#include "impl/rendering.cl"
//
//// ==< kernel >============================================================
//#include "impl/render.kernel.cl"
