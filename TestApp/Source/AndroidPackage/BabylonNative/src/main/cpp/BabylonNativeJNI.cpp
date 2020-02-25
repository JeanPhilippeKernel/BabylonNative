#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <memory>
#include <android/native_window.h> // requires ndk r5 or newer
#include <android/native_window_jni.h> // requires ndk r5 or newer
#include <android/log.h>
#include <Babylon/Console.h>
#include <Babylon/RuntimeAndroid.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <InputManager.h>

extern "C" {
JNIEXPORT void JNICALL Java_BabylonNative_Wrapper_initEngine(JNIEnv* env, jobject obj, jobject assetMgr);
JNIEXPORT void JNICALL Java_BabylonNative_Wrapper_finishEngine(JNIEnv* env, jobject obj);
JNIEXPORT void JNICALL Java_BabylonNative_Wrapper_surfaceCreated(JNIEnv* env, jobject obj, jobject surface);
JNIEXPORT void JNICALL Java_BabylonNative_Wrapper_activityOnPause(JNIEnv* env);
JNIEXPORT void JNICALL Java_BabylonNative_Wrapper_activityOnResume(JNIEnv* env);
JNIEXPORT void JNICALL Java_BabylonNative_Wrapper_surfaceChanged(JNIEnv* env, jobject obj, jint width, jint height, jobject surface);
JNIEXPORT void JNICALL Java_BabylonNative_Wrapper_loadScript(JNIEnv* env, jobject obj, jstring path);
JNIEXPORT void JNICALL Java_BabylonNative_Wrapper_eval(JNIEnv* env, jobject obj, jstring source, jstring sourceURL);
JNIEXPORT void JNICALL Java_BabylonNative_Wrapper_setTouchInfo(JNIEnv* env, jobject obj, jfloat dx, jfloat dy, jboolean down);
};

std::unique_ptr<Babylon::RuntimeAndroid> runtime{};
std::unique_ptr<InputManager::InputBuffer> inputBuffer{};

static AAssetManager* g_assetMgrNative = nullptr;
static const char* Root = "file://";
namespace
{
    // this is the way to load apk embedded assets.
    static std::vector<char> GetAssetContents(const char* filename)
    {
        std::string filenameStr{filename};
        if (filenameStr.substr(0, strlen(Root)) == std::string(Root))
        {
            filename += strlen(Root) + 1; // + "/"
        }
        std::vector<char> buffer;
        AAsset *asset = AAssetManager_open(g_assetMgrNative, filename,
                                           AASSET_MODE_UNKNOWN);
        if (asset != nullptr)
        {
            size_t size = AAsset_getLength64(asset);
            buffer.resize(size);
            AAsset_read(asset, buffer.data(), size);
            AAsset_close(asset);
        }
        return buffer;
    }
}

JNIEXPORT void JNICALL
Java_BabylonNative_Wrapper_initEngine(JNIEnv* env, jobject obj,
                                                   jobject assetMgr)
{
    auto asset_manager = AAssetManager_fromJava(env, assetMgr);
    g_assetMgrNative = asset_manager;
    jboolean iscopy;
}

JNIEXPORT void JNICALL
Java_BabylonNative_Wrapper_finishEngine(JNIEnv* env, jobject obj)
{
}

JNIEXPORT void JNICALL
Java_BabylonNative_Wrapper_surfaceCreated(JNIEnv* env, jobject obj, jobject surface)
{
    if (!runtime)
    {
        ANativeWindow *window = ANativeWindow_fromSurface(env, surface);

        int32_t width  = ANativeWindow_getWidth(window);
        int32_t height = ANativeWindow_getHeight(window);

        runtime = std::make_unique<Babylon::RuntimeAndroid>(window, Root, width, height, GetAssetContents);

        runtime->Dispatch([](Babylon::Env& env)
                          {
                              Babylon::Console::CreateInstance(env, [](const char* message, Babylon::Console::LogLevel level)
                              {
                                  switch (level)
                                  {
                                      case Babylon::Console::LogLevel::Log:
                                          __android_log_write(ANDROID_LOG_INFO, "BabylonNative", message);
                                          break;
                                      case Babylon::Console::LogLevel::Warn:
                                          __android_log_write(ANDROID_LOG_WARN, "BabylonNative", message);
                                          break;
                                      case Babylon::Console::LogLevel::Error:
                                          __android_log_write(ANDROID_LOG_ERROR, "BabylonNative", message);
                                          break;
                                  }
                              });
                          });

        inputBuffer = std::make_unique<InputManager::InputBuffer>(*runtime);
        InputManager::Initialize(*runtime, *inputBuffer);

        runtime->LoadScript("Scripts/babylon.max.js");
        runtime->LoadScript("Scripts/babylon.glTF2FileLoader.js");
        //runtime->LoadScript("Scripts/experience.js");

        runtime->UpdateSize(width, height);
    }
}

JNIEXPORT void JNICALL
Java_BabylonNative_Wrapper_surfaceChanged(JNIEnv* env, jobject obj, jint width, jint height, jobject surface)
{
    if (runtime)
    {
        ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
        runtime->UpdateSurface(width, height, window);
    }
}

JNIEXPORT void JNICALL
Java_BabylonNative_Wrapper_loadScript(JNIEnv* env, jobject obj, jstring path)
{
    if (runtime)
    {
        jboolean iscopy;
        runtime->LoadScript(env->GetStringUTFChars(path, &iscopy));
    }
}

JNIEXPORT void JNICALL
Java_BabylonNative_Wrapper_eval(JNIEnv* env, jobject obj, jstring source, jstring sourceURL)
{
    if (runtime)
    {
        jboolean iscopy;
        std::string url = env->GetStringUTFChars(sourceURL, &iscopy);
        runtime->Eval(env->GetStringUTFChars(source, &iscopy), url);
    }
}

JNIEXPORT void JNICALL
Java_BabylonNative_Wrapper_setTouchInfo(JNIEnv* env, jobject obj, jfloat x, jfloat y, jboolean down)
{
    if (inputBuffer != nullptr)
    {
        inputBuffer->SetPointerPosition(x, y);
        inputBuffer->SetPointerDown(down);
    }
}

JNIEXPORT void JNICALL
Java_BabylonNative_Wrapper_activityOnPause(JNIEnv* env)
{
    if (runtime)
    {
        runtime->Suspend();
    }
}

JNIEXPORT void JNICALL
Java_BabylonNative_Wrapper_activityOnResume(JNIEnv* env)
{
    if (runtime)
    {
        runtime->Resume();
    }
}