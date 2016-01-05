float3 reflected(float3 d, float3 n) {
	n = normalize(n);
	return d - 2*dot(d, n)*n;
}

// FIXME
float3 diffused(float3 d, float3 n, uint* seed) {
	float3 un = normalize(n);
	if(dot(un, d))un = -un;
	float3 ua = mkperp(un);
	float3 ub = cross(ua, un);
	float dira = getrandf(seed)-0.5;
	float dirb = getrandf(seed)-0.5;
	return dira*ua+dirb*ub+reflected(d,un)*0.2+un*0.2;
}

// FIXME
float3 refracted(float3 d, float3 n, float r) {
	d = normalize(d); n = normalize(n);
	float c = -dot(n,d);// cos theta
	if(c<0) { c = -c; n=-n; } else { r=1/r; /* the normal is outward*/ }
	float t = 1-r*r*(1-c*c);
	if(t < 0)
		return reflected(d,n); // hack for total reflected ray
	return r*d+(r*c-sqrt(t))*n;
}

Ray spawnRayDirection(__global EGLobalInfo* info, float x, float y){
	float16 viewToWorld = vload16(0, info->viewToWorld);
	float fov = info->fov;
	float factor = tan(fov*M_PI/360.0);
	P3 imagePlane = (float3)(factor*2*(x-0.5),-factor*2*(y-0.5), -1);
	Ray ray = {
		M4P3_mul(viewToWorld, (float3)(0, 0, 0)),
		M4V3_mul(viewToWorld, imagePlane)
	};
	return ray;
}