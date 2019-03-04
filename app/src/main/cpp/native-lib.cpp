#include <jni.h>
#include <string>
#include "types.h"

extern "C" int memtest();
extern "C" int memtest2(const char * szie, const char * count,stage_callback callback);


// variable and method ID from java
static JavaVM *gs_jvm=NULL;
static JNIEnv * gs_env = NULL;
static jobject gs_object=NULL;
JNIEnv * m_env;
jobject m_object;
jmethodID m_update;
jfieldID m_fid;


extern "C" JNIEXPORT jstring JNICALL
Java_com_mid_memtest_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    memtest();
    return env->NewStringUTF(hello.c_str());
}


// get a c string from java string, it's user's response to free the buffer
char* jstringTostring(JNIEnv* env, jstring jstr)
{
    char* rtn = NULL;
    jclass clsstring = env->FindClass("java/lang/String");
    jstring strencode = env->NewStringUTF("utf-8");
    jmethodID mid = env->GetMethodID(clsstring, "getBytes", "(Ljava/lang/String;)[B");
    jbyteArray barr= (jbyteArray)env->CallObjectMethod(jstr, mid, strencode);
    jsize alen = env->GetArrayLength(barr);
    jbyte* ba = env->GetByteArrayElements(barr, JNI_FALSE);
    if (alen > 0) {
        rtn = (char*)malloc(alen + 1);
        memcpy(rtn, ba, alen);
        rtn[alen] = 0;
    }
    env->ReleaseByteArrayElements(barr, ba, 0);
    return rtn;
}




void update_msg(const char *msg){
    jstring s = gs_env->NewStringUTF(msg);
    gs_env->CallVoidMethod(gs_object, m_update, s);
}

int wrap_memtest(const char *size, const char *count, stage_callback callback)
{
    jclass  clazz;
    int rv;
    gs_jvm->AttachCurrentThread(&gs_env, NULL);  // it's ok to attach repeately
    clazz = gs_env->GetObjectClass(gs_object);
    m_update = gs_env->GetMethodID(clazz, "updateProgress", "(Ljava/lang/String;)V");
    rv = memtest2(size, count, callback);
 //   gs_jvm->DetachCurrentThread();
    return rv;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_mid_memtest_MainActivity_run_1memtest(
        JNIEnv *env,
        jobject /* this */, jstring size, jstring count) {
    jint rv = 0;
    char *csize = jstringTostring(env, size);
    char * ccount = jstringTostring(env, count);
    if(csize && ccount){
        rv = wrap_memtest(csize, ccount,update_msg);
    }
    if(ccount){
        free(ccount);
    }
    if(csize){
        free(csize);
    }
    return rv;
}

extern "C" JNIEXPORT void JNICALL
Java_com_mid_memtest_MainActivity_setUp(JNIEnv *env, jobject thiz){
    m_env = env;
    jclass clazz = env->GetObjectClass(thiz);
    m_object = env->NewGlobalRef(thiz);
    m_update = env->GetMethodID(clazz, "updateProgress", "(Ljava/lang/String;)V");
    env->GetJavaVM(&gs_jvm);
    gs_object = env->NewGlobalRef(thiz);

}