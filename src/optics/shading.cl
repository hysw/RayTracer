float lighting(Env env, float3 dest, float3 source) {
#if ENABLE_SHADOW == 0
	return 1;
#elif ENABLE_SHADOW == 1 // hard shadow
	return check_intersect(env, dest, source);
#else
	float3 n = source - dest;
	// TODO double check norm
	float3 v1 = normalize(cross(n, n+(float3)(0,0,1)));
	float3 v2 = normalize(cross(v1, n));
	float r = 0.01;
	float res = check_intersect(env, dest, source)
		+ check_intersect(env, dest, source + r*v1)
		+ check_intersect(env, dest, source + r*v2)
		+ check_intersect(env, dest, source - r*v1)
		+ check_intersect(env, dest, source - r*v2)
		+ check_intersect(env, dest, source + r*v1 + r*v2)
		+ check_intersect(env, dest, source + r*v1 - r*v2)
		+ check_intersect(env, dest, source - r*v1 + r*v2)
		+ check_intersect(env, dest, source - r*v1 - r*v2);
	#if ENABLE_SHADOW == 2
		return res / 9;
	#else
		if(res < 0.5 || res > 8.5)
			return res / 9;
		int i,j;
		res = 0;

		int k = SOFTSHADOW_DIRECTIONAL_SAMPLES;
		for(i=-k;i<=k;++i) { for(j=-k;j<=k;++j) {
			res += check_intersect(env, dest, source + r*i/k*v1+r*j/k*v2);
		}}
		return res/(2*k+1)/(2*k+1);
	#endif
#endif
}

#if ENABLE_DIFFUSE
float3 shade_diffuse(Env env, Ray ray, Intersection it, P3 source) {
	float3 dif = env.v[env.s[it.mt+MATERIAL_DIFFUSE]].xyz;

	V3 n = it.n; n = normalize(n);
	V3 d = ray.d; d = normalize(d);

	V3 s = source - it.at;
	s = normalize(s);
	V3 m = s - 2*dot(s, n)*n;
	m = normalize(m);

	float dif_frac = max(0.0, dot(s, n));
	return dif*dif_frac;
}
#endif

#if ENABLE_SPECULAR
float3 shade_specular(Env env, Ray ray, Intersection it, P3 source) {
	float4 spec_ = env.v[env.s[it.mt+MATERIAL_SPECULAR]];
	float3 spec = spec_.xyz; float spec_exp = spec_.w;

	V3 n = it.n; n = normalize(n);
	V3 d = ray.d; d = normalize(d);


	V3 s = source - it.at;
	s = normalize(s);
	V3 m = s - 2*dot(s, n)*n;
	m = normalize(m);

	float spec_frac = pow(max(0.0, dot(m, d)), spec_exp);
	return spec*spec_frac;
}
#endif

float3 shade(Env env, Ray ray, Intersection it) {
	if(!it.mt) return -1; // no material, wtf?

	if(dot(it.n,ray.d)>0) { it.n = -it.n; /* return 0; */ } // hack

	float3 color = uvmap_TODO(it.uv);
	float3 acc = 0;
	P3 p = it.at;
	P3 source = (float3)(-0.5,0.7,0.5);

#if ENABLE_DIFFUSE || ENABLE_SPECULAR
// #if ENABLE_SHADOW
#if ENABLE_PHOTONMAP
	float3 sourcelight = getflux(env, it.at);
#else
	//P3 lightdir = normalize((float3)(1,-1,-1));
	//float consentrate = 1; //sqrt(clamp((dot(normalize(p-source), lightdir)-0.1)/0.9,0.0,1.0));
	float ivdissq = rsqrt(dot(source - p, source - p));
	float visible = lighting(env, p, source);
	float3 sourcelight = ivdissq*visible;
#endif
#endif

#if ENABLE_DIFFUSE
	acc += sourcelight * shade_diffuse(env, ray, it, source);
#endif

#if ENABLE_SPECULAR
	acc += sourcelight * shade_specular(env, ray, it, source);
#endif

#if ENABLE_AMBIENT
	if(length(acc)<length(env.v[env.s[it.mt+MATERIAL_AMBIENT]].xyz))
		acc = env.v[env.s[it.mt+MATERIAL_AMBIENT]].xyz;
#endif

#if ENABLE_SIGNITURE
	acc += acc + env.v[env.s[it.mt+MATERIAL_SIGNITURE]].xyz;
#endif

	return clamp(color*acc, 0.0, INFINITY);
}