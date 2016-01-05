#define __CL_ENABLE_EXCEPTIONS

// btw, I don't program on apple...
#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <iterator>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <array>
#include <cmath>
#include "inc/enums.h"
#include "inc/constants.h"
#include "inc/entity.h"
#include "inc/record.h"

using namespace hysw::raytracer;
double gettime_r() {
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return t.tv_sec + t.tv_nsec*0.000000001;
}

// ========================================================================
// ==< Classes >===========================================================

// --< Triangle mesh >-----------------------------------------------------
struct Point3D{float x,y,z;}; typedef Point3D Vector3D;
struct Triangle{int a,b,c;};
struct MeshObject {
	std::vector<Point3D> vertices;
	std::vector<Triangle> triangles;
	// scale the model, so we don't need a matrix
	static MeshObject FromFile(const char* name, double scale=1);
};

Vector3D operator*(float by, Vector3D a) {
	return {a.x*by, a.y*by, a.z*by};
}
Vector3D operator-(Vector3D a, Vector3D b) {
	return {a.x-b.x, a.y-b.y, a.z-b.z};
}
double dot(Vector3D a, Vector3D b) {
	return a.x*b.x + a.y*b.y + a.z*b.z;
}
Vector3D normalize(Vector3D a) {
	return sqrt(dot(a, a))*a;
}
Vector3D cross(Vector3D a, Vector3D b) {
	return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}

// --< Matrix >------------------------------------------------------------
struct Matrix4x4 {
	Matrix4x4():v{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}{}
	Matrix4x4(float v0, float v1, float v2, float v3,
				float v4, float v5, float v6, float v7,
				float v8, float v9, float va, float vb,
				float vc, float vd, float ve, float vf):
				v{{v0, v1, v2, v3, v4, v5, v6, v7,
				v8, v9, va, vb, vc, vd, ve, vf}}{}
	float& operator()(int r, int c) { return v[r*4+c]; }
	float operator()(int r, int c) const { return v[r*4+c]; }
	std::array<float, 16> v;
};

Matrix4x4 operator*(const Matrix4x4& a, const Matrix4x4& b);

struct TrMatrix {
	Matrix4x4 mtx,inv;
	void rotate(char axis, double angle);
	void translate(double x, double y, double z);
	void scale(double x, double y, double z);
};


// --< Scene >-------------------------------------------------------------

typedef int BVH_id_t;

class Scene {
public:
	Scene();
	std::vector<float> vertices;
	std::vector<int>   structs;
	std::vector<float> matrices;
	int matrix(const Matrix4x4& m);
	int matrix(const TrMatrix& m);
	int vertex(float x, float y, float z, float w = 0);
	int vertex(Point3D p) {return vertex(p.x,p.y,p.z);}
	int& operator[](int i){return structs[i];}
	int append_struct(int type, int size);
	int vertices_count() {return vertices.size()/4;}
	void setroot(int structid) {structs[0] = structid;}
	BVH_id_t append_BVH_Mesh(const MeshObject& mesh, int cutoff = 5);
	BVH_id_t append_BVH_Photon(const MeshObject& mesh, int cutoff = 5);
	BVH_id_t append_BOX(int x);
	BVH_id_t append_LIST(std::vector<int> l);
};

// --< OpenCL >------------------------------------------------------------
class OpenCLEnv {
public:
	std::vector<cl::Platform> platforms;
	std::vector<cl::Device> devices;
	cl::Context context;
	OpenCLEnv() {
		cl::Platform::get(&platforms);
		if (platforms.size() == 0) { throw "Platform size 0"; }
		cl_context_properties properties[] = {
			CL_CONTEXT_PLATFORM, (cl_context_properties)(platforms[0])(), 0};
		context = cl::Context(CL_DEVICE_TYPE_ALL, properties);
		devices = context.getInfo<CL_CONTEXT_DEVICES>();
	}
	void dump(std::ostream& ost) {
		float localmem = devices[0].getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
		float constmem = devices[0].getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>();
		float globlmem = devices[0].getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
		ost << "local size: "  << localmem/1024    <<"KiB\n"
		    << "const size: "  << constmem/1024    <<"KiB\n"
		    << "global size: " << globlmem/1048576 <<"MiB\n";
	}
};

std::string LoadKernel (const char* name);
const char *opencl_getErrorString(cl_int error);

// --< misc >--------------------------------------------------------------
int construct_model(Scene& scene);


// ========================================================================
// ==< Main >==============================================================

#define OUTPUT_TIMEUSED(s) do{\
	timestamp_temp=gettime_r();\
	std::cerr<<s<<timestamp_temp-timestamp_last<<"s\n";\
	timestamp_last = timestamp_temp;\
	}while(0)
struct Float4 {float x,y,z,w;};
unsigned char f2color(float x) {
	//return x<0? 0 : x>1 ? 255 : x*255;
	int dyn = 32;
	if(x>1) {
		float t = log(x)/2;
		return t>1?255:(255-dyn)+dyn*t;
	} else {
		return x*(255-dyn);
	}
}

void writeImage(const char* prefix, int height, int width, int pass, Float4* acc) {
	std::stringstream sst; sst<<prefix<<width<<"x"<<height<<"p"<<pass<<".ppm";
	std::string filename = sst.str();
	std::ofstream fst(filename.c_str());

	unsigned char* ouput_data = new unsigned char[height*width*3];
	double totalflux = 0;
	for(int i=0;i!=height*width;++i) {
		totalflux+=acc[i].x;
		totalflux+=acc[i].y;
		totalflux+=acc[i].z;
	};
	double aveflux = (totalflux + 0.0001) / (height*width*3);
	double capflux = aveflux * 3;
	std::cerr<<"averate flux: "<<aveflux<<"\n";
	for(int i=0;i!=height*width;++i) {
		ouput_data[3*i+0] = f2color(acc[i].x/capflux);
		ouput_data[3*i+1] = f2color(acc[i].y/capflux);
		ouput_data[3*i+2] = f2color(acc[i].z/capflux);
	}

	fst<<"P6 "<<width<<" "<<height<<" "<<255<<"\n";
	fst.write((char*)ouput_data, height*width*3);
	delete[] ouput_data;
}

void constructview(void) {
/*
	Info info = {120, {1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1}};
	Info info2 = {120, {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}};
	{
		Vector3D w;
		Vector3D eye = {0,0.6,0};
		Vector3D view = Vector3D({0,0,0})-eye;
		Vector3D up = {0,0,1};
		up = normalize(up - dot(up, view)*view);
		w = cross(view, up);

		float* m = info2.viewToWorld;
		m[0*4+0] = w.x;
		m[1*4+0] = w.y;
		m[2*4+0] = w.z;
		m[0*4+1] = up.x;
		m[1*4+1] = up.y;
		m[2*4+1] = up.z;
		m[0*4+2] = -view.x;
		m[1*4+2] = -view.y;
		m[2*4+2] = -view.z;
		m[0*4+3] = eye.x;
		m[1*4+3] = eye.y;
		m[2*4+3] = eye.z;
	}
 */
}

EStruct* boxmodel() {
	EList* l = new EList;
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({-1,-1,-1});
		e->v[1] = new V3({-1, 1,-1});
		e->v[2] = new V3({ 1,-1,-1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({ 1, 1, 1});
		e->v[1] = new V3({-1, 1, 1});
		e->v[2] = new V3({ 1,-1, 1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({-1,-1,-1});
		e->v[1] = new V3({ 1,-1,-1});
		e->v[2] = new V3({-1,-1, 1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({ 1, 1, 1});
		e->v[1] = new V3({ 1, 1,-1});
		e->v[2] = new V3({-1, 1, 1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({-1,-1,-1});
		e->v[1] = new V3({-1,-1, 1});
		e->v[2] = new V3({-1, 1,-1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({ 1, 1, 1});
		e->v[1] = new V3({ 1,-1, 1});
		e->v[2] = new V3({ 1, 1,-1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	return l;
}

ETexture* ETexture_fromfile(const char* name) {
	std::ifstream f(name);
	std::string s; int w; int h; int d; f>>s>>w>>h>>d;getline(f, s);
	EBuffer* txtbuf = new EBuffer;
	ETexture* txt = new ETexture; txt->w = w; txt->h = h; txt->t = 2;
	txtbuf->v.insert(txtbuf->v.end(),
		std::istreambuf_iterator<char>(f),
		std::istreambuf_iterator<char>());
	// check if input is okay...
	if(w*h == 0 || txtbuf->v.size()!=w*h*3)
		throw "??";
	txt->v = txtbuf;
	return txt;
}

EStruct* genmodel() {
	ETexture* txt = ETexture_fromfile("model/texture.ppm");
	/*
	ETexture* txt = new ETexture; txt->w = txt->h = 4; txt->t = 1;
	{
		EBuffer* txtbuf = new EBuffer;
		char a[] = {
			-1,127,127, -1,-1,127, 127,-1,127, 127,127,-1,
			127,127,-1, -1,127,127, -1,-1,127, 127,-1,127,
			127,-1,127, 127,127,-1, -1,127,127, -1,-1,127,
			-1,-1,127, 127,-1,127, 127,127,-1, -1,127,127
		};
		txtbuf->v.insert(txtbuf->v.end(), a, a+sizeof(a));
		txt->v = txtbuf;
	}*/
	ETexture* grad = new ETexture; grad->w = 6 ; grad->h = 10; grad->t = 1;
	{
		EBuffer* txtbuf = new EBuffer;
		char a[] = {
			 0, 0, 0, -1,-1,-1,   0, 0, 0, -1,-1,-1,   0, 0, 0, -1,-1,-1,
			-1,-1,-1,  0, 0, 0,  -1,-1,-1,  0, 0, 0,  -1,-1,-1,  0, 0, 0,
			 0, 0, 0, -1,-1,-1,   0, 0, 0, -1,-1,-1,   0, 0, 0, -1,-1,-1,
			-1,-1,-1,  0, 0, 0,  -1,-1,-1,  0, 0, 0,  -1,-1,-1,  0, 0, 0,
			 0, 0, 0, -1,-1,-1,   0, 0, 0, -1,-1,-1,   0, 0, 0, -1,-1,-1,
			-1,-1,-1,  0, 0, 0,  -1,-1,-1,  0, 0, 0,  -1,-1,-1,  0, 0, 0,
			 0, 0, 0, -1,-1,-1,   0, 0, 0, -1,-1,-1,   0, 0, 0, -1,-1,-1,
			-1,-1,-1,  0, 0, 0,  -1,-1,-1,  0, 0, 0,  -1,-1,-1,  0, 0, 0,
			 0, 0, 0, -1,-1,-1,   0, 0, 0, -1,-1,-1,   0, 0, 0, -1,-1,-1,
			-1,-1,-1,  0, 0, 0,  -1,-1,-1,  0, 0, 0,  -1,-1,-1,  0, 0, 0
		};
		txtbuf->v.insert(txtbuf->v.end(), a, a+sizeof(a));
		grad->v = txtbuf;
	}

	EMaterial* mt_weird = new EMaterial;
		mt_weird->v[MATERIAL_DIFFUSE] = new V3({0.3,0.3,0.3});
		mt_weird->t[MATERIAL_DIFFUSE] = txt;
		mt_weird->v[MATERIAL_REFLECT] = new V3({0.2,0.2,0.2});
		mt_weird->t[MATERIAL_REFLECT] = txt;
		mt_weird->v[MATERIAL_REFRACT] = new V3({0.4,0.4,0.4});
		mt_weird->t[MATERIAL_REFRACT] = txt;
	EMaterial* mt_white = new EMaterial;
		mt_white->v[MATERIAL_DIFFUSE] = new V3({1,1,1});
	EMaterial* mt_mirror = new EMaterial;
		mt_mirror->t[MATERIAL_REFLECT] = grad;
		mt_mirror->v[MATERIAL_REFLECT] = new V3({0.9,0.9,0.9});
	EMaterial* mt_red = new EMaterial;
		mt_red->v[MATERIAL_DIFFUSE] = new V3({1,0.5,0.5});
	EMaterial* mt_green = new EMaterial;
		mt_green->v[MATERIAL_DIFFUSE] = new V3({0.5,1,0.5});
	EMaterial* mt_blue = new EMaterial;
		mt_blue->v[MATERIAL_DIFFUSE] = new V3({0.5,0.5,1});

	EList* l = new EList;

	l->v.push_back(mt_mirror);
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({-1,-1,-1});
		e->v[1] = new V3({-1, 1,-1});
		e->v[2] = new V3({ 1,-1,-1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({ 1, 1, 1});
		e->v[1] = new V3({-1, 1, 1});
		e->v[2] = new V3({ 1,-1, 1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	l->v.push_back(mt_white);
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({-1,-1,-1});
		e->v[1] = new V3({ 1,-1,-1});
		e->v[2] = new V3({-1,-1, 1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({ 1, 1, 1});
		e->v[1] = new V3({ 1, 1,-1});
		e->v[2] = new V3({-1, 1, 1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	l->v.push_back(mt_red);
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({-1,-1,-1});
		e->v[1] = new V3({-1,-1, 1});
		e->v[2] = new V3({-1, 1,-1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	l->v.push_back(mt_blue);
	{
		EPlanar* e = new EPlanar(ENTITY_Square);
		e->v[0] = new V3({ 1, 1, 1});
		e->v[1] = new V3({ 1,-1, 1});
		e->v[2] = new V3({ 1, 1,-1});
		e->t[0] = new V2({0, 0});
		e->t[1] = new V2({1, 0});
		e->t[2] = new V2({0, 1});
		l->v.push_back(e);
	}
	l->v.push_back(mt_weird);
	{
		TrMatrix sc;
			sc.translate(0.5,-0.6,-0.3);
			sc.rotate('y', 60);
			sc.scale(0.2, 0.3, 0.4);
		TrMtx* m = new TrMtx{{sc.mtx.v, sc.inv.v}};
		ETrMatrix* e = new ETrMatrix(m, boxmodel());
		l->v.push_back(e);
	}
	return l;
}
std::vector<char> getmodel() {
	std::vector<char> buf; buf.resize(4 + sizeof(S::EGlobal));
	int root = SerializationMap::serialize(buf, genmodel());
	S::EGlobal ginfo = {{ENTITY_Global}, root, 120,
		{1,0,0,0, 0,1,0,0, 0,0,1,1, 0,0,0,1}};
	*reinterpret_cast<S::EGlobal*>(&buf[4]) = ginfo;
	return buf;
}

int main(int argc, char**argv) try {
	// --------------------------------------------------------------------
	// parse args

	srand(time(0));
	double timestamp_start = gettime_r();
	double timestamp_last = timestamp_start, timestamp_temp;
	int height = 300;
	int width = 400;
	if(argc == 2) { width = height = atoi(argv[1]); }
	else if(argc == 3) { width = atoi(argv[1]); height = atoi(argv[2]); }

	// --------------------------------------------------------------------
	// load opencl kernels
	OpenCLEnv env;
	cl_int err = CL_SUCCESS;

	std::string source = LoadKernel("src/raytracer.cl");
	cl::Program::Sources sources;
	sources.push_back(std::make_pair(&source[0],source.size()));
	cl::Program program(env.context, sources);

	try {
		// -cl-opt-disable -cl-strict-aliasing
		program.build(env.devices, "-Werror");
	} catch(const cl::Error& err) {
		auto errlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(env.devices[0]);
		std::cerr<<errlog;
		throw;
	}
	auto errlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(env.devices[0]);
	std::cerr<<errlog;

	cl::CommandQueue queue(env.context, env.devices[0], 0, &err);

	OUTPUT_TIMEUSED("setup opencl: ");

	// --------------------------------------------------------------------
	// construct model

	Float4* imagebuffer = new Float4[width*height];
	std::vector<char> model = getmodel();

	OUTPUT_TIMEUSED("construct scene: ");

	// --------------------------------------------------------------------
	// pre compute

	int* crandbuf = new int[FWIDTH*FHEIGHT];
	for(int i=0;i!=FWIDTH*FHEIGHT;++i) {
		crandbuf[i] = rand();
	}

	OUTPUT_TIMEUSED("preconputing randoms: ");

	// --------------------------------------------------------------------
	// alloc buffers

	cl::Buffer buf_model(env.context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, model.size(), &model[0]);

	cl::Buffer buf_spacemap   (env.context, CL_MEM_READ_WRITE, sizeof(int)*SPACEHASH_SIZE);
	cl::Buffer buf_photons    (env.context, CL_MEM_READ_WRITE, sizeof(Hit)*RAYLIMIT);
	cl::Buffer buf_rseed      (env.context, CL_MEM_READ_WRITE, sizeof(int)*FWIDTH*FHEIGHT);
	cl::Buffer buf_accumulator(env.context, CL_MEM_READ_WRITE, sizeof(float)*4*height*width);
	cl::Buffer buf_worker     (env.context, CL_MEM_READ_WRITE, sizeof(WorkerUnit)*NWORKERS);
	queue.enqueueWriteBuffer(buf_rseed, CL_TRUE, 0, FWIDTH*FHEIGHT*sizeof(int), crandbuf);

	// FIXME memset 0, hack
	cl::Kernel kernel_memzero(program, "memzero", &err);
	{
		kernel_memzero.setArg(0, buf_spacemap);
		kernel_memzero.setArg(1, (int)(sizeof(int)));
		queue.enqueueNDRangeKernel(kernel_memzero, cl::NullRange, cl::NDRange(SPACEHASH_SIZE), cl::NullRange);
		kernel_memzero.setArg(0, buf_accumulator);
		kernel_memzero.setArg(1, (int)(sizeof(float)*4));
		queue.enqueueNDRangeKernel(kernel_memzero, cl::NullRange, cl::NDRange(height*width), cl::NullRange);
	}

	queue.finish(); // clear all previous command before we start

	OUTPUT_TIMEUSED("buffer allocation: ");


	// --------------------------------------------------------------------
	// start raytracing and photon mapping

	cl::Kernel kernel_raycast(program, "raycast", &err);
	cl::Kernel kernel_hitmap(program,  "hitmap", &err);
	cl::Kernel kernel_revpmap(program, "revpmap", &err);

	std::vector<Hit> hits;
	std::vector<WorkerUnit> workers;

	for(int w=0;w!=width;++w)
		for(int h=0;h!=height;++h) {
			WorkerUnit unit = {
				WorkerUnit_SPAWN, h*width + w, 0, 0, 0, 0, 0, 0,
				{1.0f*w/width, 1.0f*h/height, 0, 1, 1, 1}
			};
			workers.push_back(unit);
		}

	kernel_raycast.setArg(0, buf_model);
	kernel_raycast.setArg(1, buf_worker);

	int transportcnt = 0;
	while(!workers.empty() && workers.size() + hits.size() <= RAYLIMIT && transportcnt<200) {
		std::cerr<<"round "<<transportcnt<<" with "<<workers.size() <<" rays ";
		++transportcnt;
		std::vector<WorkerUnit> iteration; iteration.swap(workers);
		for(int procidx = 0;procidx < iteration.size(); procidx+=NWORKERS) {
			int workers_count = std::min<std::size_t>(iteration.size()-procidx, NWORKERS);
			queue.enqueueWriteBuffer(buf_worker, CL_TRUE, 0,
				workers_count*sizeof(WorkerUnit), &iteration[0]+procidx);
			queue.enqueueNDRangeKernel(kernel_raycast,
				cl::NullRange,cl::NDRange(NWORKERS),cl::NullRange);
			queue.finish();
			queue.enqueueReadBuffer(buf_worker, CL_TRUE, 0,
				workers_count*sizeof(WorkerUnit), &iteration[0]+procidx);
		}
		int hitcount = hits.size();
		for(int i = 0;i < iteration.size(); ++i) {
			WorkerUnit w = iteration[i];
			if(w.flags & WorkerUnit_SPAWN)
				hits.push_back({0, w.deposit, w.ox, w.oy, w.oz, w.fx, w.fy, w.fz});
			w.flags &= ~WorkerUnit_SPAWN;
			if(w.flags & WorkerUnit_REFLECT)
				workers.push_back(w);
			w.reflectray = w.refractray;
			if(w.flags & WorkerUnit_REFRACT)
				workers.push_back(w);
		}
		std::cerr<<"hits "<< hits.size() - hitcount <<" solids\n";
	}
	std::cerr<<"light transport count: "<<transportcnt<<"\n";
	if(transportcnt==0)
		return -1;
	std::cerr<<"hit points: "<<hits.size()<<"/"<<RAYLIMIT <<" = ("<<1.0*hits.size()/RAYLIMIT<<")"<<"\n";


	kernel_hitmap.setArg(0, buf_spacemap);
	kernel_hitmap.setArg(1, buf_photons);
	queue.enqueueWriteBuffer(buf_photons, CL_TRUE, 0,
		hits.size()*sizeof(Hit), &hits[0]);
	queue.enqueueNDRangeKernel(kernel_hitmap,
		cl::NullRange,cl::NDRange(hits.size()),cl::NullRange);

	queue.finish();
	OUTPUT_TIMEUSED("ray casting: ");


	kernel_revpmap.setArg(0, buf_model);
	kernel_revpmap.setArg(1, buf_spacemap);
	kernel_revpmap.setArg(2, buf_photons);
	kernel_revpmap.setArg(3, buf_rseed);
	kernel_revpmap.setArg(4, buf_accumulator);


	for(int i=0;i!=REVPMAP_REPEATS;++i) {
		std::cerr<<"reverse photon mapping pass "<<i;
		queue.enqueueNDRangeKernel(kernel_revpmap,
			cl::NullRange,cl::NDRange(FWIDTH, FHEIGHT),cl::NullRange);
		queue.finish();
		queue.enqueueReadBuffer(buf_accumulator, CL_TRUE, 0, height*width*sizeof(Float4), imagebuffer);
		OUTPUT_TIMEUSED(" with ");
		writeImage("output/", height, width, i, imagebuffer);
	}
	do{
		timestamp_temp=gettime_r();
		std::cerr<<"tital time:"<<timestamp_temp-timestamp_start<<"s\n";
	} while(0);
	return EXIT_SUCCESS;



	// --------------------------------------------------------------------
	// running

// 	cl::Kernel kernel_hittest(program, "hittest", &err);
//
// 	kernel_hittest.setArg(0, buf_model);
// 	kernel_hittest.setArg(1, buf_out);
//
// 	queue.enqueueNDRangeKernel(kernel_hittest, cl::NullRange,cl::NDRange(width, height),cl::NullRange);
//
// 	queue.finish();
// 	queue.enqueueReadBuffer(buf_out, CL_TRUE, 0, height*width*sizeof(Float4), imagebuffer);
//
// 	writeImage("", height, width, 0, imagebuffer);
// 	OUTPUT_TIMEUSED("running kernels: ");
//
// 	timestamp_temp=gettime_r();
// 	std::cerr<<"tital time:"<<timestamp_temp-timestamp_start<<"s\n";
//
// 	return EXIT_SUCCESS;
} catch (cl::Error err) {
	std::cerr
		<< "ERROR: "
		<< err.what()
		<< "("
		<< err.err()
		<< ":"
		<< opencl_getErrorString(err.err())
		<< ")"
		<< std::endl;
	return EXIT_FAILURE;
}





// ========================================================================
// ==< temp model >========================================================

/* WARNING enable reflection with refraction is very bad
 */
# if 0
int def_material_1(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	p[MATERIAL_AMBIENT] = scene.vertex(0.02,0,0.04);
	p[MATERIAL_REFLECT] = scene.vertex(0.9, 0.9, 0.9);
	//p[MATERIAL_DIFFUSE] = scene.vertex(0.30, 0.25, 0.45);
	p[MATERIAL_SPECULAR] = scene.vertex(0.316228, 0.316228, 0.316228, 12.8);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.2,0,0.4);
	return material;
}

int def_material_1t(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	p[MATERIAL_AMBIENT] = scene.vertex(0,0.02,0.08);
	p[MATERIAL_REFLECT] = scene.vertex(0.9, 0.9, 0.9);
	//p[MATERIAL_DIFFUSE] = scene.vertex(0.6, 0.6, 0.6);
	p[MATERIAL_SPECULAR] = scene.vertex(0.316228, 0.316228, 0.316228, 70);
	p[MATERIAL_SIGNITURE] = scene.vertex(0,0.2,0.4);
	return material;
}

int def_material_1f(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	p[MATERIAL_AMBIENT] = scene.vertex(0.02,0,0.04);
	//p[MATERIAL_REFLECT] = scene.vertex(0.9, 0.9, 0.9);
	//p[MATERIAL_REFLECT] = scene.vertex(0.15, 0.12, 0.22);
	p[MATERIAL_DIFFUSE] = scene.vertex(0.2, 0.2, 0.2);
	p[MATERIAL_SPECULAR] = scene.vertex(0.316228, 0.316228, 0.316228, 12.8);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.2,0,0.4);
	return material;
}

int def_material_2(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	p[MATERIAL_AMBIENT] = scene.vertex(0.02,0,0.04);
	p[MATERIAL_REFLECT] = scene.vertex(0.54/4, 0.89/4, 0.63/4);
	p[MATERIAL_DIFFUSE] = scene.vertex(0.54, 0.89, 0.63);
	p[MATERIAL_SPECULAR] = scene.vertex(0.316228, 0.316228, 0.316228, 12.8);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.2, 0.4, 0.3);
	return material;
}

int def_material_3(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	p[MATERIAL_AMBIENT] = scene.vertex(0.10,0.05,0);
	p[MATERIAL_DIFFUSE] = scene.vertex(0.7,0.6,0.5);
	//p[MATERIAL_REFLECT] = scene.vertex(0.90,0.7,0.5);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.4, 0.2, 0);
	return material;
}

int def_material_4(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	p[MATERIAL_REFRACT] = scene.vertex(0.95, 0.95, 0.95, 1.33);
	p[MATERIAL_REFLECT] = scene.vertex(0.2, 0.2, 0.2);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.1, 0.1, 0.2);
	return material;
}

int def_material_5(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	//p[MATERIAL_REFRACT] = scene.vertex(0.95, 0.95, 0.95, 1.52);
	p[MATERIAL_DIFFUSE] = scene.vertex(0.8,0.4,0);
	//p[MATERIAL_REFLECT] = scene.vertex(0.2, 0.2, 0.2);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.1, 0.1, 0.2);
	return material;
}

int def_material_6(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	p[MATERIAL_REFLECT] = scene.vertex(0.95, 0.95, 0.95);
	//p[MATERIAL_DIFFUSE] = scene.vertex(0.8,0.4,0);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.1, 0.1, 0.2);
	return material;
}

int def_material_7(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	//p[MATERIAL_REFLECT] = scene.vertex(0.95, 0.95, 0.95);
	//p[MATERIAL_DIFFUSE] = scene.vertex(0.2,0.2,0.2);
	p[MATERIAL_REFRACT] = scene.vertex(0.95, 0.95, 0.95, 1.45);
	p[MATERIAL_REFLECT] = scene.vertex(0.2, 0.2, 0.2);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.1, 0.1, 0.2);
	return material;
}

int def_material_8(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	//p[MATERIAL_REFLECT] = scene.vertex(0.95, 0.95, 0.95);
	p[MATERIAL_DIFFUSE] = scene.vertex(0.3,0.2,0.4);
	//p[MATERIAL_REFRACT] = scene.vertex(0.5, 0.5, 0.5, 1.45);
	p[MATERIAL_REFLECT] = scene.vertex(0.3, 0.2, 0.4);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.1, 0.1, 0.2);
	return material;
}
int def_material_light(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	//p[MATERIAL_REFLECT] = scene.vertex(0.95, 0.95, 0.95);
	p[MATERIAL_DIFFUSE] = scene.vertex(0.05, 0.05, 0.05);
	p[MATERIAL_REFRACT] = scene.vertex(0.95, 0.95, 0.95, 1);
	//p[MATERIAL_REFLECT] = scene.vertex(0.3, 0.2, 0.4);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.1, 0.1, 0.2);
	return material;
}

int def_material_refractor(Scene& scene) {
	int material = scene.append_struct(TYPE_MATERIAL, MATERIAL_END);
	int* p = &scene[material];
	p[MATERIAL_REFRACT] = scene.vertex(0.95, 0.95, 0.95, 4);
	//p[MATERIAL_DIFFUSE] = scene.vertex(0.8,0.4,0);
	//p[MATERIAL_REFLECT] = scene.vertex(0.6, 0.6, 0.6);
	p[MATERIAL_SIGNITURE] = scene.vertex(0.1, 0.1, 0.2);
	return material;
}
#endif
int construct_model(Scene& scene) {
#if 0
	int teapot = scene.append_BVH_Mesh(MeshObject::FromFile("model/teapot.obj", 0.003));
	int bunny = scene.append_BVH_Mesh(MeshObject::FromFile("model/bunny.obj", 10));
	int tetrahedron = scene.append_BVH_Mesh(MeshObject::FromFile("model/tetrahedron.obj"));
	int room = scene.append_BVH_Mesh(MeshObject::FromFile("model/room.obj"));
	int icosahedron = scene.append_BVH_Mesh(MeshObject::FromFile("model/icosahedron.obj"));
	int triprism = scene.append_BVH_Mesh(MeshObject::FromFile("model/triprism.obj"));
	int sphere = scene.append_struct(TYPE_SPHERE, SPHERE_END);
		scene[sphere+1] = scene.vertex(0,0,0,1);

	int material_1 = def_material_1(scene);
	int material_1t = def_material_1t(scene);
	int material_1f = def_material_1f(scene);
	int material_2 = def_material_2(scene);
	int material_3 = def_material_3(scene);
	int material_4 = def_material_4(scene);
	int material_5 = def_material_5(scene);
	int material_6 = def_material_6(scene);
	int material_7 = def_material_7(scene);
	int material_8 = def_material_8(scene);
	int material_light = def_material_light(scene);

	int material_refractor = def_material_refractor(scene);

	std::vector<int> root_objects;
	/* room */ {
		int box = scene.append_BOX(room);
		scene[box+BVH_MATERIAL] = material_1;
		root_objects.push_back(box);
	}
	/* room */ {
		int box = scene.append_BOX(room);
		TrMatrix m;
			m.translate(0.01,0.01,0.01);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_1t;
		root_objects.push_back(box);
	}
	/* floor */ {
		int box = scene.append_BOX(room);
		TrMatrix m;
			m.translate(0,-1.98,0);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_1f;
		root_objects.push_back(box);
	}
	/* box */  {
		int box = scene.append_BOX(room);
		TrMatrix m;
			m.translate(0.5,-0.6,-0.5);
			m.scale(0.2,0.3,0.2);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_3;
		root_objects.push_back(box);
	}
	/* light */  {
		int box = scene.append_BOX(room);
		TrMatrix m;
			m.translate(0.0, 0.99, 0.0);
			m.scale(0.167,0.01,0.167);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_light;
		root_objects.push_back(box);
	}
//#if 0
	/* mag glass  {
		TrMatrix m;
			m.translate(-0.4, -0.4, 0.4);
			m.rotate('x', 60);
			m.rotate('z', -10);
			m.scale(0.3, 0.06, 0.3);
		int box = scene.append_BOX(sphere);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_4;
		root_objects.push_back(box);
	} */
	/* teapot */ {
		TrMatrix m;
			m.translate(-0.5,-0.65,-0.5);
			m.scale(1.5,1.5,1.5);
		int box = scene.append_BOX(teapot);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_2;
		root_objects.push_back(box);
	}
	/* bunny */  {
		TrMatrix m;
			m.translate(0,-0.7,0.3);
			m.rotate('y', 90);
			m.scale(0.25,0.25,0.25);
		int box = scene.append_BOX(bunny);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_7;
		root_objects.push_back(box);
	}
	/* tetrahedron  {
		TrMatrix m;
			m.translate(0,-0.4,0);
			m.scale(0.5,0.5,0.5);
		int box = scene.append_BOX(tetrahedron);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_3;
		root_objects.push_back(box);
	} */
//#else
	/* sphere */ {
		TrMatrix m;
			m.translate(0.65, -0.65, 0.3);
			m.scale(0.3, 0.3, 0.3);
		int box = scene.append_BOX(sphere);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_6;
		root_objects.push_back(box);
	}
	/* sphere  {
		TrMatrix m;
			m.translate(0, -0.4, 0.4);
			m.scale(0.06, 0.06, 0.06);
		int box = scene.append_BOX(sphere);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_4;
		root_objects.push_back(box);
	} */
	/* sphere */ {
		TrMatrix m;
			m.translate(-0.2, -0.3, -0.2);
			m.scale(0.3, 0.1, 0.3);
		int box = scene.append_BOX(sphere);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_4;
		root_objects.push_back(box);
	}
	/* cube */  {
		TrMatrix m;
			m.translate(0.2, 0.3, -0.3);
			m.rotate('z', -10);
			m.rotate('y', 30);
			m.scale(0.2, 0.2, 0.2);
		int box = scene.append_BOX(room);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_8;
		root_objects.push_back(box);
	}
	/* triprism  {
		int box = scene.append_BOX(triprism);
		TrMatrix m;
			//m.translate(-0.8, -0.7, 0.4);
			m.translate(0, 0, 0);
			m.rotate('y', 45);
			m.rotate('x', 90);
			m.scale(0.2, 0.2, 0.99);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_5; //material_refractor
		root_objects.push_back(box);
	} */
	/* icosahedron */ {
		TrMatrix m;
			m.translate(-0.4, -0.6, 0.3);
			m.scale(0.2, 0.2, 0.2);
		int box = scene.append_BOX(icosahedron);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_5;
		root_objects.push_back(box);
	}
	/* droplet  {
		TrMatrix m;
			m.translate(0, 0, 0.8);
			m.scale(0.1, 0.1, 0.1);
		int box = scene.append_BOX(sphere);
		scene[box+BVH_MATRIX] = scene.matrix(m);
		scene[box+BVH_MATERIAL] = material_refractor;
		root_objects.push_back(box);
	}*/
//#endif
	return scene.append_LIST(root_objects);
#endif
	return 0;
}

// ========================================================================
// ==< BVH related >=======================================================

int compute_AABB(Scene& scene, const MeshObject& mesh, std::vector<int> ids) {
	std::vector<Point3D> points;
	for(int id:ids) {
		points.push_back(mesh.vertices[mesh.triangles[id].a]);
		points.push_back(mesh.vertices[mesh.triangles[id].b]);
		points.push_back(mesh.vertices[mesh.triangles[id].c]);
	}
	auto cmp_x = [](Point3D a, Point3D b){return a.x<b.x;};
	auto cmp_y = [](Point3D a, Point3D b){return a.y<b.y;};
	auto cmp_z = [](Point3D a, Point3D b){return a.z<b.z;};
	auto minmax_x = std::minmax_element(points.begin(),points.end(), cmp_x);
	auto minmax_y = std::minmax_element(points.begin(),points.end(), cmp_y);
	auto minmax_z = std::minmax_element(points.begin(),points.end(), cmp_z);

	int box_delm = scene.vertices_count();
	scene.vertex(minmax_x.first->x,minmax_y.first->y,minmax_z.first->z);
	scene.vertex(minmax_x.second->x,minmax_y.second->y,minmax_z.second->z);
	// std::cerr<<"["<<minmax_x.first->x<<"->"<<minmax_x.second->x<<"]";
	return box_delm;
}


int compute_BVH(
	Scene& scene,
	const MeshObject& mesh,
	const std::vector<Point3D>& centers,
	std::vector<int> ids,
	int trg_base,
	int cutoff,
	int parent = 0,
	int parentoff = 0
) {
	auto cmp_x = [&centers](int a, int b){return centers[a].x<centers[b].x;};
	auto cmp_y = [&centers](int a, int b){return centers[a].y<centers[b].y;};
	auto cmp_z = [&centers](int a, int b){return centers[a].z<centers[b].z;};
	auto minmax_x = std::minmax_element(ids.begin(),ids.end(), cmp_x);
	auto minmax_y = std::minmax_element(ids.begin(),ids.end(), cmp_y);
	auto minmax_z = std::minmax_element(ids.begin(),ids.end(), cmp_z);
# if 0
	int box_delm =compute_AABB(scene, mesh, ids);
	if(ids.size()<cutoff) {
		int box = scene.append_struct(TYPE_BVH, ids.size()+BVH_END);
		scene[box+1] = parent;
		scene[box+2] = parentoff;
		scene[box+3] = box_delm;
		scene[box+4] = 0;
		scene[box+BVH_SIZE] = ids.size();
		int offset = box+BVH_END;
		for(int tid:ids)
			scene[offset++] = trg_base+tid*4;
		return box;
	} else {
		float diff_x = centers[*minmax_x.second].x - centers[*minmax_x.first].x;
		float diff_y = centers[*minmax_y.second].y - centers[*minmax_y.first].y;
		float diff_z = centers[*minmax_z.second].z - centers[*minmax_z.first].z;
		if(diff_z > diff_y) {
			if(diff_z > diff_x) {
				std::sort(ids.begin(),ids.end(), cmp_z);
			} else {
				std::sort(ids.begin(),ids.end(), cmp_x);
			}
		} else {
			if(diff_y > diff_x) {
				std::sort(ids.begin(),ids.end(), cmp_y);
			} else {
				std::sort(ids.begin(),ids.end(), cmp_x);
			}
		}
		int box = scene.append_struct(TYPE_BVH, BVH_END+2);
		scene[box+1] = parent;
		scene[box+2] = parentoff;
		scene[box+3] = box_delm;
		scene[box+4] = 0;
		scene[box+BVH_SIZE] = 2;
		scene[box+BVH_END] = compute_BVH(scene, mesh, centers, std::vector<int>(
			ids.begin(), ids.begin()+ids.size()/2), trg_base, cutoff, box, 0);
		scene[box+BVH_END+1] = compute_BVH(scene, mesh, centers, std::vector<int>(
			ids.begin()+ids.size()/2, ids.end()), trg_base, cutoff, box, 1);
		return box;
	}
#endif
	return 0;
}

// ========================================================================
// ==< class Scene >=======================================================
Scene::Scene() {
	structs.push_back(0);
	vertex(0,0,0);
	matrix({1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1});
}

int Scene::matrix(const Matrix4x4& m) {
	for(float f:m.v)
		matrices.push_back(f);
	return matrices.size()/16-1;
}

int Scene::matrix(const TrMatrix& m) {
	int id=matrix(m.mtx);matrix(m.inv);return id;
}

int Scene::vertex(float x, float y, float z, float w) {
	vertices.push_back(x);
	vertices.push_back(y);
	vertices.push_back(z);
	vertices.push_back(w);
	return vertices.size()/4-1;
}

int Scene::append_struct(int type, int size) {
	int offset = structs.size();
	// TODO
	structs.push_back(type);
	structs.resize(structs.size()+size-1);
	return offset;
}

int Scene::append_BOX(int x) {
# if 0
	int sbox = append_struct(TYPE_BVH, BVH_END+1);
	structs[sbox+BVH_SIZE] = 1;
	structs[sbox+BVH_END] = x;
	return sbox;
#endif
	return 0;
}

int Scene::append_BVH_Mesh(const MeshObject& mesh, int cutoff) {
# if 0
	std::vector<Point3D> centers;
	std::vector<int> ids;
	int vtx_idx = vertices_count();
	for(Point3D p:mesh.vertices)
		vertex(p.x,p.y,p.z);
	int trg_base = structs.size();
	for(int i=0;i!=mesh.triangles.size();++i) {
		ids.push_back(i);
		int idx = append_struct(TYPE_TRIANGLE, TRIANGLE_END);
		structs[idx+1] = mesh.triangles[i].a + vtx_idx;
		structs[idx+2] = mesh.triangles[i].b + vtx_idx;
		structs[idx+3] = mesh.triangles[i].c + vtx_idx;

		Point3D a = mesh.vertices[mesh.triangles[i].a];
		Point3D b = mesh.vertices[mesh.triangles[i].b];
		Point3D c = mesh.vertices[mesh.triangles[i].c];
		Point3D p = {
			( a.x + b.x + c.x ) / 3,
			( a.y + b.y + c.y ) / 3,
			( a.z + b.z + c.z ) / 3
		};
		centers.push_back(p);
	}
	return compute_BVH(*this, mesh, centers, ids, trg_base, cutoff);
#endif
	return 0;
}


BVH_id_t append_BVH_Photon(const MeshObject& mesh, int cutoff = 5) {
	return 0;
}

int Scene::append_LIST(std::vector<int> l) {
#if 0
	int box = append_struct(TYPE_BVH, BVH_END+l.size());
	(*this)[box+BVH_SIZE] = l.size();
	for(int i=0;i!=l.size();++i)
		(*this)[box+BVH_END+i] = l[i];
	return box;
#endif
	return 0;
}

// ========================================================================
// ==< class MeshObject >==================================================

MeshObject MeshObject::FromFile(const char* name, double scale) {
	MeshObject m;
	std::ifstream f(name);
	std::string s;
	int triangle_count = 0;
	while(getline(f, s)) {
		std::stringstream ss(s);
		std::string tag; ss>>tag;
		if(tag == "v") {
			Point3D p; ss >> p.x >> p.y >> p.z;
			p.x*=scale;p.y*=scale;p.z*=scale;
			m.vertices.push_back(p);
		}
		if(tag == "f") {
			Triangle p; char tc; int ti;
			ss >> p.a; while(ss.peek() == '/') ss>>tc>>ti;
			ss >> p.b; while(ss.peek() == '/') ss>>tc>>ti;
			ss >> p.c;
			--p.a; --p.b; --p.c;
			m.triangles.push_back(p);
		}
	}
	return m;
}

// ========================================================================
// ==< class Matrix4x4 >===================================================

Matrix4x4 operator*(const Matrix4x4& a, const Matrix4x4& b) {
	return {
		a(0,0)*b(0,0)+a(0,1)*b(1,0)+a(0,2)*b(2,0)+a(0,3)*b(3,0),
		a(0,0)*b(0,1)+a(0,1)*b(1,1)+a(0,2)*b(2,1)+a(0,3)*b(3,1),
		a(0,0)*b(0,2)+a(0,1)*b(1,2)+a(0,2)*b(2,2)+a(0,3)*b(3,2),
		a(0,0)*b(0,3)+a(0,1)*b(1,3)+a(0,2)*b(2,3)+a(0,3)*b(3,3),
		a(1,0)*b(0,0)+a(1,1)*b(1,0)+a(1,2)*b(2,0)+a(1,3)*b(3,0),
		a(1,0)*b(0,1)+a(1,1)*b(1,1)+a(1,2)*b(2,1)+a(1,3)*b(3,1),
		a(1,0)*b(0,2)+a(1,1)*b(1,2)+a(1,2)*b(2,2)+a(1,3)*b(3,2),
		a(1,0)*b(0,3)+a(1,1)*b(1,3)+a(1,2)*b(2,3)+a(1,3)*b(3,3),
		a(2,0)*b(0,0)+a(2,1)*b(1,0)+a(2,2)*b(2,0)+a(2,3)*b(3,0),
		a(2,0)*b(0,1)+a(2,1)*b(1,1)+a(2,2)*b(2,1)+a(2,3)*b(3,1),
		a(2,0)*b(0,2)+a(2,1)*b(1,2)+a(2,2)*b(2,2)+a(2,3)*b(3,2),
		a(2,0)*b(0,3)+a(2,1)*b(1,3)+a(2,2)*b(2,3)+a(2,3)*b(3,3),
		a(3,0)*b(0,0)+a(3,1)*b(1,0)+a(3,2)*b(2,0)+a(3,3)*b(3,0),
		a(3,0)*b(0,1)+a(3,1)*b(1,1)+a(3,2)*b(2,1)+a(3,3)*b(3,1),
		a(3,0)*b(0,2)+a(3,1)*b(1,2)+a(3,2)*b(2,2)+a(3,3)*b(3,2),
		a(3,0)*b(0,3)+a(3,1)*b(1,3)+a(3,2)*b(2,3)+a(3,3)*b(3,3)
	};
}

// ========================================================================
// ==< class TrMatrix >====================================================

void TrMatrix::rotate(char axis, double angle) {
	Matrix4x4 rotation;
	double toRadian = 2*M_PI/360.0;
	int i;

	for (i = 0; i < 2; i++) {
		switch(axis) {
			case 'x':
				rotation(0,0) = 1;
				rotation(1,1) = cos(angle*toRadian);
				rotation(1,2) = -sin(angle*toRadian);
				rotation(2,1) = sin(angle*toRadian);
				rotation(2,2) = cos(angle*toRadian);
				rotation(3,3) = 1;
			break;
			case 'y':
				rotation(0,0) = cos(angle*toRadian);
				rotation(0,2) = sin(angle*toRadian);
				rotation(1,1) = 1;
				rotation(2,0) = -sin(angle*toRadian);
				rotation(2,2) = cos(angle*toRadian);
				rotation(3,3) = 1;
			break;
			case 'z':
				rotation(0,0) = cos(angle*toRadian);
				rotation(0,1) = -sin(angle*toRadian);
				rotation(1,0) = sin(angle*toRadian);
				rotation(1,1) = cos(angle*toRadian);
				rotation(2,2) = 1;
				rotation(3,3) = 1;
			break;
		}
		if (i == 0) {
			mtx = mtx*rotation;
			angle = -angle;
		}
		else {
			inv = rotation*inv;
		}
	}
}


void TrMatrix::translate(double x, double y, double z) {
	Matrix4x4 translation;

	translation(0,3) = x;
	translation(1,3) = y;
	translation(2,3) = z;
	mtx = mtx*translation;
	translation(0,3) = -x;
	translation(1,3) = -y;
	translation(2,3) = -z;
	inv = translation*inv;
}

void TrMatrix::scale(double x, double y, double z) {
	Matrix4x4 scale;

	scale(0,0) = x;
	scale(1,1) = y;
	scale(2,2) = z;
	mtx = mtx*scale;
	scale(0,0) = 1/x;
	scale(1,1) = 1/y;
	scale(2,2) = 1/z;
	inv = scale*inv;
}

// ========================================================================
// ==< opencl helpers >====================================================

const char *opencl_getErrorString(cl_int error) {
switch(error){
	// run-time and JIT compiler errors
	case 0: return "CL_SUCCESS";
	case -1: return "CL_DEVICE_NOT_FOUND";
	case -2: return "CL_DEVICE_NOT_AVAILABLE";
	case -3: return "CL_COMPILER_NOT_AVAILABLE";
	case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
	case -5: return "CL_OUT_OF_RESOURCES";
	case -6: return "CL_OUT_OF_HOST_MEMORY";
	case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
	case -8: return "CL_MEM_COPY_OVERLAP";
	case -9: return "CL_IMAGE_FORMAT_MISMATCH";
	case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
	case -11: return "CL_BUILD_PROGRAM_FAILURE";
	case -12: return "CL_MAP_FAILURE";
	case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
	case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
	case -15: return "CL_COMPILE_PROGRAM_FAILURE";
	case -16: return "CL_LINKER_NOT_AVAILABLE";
	case -17: return "CL_LINK_PROGRAM_FAILURE";
	case -18: return "CL_DEVICE_PARTITION_FAILED";
	case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

	// compile-time errors
	case CL_INVALID_VALUE: return "CL_INVALID_VALUE";
	case -31: return "CL_INVALID_DEVICE_TYPE";
	case -32: return "CL_INVALID_PLATFORM";
	case -33: return "CL_INVALID_DEVICE";
	case CL_INVALID_CONTEXT: return "CL_INVALID_CONTEXT";
	case -35: return "CL_INVALID_QUEUE_PROPERTIES";
	case -36: return "CL_INVALID_COMMAND_QUEUE";
	case -37: return "CL_INVALID_HOST_PTR";
	case -38: return "CL_INVALID_MEM_OBJECT";
	case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
	case -40: return "CL_INVALID_IMAGE_SIZE";
	case -41: return "CL_INVALID_SAMPLER";
	case -42: return "CL_INVALID_BINARY";
	case -43: return "CL_INVALID_BUILD_OPTIONS";
	case -44: return "CL_INVALID_PROGRAM";
	case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
	case -46: return "CL_INVALID_KERNEL_NAME";
	case -47: return "CL_INVALID_KERNEL_DEFINITION";
	case -48: return "CL_INVALID_KERNEL";
	case -49: return "CL_INVALID_ARG_INDEX";
	case -50: return "CL_INVALID_ARG_VALUE";
	case -51: return "CL_INVALID_ARG_SIZE";
	case -52: return "CL_INVALID_KERNEL_ARGS";
	case -53: return "CL_INVALID_WORK_DIMENSION";
	case -54: return "CL_INVALID_WORK_GROUP_SIZE";
	case -55: return "CL_INVALIDmake && time ./a.out >t.ppm_WORK_ITEM_SIZE";
	case -56: return "CL_INVALID_GLOBAL_OFFSET";
	case -57: return "CL_INVALID_EVENT_WAIT_LIST";
	case CL_INVALID_EVENT: return "CL_INVALID_EVENT";
	case -59: return "CL_INVALID_OPERATION";
	case -60: return "CL_INVALID_GL_OBJECT";
	case -61: return "CL_INVALID_BUFFER_SIZE";
	case -62: return "CL_INVALID_MIP_LEVEL";
	case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
	case -64: return "CL_INVALID_PROPERTY";
	case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
	case -66: return "CL_INVALID_COMPILER_OPTIONS";
	case -67: return "CL_INVALID_LINKER_OPTIONS";
	case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

	// extension errors
	case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
	case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
	case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
	case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
	case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
	case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
	default: return "Unknown OpenCL error";
	}
}


std::string LoadKernel (const char* name) {
	std::ifstream f(name);
	// disable compiler cache -_-
	std::stringstream sst; sst<<"/*"<<std::time(0)<<"*/";
	return sst.str()+std::string(
		std::istreambuf_iterator<char>(f),
		std::istreambuf_iterator<char>());
}