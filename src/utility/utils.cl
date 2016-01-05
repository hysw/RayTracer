

// https://en.wikipedia.org/wiki/Lehmer_random_number_generator
uint getrand(uint* x) {
	//if(*x == 0) *x = 1;
	//*x = ((ulong)*x * 48271UL) % 2147483647UL;
	*x = ((ulong)*x * 279470273UL) % 4294967291UL;
	return *x;
}

float getrandf(uint* x) {
	return getrand(x)%10000000/10000000.0;
}

float maxv3(float3 v) {
	return max(v.x, max(v.y, v.z));
}

int3 intloc(float3 loc) {
	loc = round(loc * SPACEPART);
	return (int3)((int)loc.x, (int)loc.y, (int)loc.z);
}

float3 i2f3(int3 v) {
	int x=v.x, y=v.y, z=v.z;
	return (float3)(*(float*)&x, *(float*)&y, *(float*)&z);
};
int3 f2i3(float3 v) {
	float x=v.x, y=v.y, z=v.z;
	return (int3)(*(int*)&x, *(int*)&y, *(int*)&z);
};

int spacehash(int3 v) {
	int g = (v.x/11 + v.z/7 + v.y/5) * (v.x/19 + v.z/17 + v.y/13);
	int h = (v.x * 73856093) ^ (v.y * 19349663) ^ (v.z * 83492791) ^ g;
	//int h = (v.x * 756093) ^ (v.y * 349663) ^ (v.z * 832791);
	return ((unsigned int)h) % (SPACEHASH_SIZE);
}


float3 getflux(Env env, float3 at) {
	return 0;

	//int3 loc = intloc(at);
	//int nl = 1;
	//float3 flux = 0;
	//for(int i=-nl;i<=nl;++i)for(int j=-nl;j<=nl;++j)for(int k=-nl;k<=nl;++k) {
	//	int loct = spacehash(loc + (int3)(i,j,k));
	//	flux += triv(at, env.spacemap[loct], env.photons);
	//}
	//return flux * 8.0 * SPACEPART*SPACEPART/FWIDTH/FHEIGHT;
}

bool cas_float(volatile __global float* p_, float old, float val) {
	volatile __global int* p = (volatile __global int*) p_;
	int cmp = *(int*)&old;
	int rep = *(int*)&val;
	return atomic_cmpxchg(p, cmp, rep) == cmp;
}

void atomic_float_add(volatile __global float* p, float k) {
	float v = 0, o = 0;
	do {o = *p; v = o + k;} while(!cas_float(p, o, v));
}

float16 f16_from_garray(__global float* p) {
	return (float16)(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	                 p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}

float3 modrange(int3 u, int m) {
	uint3 v = abs(u%m);
	float t = m;
	return (float3)(v.x/t, v.y/t, v.z/t);
}

float3 mixk(float k, float3 a, float3 b) {return b*k+a*(1-k);}
float3 gencolor_impl(float x) {
	x = x - (int) x;
	x = x * 6;
	if( x < 1 ) {
		return mixk(x-0, (float3)(1,0,0), (float3)(1,1,0));
	} else if(x < 2) {
		return mixk(x-1, (float3)(1,1,0), (float3)(0,1,0));
	} else if(x < 3) {
		return mixk(x-2, (float3)(0,1,0), (float3)(0,1,1));
	} else if(x < 4) {
		return mixk(x-3, (float3)(0,1,1), (float3)(0,0,1));
	} else if(x < 5) {
		return mixk(x-4, (float3)(0,0,1), (float3)(1,0,1));
	} else {
		return mixk(x-5, (float3)(1,0,1), (float3)(1,0,0));
	}
}
float3 gencolor(float x) {
	return gencolor_impl(x*0.93);
}


float3 spherical(float u, float v) {
	float y = 2*v-1;
	float r = sqrt(1-y*y);
	return (float3)(cos(2*M_PI*u)*r, y, sin(2*M_PI*u)*r);
}

float3 mkperp(float3 n) {
	if(n.x>n.y) {
		return normalize(cross(n, n+(float3)(0,1,0)));
	} else {
		return normalize(cross(n, n+(float3)(1,0,0)));
	}
}
/*
int maplocation(float3 v) {
	v = (v+1.1)/2.2;
	if(v.x<0 || v.y<0 || v.z<0 || v.x>=1 || v.y>=1 || v.z>=1)
		return -1;
	v=v*0.99999*DIMSIZE;
	return
		((int)(v.x))
		+((int)(v.y))*DIMSIZE
		+((int)(v.z));
}*/

/*
bool cas_float(volatile __global int* p_, float old, float val) {
	int o = *(int*)&old;
	int v = *(int*)&val;
	volatile __global int* p = (volatile __global int*) p_;
	return atomic_cmpxchg(p, o, v) == o;
}*/

/*
struct Photon {
	int link;  // next photon in the space mapping
	int flags; //
	float x, y, z; // location
	float r, g, b;
};*/

# if 0

float3 triv(float3 loc, int p, __global int8* links) {
	float3 flux = 0;
	while(p) {
		int8 info = links[p>>1];
		float3 at = i2f3(info.yzw);
		if(length(loc-at)*SPACEPART<1)
			flux += i2f3(info.s456);
		p = info.x;
		//acc += in[spacehash(intloc(intersect.at))];
	}
	return flux;
}
# endif
