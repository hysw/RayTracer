all: raytracer

gen: all
	./raytracer 400 300
gen-vs: all
	./raytracer 80 60
gen-l: all
	./raytracer 800 600
gen-m: all
	./raytracer 1600 1200
gen-h: all
	./raytracer 3200 2400
gen-vh: all
	./raytracer 6400 4800
gen-vvh: all
	./raytracer 12800 9600
gen-vvvh: all
	./raytracer 25600 19200

raytracer: src/main.cpp src/entity.cpp  $(wildcard inc/*)
	clang++ -I. -std=c++11 -g -lOpenCL src/{main,entity}.cpp -o raytracer