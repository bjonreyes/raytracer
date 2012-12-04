
#include <jni.h>
#include <time.h>
#include <android/log.h>
#include <android/bitmap.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

#include "ColorUtils.h"

#include "Point3.h"
#include "Vector3.h"
#include "Ray3.h"
#include "Sphere3.h"
#include "Scene.h"

#define  LOG_TAG    "libplasma"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

static bool VerifyBitmap(JNIEnv * env, jobject bitmap, AndroidBitmapInfo & info) {
	int ret;
	if((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
		LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
		return false;
	}
	if(info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
		LOGE("Bitmap format is not RGBA_8888 !");
		return false;
	}
	return true;
}

static int num_threads = 8;
static int num_rays;

struct thread_args {
	AndroidBitmapInfo * info;
	void * pixels;
	Scene * scene;

	int threadNum;
};

void * workerThread(void * ptr){
	struct thread_args args = * (struct thread_args *) ptr;
	for(int y = args.threadNum * args.info->height / num_threads; y < (args.threadNum + 1) * args.info->height / num_threads; y++) {
		for(int x = 0; x < args.info->width; x++) {
			if(rand() % 4 < 3)
				continue;
			uint32_t * p = pixRef(*args.info, args.pixels, x, y);
			*p = args.scene->TraceRay(x - args.info->width / 2.0f, y - args.info->height / 2.0f);
			num_rays++;
		}
	}
	return NULL;
}

static void ThreadedRayTrace(AndroidBitmapInfo & info, void * pixels, int frame) {

	Scene mScene;
	frame /= 4;

	Sphere3 sphere0 = Sphere3(100, -90, -100, 100);
	sphere0.SetMaterial(RGBAtoU32(100, 0, 0));
	mScene.Add(sphere0);

	Sphere3 sphere1 = Sphere3(100, 80+40*sin(frame/34.0f + 6), -90*sin(frame/43.0f), 50);
	sphere1.SetMaterial(RGBAtoU32(0, 100, 0));
	mScene.Add(sphere1);

	Sphere3 sphere2 = Sphere3(100, -40+10*sin(frame/54.0f + 5), 20+40*sin(frame/30.0f), 40);
	sphere2.SetMaterial(RGBAtoU32(0, 0, 100));
	mScene.Add(sphere2);

	PointLight light0 = PointLight(Point3(0, 200, -100), .01f);
	mScene.Add(light0);

	Camera camera0;
	camera0.PinHole = Point3(-800, 0, 0);
	camera0.LensPlane = 0;
	mScene.Add(camera0);

	PointLight light1 = PointLight(Point3(0, -400, -100), .005f);
	mScene.Add(light1);

	mScene.BuildAccelerationStructure();

	pthread_t * threads[num_threads];
	struct thread_args * args_array[num_threads];

	for(int i=0; i < num_threads; i++) {
		struct thread_args * args = new struct thread_args;
		args->info = &info;
		args->pixels = pixels;
		args->scene = &mScene;
		args->threadNum = i;
		threads[i] = new pthread_t;
		args_array[i] = args;
		pthread_create(threads[i],NULL,&workerThread,(void *)args);

	}
	for(int i=0; i < num_threads; i++) {
		pthread_join(*threads[i], NULL);
		delete threads[i];
		delete args_array[i];
	}
}

/*******************************************************************************************/

extern "C"
JNIEXPORT jint JNICALL Java_edu_stanford_nicd_raytracer_MainActivity_RayTrace(JNIEnv * env, jobject obj, jobject mBitmap, jint frame) {

	AndroidBitmapInfo info;
	void * mPixels;

	if(!VerifyBitmap(env, mBitmap, info))
		return 0;

	if(AndroidBitmap_lockPixels(env, mBitmap, &mPixels) < 0)
		LOGE("AndroidBitmap_lockPixels() failed!");

	num_rays = 0;
	ThreadedRayTrace(info, mPixels, frame);
	AndroidBitmap_unlockPixels(env, mBitmap);
	return num_rays;

}
