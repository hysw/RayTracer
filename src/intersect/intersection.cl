bool hit_triangle(Env env, int id, Ray ray, Intersection* inter) {
	// http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
	// assume we are already in the world
	float3 v0 = env.v[env.s[id+1]].xyz;
	float3 v1 = env.v[env.s[id+2]].xyz;
	float3 v2 = env.v[env.s[id+3]].xyz;

	float3 n = cross(v1 - v0, v2 - v0);
	float3 res = intersect_ray_triangle(ray, v0, v1, v2);

	float t = res.x;

	if(t>0 && (inter->t < 0 || t < inter->t)) {
		inter->t  = t;
		inter->id = id;
		inter->n  = n;
		 // maybe uv will have better compute?
		inter->at = ray.o+t*ray.d;
		inter->uv = res.yz;
		return true;
	}
	return false;
}

bool hit_sphere(Env env, int id, Ray ray, Intersection* inter) {
	P3 o = ray.o;
	V3 d = ray.d;
	float4 circ = env.v[env.s[id+1]];
	float3 res = intersect_ray_sphere(ray, circ.xyz, circ.w);

	float t = res.x;
	if(t>0 && (inter->t < 0 || t < inter->t)) {
		inter->t  = t;
		inter->id = id;
		inter->n  = o+d*t - circ.xyz;
		// it seems to be much better to calculate location use normal and radius
		inter->at = normalize(inter->n)*circ.w;
		inter->uv = res.yz;
		return true;
	}
	return false;
}


void hit(Env env, Ray ray_, Intersection* it) {
	// identity matrix
	float16 modelToWorld = (float16)(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
	float16 worldToModel = (float16)(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
	// Material
	int material;
	// BVH
	Iterator stack[16]; int sp = 0;
	// list iterator
	int o = env.s[0], i = 0;
	// ray
	Ray ray = ray_;

	if(env.s[o+4]) {
		worldToModel = mmul(env.m[env.s[o+4]+1], worldToModel);
		modelToWorld = mmul(modelToWorld, env.m[env.s[o+4]]);
		ray.d = M4V3_mul(worldToModel, ray_.d);
		ray.o = M4P3_mul(worldToModel, ray_.o);
	}
	if(env.s[o+BVH_MATERIAL]) // we don't save the material anyway
		material = env.s[o+BVH_MATERIAL];

	// TODO root transformation
	while(o || sp) {
		// we finished with current object? pop;
		if(!o) { --sp; o = stack[sp].o; i = stack[sp].i; continue; }
		__global int* p = &env.s[o];
		// the only container type currently is BVH
		// we are at the end of current componenets
		if(i >= p[BVH_SIZE]) {
			// multiply inverse matrix
			if(p[4]) {
				worldToModel = mmul(env.m[p[4]], worldToModel);
				modelToWorld = mmul(modelToWorld, env.m[p[4]+1]);
				ray.d = M4V3_mul(worldToModel, ray_.d);
				ray.o = M4P3_mul(worldToModel, ray_.o);
			}
			i = p[2] + 1;
			o = p[1];
			continue;
		}

		int q = p[BVH_END+i];

		switch(env.s[q]) {
		case TYPE_BVH:
			if(env.s[q+BVH_MATERIAL]) // we don't save the material anyway
				material = env.s[q+BVH_MATERIAL];
			if(env.s[q+4]) {
				worldToModel = mmul(env.m[env.s[q+4]+1], worldToModel);
				modelToWorld = mmul(modelToWorld, env.m[env.s[q+4]]);
				ray.d = M4V3_mul(worldToModel, ray_.d);
				ray.o = M4P3_mul(worldToModel, ray_.o);
			}
			if(!test_AABB(env, ray, env.s[q+3])) { ++i; break; } // bounding box test
			if(!env.s[q+1]) { stack[sp].o = o; stack[sp].i = i + 1; ++sp; }
			o = q; i = 0;
			break;
		case TYPE_TRIANGLE:
			if(hit_triangle(env, q, ray, it)) {
				it->n = M4V3_mul(M4_tr(worldToModel), it->n);
				it->at = M4P3_mul(modelToWorld, it->at);
				it->mt = material;
			}
			++i;
			break;
		case TYPE_SPHERE:
			if(hit_sphere(env, q, ray, it)) {
				it->n = M4V3_mul(M4_tr(worldToModel), it->n);
				it->at = M4P3_mul(modelToWorld, it->at);
				it->mt = material;
			}
			++i;
			break;
		default:
			return; // TODO fault
		}
	}
}

float check_intersect(Env env, float3 dest, float3 source) {
	Intersection it = { -1 };
	Ray ray = { dest, source - dest };
	hit(env, ray, &it);
	if(it.t < 0 || it.t > 1.000001)
		return 1;
	else
		return 0;
}