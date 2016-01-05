float3 rendering_trace(Env env) {
	//float16 viewToWorld = (float16)(0.83205, -0.14825, 0.534522, 4, 1.38778e-17, 0.963624, 0.267261, 2, -0.5547, -0.222375, 0.801784, 1+5, 0, 0, 0, 1);
	//float16 viewToWorld = (float16)(1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1);
	float16 viewToWorld = (float16)(1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1);
	float fov = 120;
	float factor = (env.h/2.0)/tan(fov*M_PI/360.0);
	P3 imagePlane = (float3)(
		(-env.w/2.0 + 0.5 + env.x)/factor,
		-(-env.h/2.0 + 0.5 + env.y)/factor, -1);
	Ray ray = {
		M4P3_mul(viewToWorld, (float3)(0, 0, 0)),
		M4V3_mul(viewToWorld, imagePlane)
	};
	float3 color = 0;
	float3 frac = 1.0;
	float3 tfrac = 1.0;

	Ray rays[RECURSIVE_TRACE_LIMIT];
	float3 fracs[RECURSIVE_TRACE_LIMIT];
	int actions[RECURSIVE_TRACE_LIMIT];
	Intersection intersects[RECURSIVE_TRACE_LIMIT];
	int sp = 0; // stack pointer
	int action = 0; // instruction pointer

/*
trace(ray) {
0	hit(ray, intersection)
	if(notlimit) {
1		trace(refracted(ray))
2		trace(reflected(ray))
	}
}
*/
	Intersection intersect = {-1};
	while( sp || action != -1 ) {
		switch(action) {
		case 0: ++action;
			intersect.t = -1; hit(env, ray, &intersect);
			if(intersect.t <= 0) { action = -1; break; }
			color = color + frac * shade(env, ray, intersect);
			if(sp >= RECURSIVE_TRACE_LIMIT) { action = -1; break; }
		case 1: ++action;
#if ENABLE_REFLECT!=0
			if(sp >= RECURSIVE_TRACE_LIMIT) { action = -1; break; }
			tfrac = frac * env.v[env.s[intersect.mt+MATERIAL_REFLECT]].xyz;
			if(length(tfrac)>0.1){
				rays[sp] = ray;
				fracs[sp] = frac;
				actions[sp] = action;
				intersects[sp] = intersect;
				ray.d = reflected(ray.d, intersect.n);
				ray.o = intersect.at;
				frac = tfrac;
				action = 0; ++sp;
				break;
			}
#endif
		case 2: ++action;
#if ENABLE_REFRACT!=0
			tfrac = frac * env.v[env.s[intersect.mt+MATERIAL_REFRACT]].xyz;
			if(length(tfrac)>0.1){
				rays[sp] = ray;
				fracs[sp] = frac;
				actions[sp] = action;
				intersects[sp] = intersect;
				float rfidx = env.v[env.s[intersect.mt+MATERIAL_REFRACT]].w;
				ray.d = refracted(ray.d, intersect.n, rfidx);
				if(length(ray.d)<0.00001) { action = -1; break; }
				ray.o = intersect.at;
				frac = tfrac;
				action = 0; ++sp;
				break;
			}
#endif
		case 3: action = -1;
		case -1:
			if(sp>0) {
				--sp;
				ray = rays[sp];
				frac = fracs[sp];
				action = actions[sp];
				intersect = intersects[sp];
			}
			break;
		}
	};
	return color;
/* return (float4)((2-julia(pos.x, pos.y))/2 * shade(n, p+t*d, d,
			(float3)(0, 0, 0) + (float3)(1,0,1)*pos.y/5,
			(float3)(0.54, 0.89, 0.63)*(1-pos.z) + (float3)(0.89, 0.63, 0.89)*pos.z,
			(float3)(0.316228, 0.316228, 0.316228), 12.8), t);
	*/
}