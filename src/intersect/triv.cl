typedef Env Model;

__global void* offptr(__global void* base, int offset) {
	return offset!=0?(((__global char*)base)+offset):0;
}
__global void* mptr(Model m, int offset) {
	return offptr(m.m, offset);
}

int mtag(Model m, int offset) {
	return ((__global EStruct*)mptr(m, offset))->tag;
}

float16 loadf16(__global void* base, int offset) {
	return vload16(0, (__global float*)offptr(base, offset));
}

float3 loadf3(__global void* base, int offset) {
	return vload3(0, (__global float*)offptr(base, offset));
}

float2 loadf2(__global void* base, int offset) {
	return vload2(0, (__global float*)offptr(base, offset));
}

float2 remapuv(float2 uv, float2 t0, float2 t1, float2 t2) {
	return t0*(1-uv.x-uv.y)+t1*uv.x+t2*uv.y;
}

bool intersect_triangle(Model m, int id, Ray ray, Intersection* inter) {
	// http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
	// assume we are already in the world
	EPlanar pln = *(__global EPlanar*)mptr(m, id);

	float3 v0 = loadf3(m.m, pln.v[0]);
	float3 v1 = loadf3(m.m, pln.v[1]);
	float3 v2 = loadf3(m.m, pln.v[2]);

	float2 t0 = loadf2(m.m, pln.t[0]);
	float2 t1 = loadf2(m.m, pln.t[1]);
	float2 t2 = loadf2(m.m, pln.t[2]);

	// TODO normal interpolation

	float3 n = cross(v1 - v0, v2 - v0);
	float3 res = intersect_ray_triangle(ray, v0, v1, v2);

	float t = res.x;

	if(t>0 && (inter->t < 0 || t < inter->t)) {
		inter->t  = t;
		inter->id = id;
		inter->n  = n;
		 // maybe uv will have better compute?
		inter->at = ray.o+t*ray.d;
		inter->uv = remapuv(res.yz, t0, t1, t2);
		return true;
	}
	return false;
}

bool intersect_planar(Model m, int id, Ray ray, Intersection* inter) {
	EPlanar pln = *(__global EPlanar*)mptr(m, id);

	float3 v0 = loadf3(m.m, pln.v[0]);
	float3 v1 = loadf3(m.m, pln.v[1]);
	float3 v2 = loadf3(m.m, pln.v[2]);

	float2 t0 = loadf2(m.m, pln.t[0]);
	float2 t1 = loadf2(m.m, pln.t[1]);
	float2 t2 = loadf2(m.m, pln.t[2]);

	float3 n = cross(v1 - v0, v2 - v0);
	float3 res = intersect_ray_planar(ray, v0, v1, v2);

	float t = res.x;

	if(t>0 && (inter->t < 0 || t < inter->t)) {
		switch(pln.s.tag) {
		case ENTITY_Square:
			if(!intersection_planer_unit_square(res.yz)) return false;
			break;
		default: return false;
		}
		inter->t  = t;
		inter->id = id;
		inter->n  = n;
		 // maybe uv will have better compute?
		inter->at = ray.o+t*ray.d;
		inter->uv = remapuv(res.yz, t0, t1, t2);
		return true;
	}
	return false;
}

// return -1 if is not primitive, 0 if not hit, 1 if hit
int intersect_primitive(Model m, Ray ray, int o, Intersection* it) {
	switch(mtag(m, o)) {
	case ENTITY_Triangle:
		return intersect_triangle(m, o, ray, it);
	case ENTITY_Square:
		return intersect_planar(m, o, ray, it);
	default:
		//ENTITY_NULL,
		//ENTITY_Global,
		//ENTITY_Sphere,
		//ENTITY_Cube,
		//ENTITY_HalfPlane,
		//ENTITY_Triangle,
		//ENTITY_Circle,
		//ENTITY_Square,
		//ENTITY_Polygon,
		//ENTITY_Material,
		//ENTITY_Texture,
		//ENTITY_List,
		//ENTITY_TrMatrix,
		//ENTITY_AABBTest,
		//ENTITY_SMaterial,
		//ENTITY_Compounded,
		return -1;
	}
	return 0;
}

int intersect(Env env, Ray ray_, Intersection* it) {
	Ray ray = ray_;
	float16 worldToModel=M4_ident();
	float16 modelToWorld=M4_ident();
	Iterator stack[32]; int sp = 0;
	__global EGlobal* g = mptr(env, 4);
	int o = g->root, i = 0, tag = mtag(env, g->root);
	int material = 0;
	while((o || sp) && sp<30) switch(tag) {
	case ENTITY_NULL:{
		if(o) { tag = mtag(env, o); break; }
		--sp; o = stack[sp].o; i = stack[sp].i;
	}break;
	case ENTITY_List:{
		int len = ((__global EList*)mptr(env, o))->c;
		if(i < len) {
			int t = *(__global int*)mptr(env, o+sizeof(EList)+sizeof(int)*i); ++i;
			int res = intersect_primitive(env, ray, t, it);
			if(res > 0) {
				it->n = M4V3_mul(M4_tr(worldToModel), it->n);
				it->at = M4P3_mul(modelToWorld, it->at);
				it->mt = material;
			} else if(res < 0) {
				stack[sp].o = o; o = t;
				stack[sp].i = i; i = 0;
				tag = ENTITY_NULL; ++sp;
			}
		} else { o = 0; i = 0; tag = ENTITY_NULL; }
	}break;
	case ENTITY_TrMatrix:{
		ETrMatrix mtx = *(__global ETrMatrix*)mptr(env, o);
		if(i < 1) {++i;
			worldToModel = mmul(loadf16(env.m, mtx.m+sizeof(float16)), worldToModel);
			modelToWorld = mmul(modelToWorld, loadf16(env.m, mtx.m));
			ray.d = M4V3_mul(worldToModel, ray_.d);
			ray.o = M4P3_mul(worldToModel, ray_.o);
			int res = intersect_primitive(env, ray, mtx.c, it);
			if(res > 0) {
				it->n = M4V3_mul(M4_tr(worldToModel), it->n);
				it->at = M4P3_mul(modelToWorld, it->at);
				it->mt = material;
			} else if(res < 0) {
				stack[sp].o = o; o = mtx.c;
				stack[sp].i = i; i = 0;
				tag = ENTITY_NULL; ++sp;
			}
		} else {
			worldToModel = mmul(loadf16(env.m, mtx.m), worldToModel);
			modelToWorld = mmul(modelToWorld, loadf16(env.m, mtx.m+sizeof(float16)));
			ray.d = M4V3_mul(worldToModel, ray_.d);
			ray.o = M4P3_mul(worldToModel, ray_.o);
			o = 0; i = 0; tag = ENTITY_NULL;
		}
	}break;
	case ENTITY_Material:{
		material = o; o = 0; i = 0; tag = ENTITY_NULL;
	}break;
	default:
		//ENTITY_NULL,
		//ENTITY_Global,
		//ENTITY_Sphere,
		//ENTITY_Cube,
		//ENTITY_HalfPlane,
		//ENTITY_Triangle,
		//ENTITY_Circle,
		//ENTITY_Square,
		//ENTITY_Polygon,
		//ENTITY_Material,
		//ENTITY_Texture,
		//ENTITY_List,
		//ENTITY_TrMatrix,
		//ENTITY_AABBTest,
		//ENTITY_SMaterial,
		//ENTITY_Compounded,
		return false;
	}
	//if(intersect_triangle(m, m.root, ray, &it))
	//	return txtu(it.uv);
	return it->t > 0;
}
