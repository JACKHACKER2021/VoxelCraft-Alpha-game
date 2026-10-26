// Android entry point: EGL setup, the event loop and touch input.
#include <EGL/egl.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include <atomic>
#include <ctime>
#include <memory>
#include <string>

#include "engine/Game.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "VoxelCraft", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VoxelCraft", __VA_ARGS__)

namespace {

struct App {
    std::unique_ptr<vc::Game> game;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int width = 0, height = 0;
    bool focused = false;
};

// The running game, so the Java side can poke it through JNI.
std::atomic<vc::Game*> g_game{nullptr};
// Back gestures arrive on the UI thread and are handled on the game thread.
std::atomic<int> g_backRequests{0};

// Calls a method on VoxelActivity from the game thread (the Java side hops
// to the UI thread itself). `call` receives the env, activity and method id.
template <typename F>
void callActivity(android_app* android, const char* name, const char* sig, F call) {
    JavaVM* vm = android->activity->vm;
    JNIEnv* env = nullptr;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    jobject activity = android->activity->clazz;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, name, sig);
    if (method) call(env, activity, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
    vm->DetachCurrentThread();
}

// Proper UTF-8 both ways (JNI's own string functions use "modified UTF-8").
jstring toJavaString(JNIEnv* env, const std::string& s) {
    jbyteArray bytes = env->NewByteArray((jsize)s.size());
    env->SetByteArrayRegion(bytes, 0, (jsize)s.size(), reinterpret_cast<const jbyte*>(s.data()));
    jclass stringClass = env->FindClass("java/lang/String");
    jmethodID ctor = env->GetMethodID(stringClass, "<init>", "([BLjava/lang/String;)V");
    jstring charset = env->NewStringUTF("UTF-8");
    jstring result = (jstring)env->NewObject(stringClass, ctor, bytes, charset);
    env->DeleteLocalRef(charset);
    env->DeleteLocalRef(stringClass);
    env->DeleteLocalRef(bytes);
    return result;
}

void showTexturePackMenu(android_app* android) {
    callActivity(android, "showTexturePackMenu", "()V",
                 [](JNIEnv* env, jobject obj, jmethodID m) { env->CallVoidMethod(obj, m); });
}

void promptText(android_app* android, int id, const std::string& title, const std::string& initial,
                const char* method = "promptText") {
    callActivity(android, method, "(ILjava/lang/String;Ljava/lang/String;)V",
                 [&](JNIEnv* env, jobject obj, jmethodID m) {
                     jstring t = toJavaString(env, title), i = toJavaString(env, initial);
                     env->CallVoidMethod(obj, m, (jint)id, t, i);
                     env->DeleteLocalRef(t);
                     env->DeleteLocalRef(i);
                 });
}

double now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

bool initEgl(App& app, ANativeWindow* window) {
    app.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(app.display, nullptr, nullptr);
    const EGLint attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                              EGL_DEPTH_SIZE, 24, EGL_NONE};
    EGLConfig config;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(app.display, attribs, &config, 1, &numConfigs) || numConfigs < 1) {
        LOGE("no suitable EGL config");
        return false;
    }
    app.surface = eglCreateWindowSurface(app.display, config, window, nullptr);
    const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    app.context = eglCreateContext(app.display, config, EGL_NO_CONTEXT, ctxAttribs);
    if (app.surface == EGL_NO_SURFACE || app.context == EGL_NO_CONTEXT ||
        !eglMakeCurrent(app.display, app.surface, app.surface, app.context)) {
        LOGE("EGL setup failed: 0x%x", eglGetError());
        return false;
    }
    eglSwapInterval(app.display, 1);
    eglQuerySurface(app.display, app.surface, EGL_WIDTH, &app.width);
    eglQuerySurface(app.display, app.surface, EGL_HEIGHT, &app.height);
    return true;
}

void termEgl(App& app) {
    if (app.display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(app.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (app.context != EGL_NO_CONTEXT) eglDestroyContext(app.display, app.context);
    if (app.surface != EGL_NO_SURFACE) eglDestroySurface(app.display, app.surface);
    eglTerminate(app.display);
    app.display = EGL_NO_DISPLAY;
    app.surface = EGL_NO_SURFACE;
    app.context = EGL_NO_CONTEXT;
}

void onCmd(android_app* android, int32_t cmd) {
    App& app = *static_cast<App*>(android->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (android->window && initEgl(app, android->window)) {
                app.game->onSurfaceCreated();
                app.game->onResize(app.width, app.height);
                LOGI("surface %dx%d", app.width, app.height);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            app.game->onSurfaceDestroyed();
            termEgl(app);
            break;
        case APP_CMD_PAUSE: app.game->onAppPause(); break;
        case APP_CMD_GAINED_FOCUS: app.focused = true; break;
        case APP_CMD_LOST_FOCUS: app.focused = false; break;
        default: break;
    }
}

int32_t onInput(android_app* android, AInputEvent* event) {
    App& app = *static_cast<App*>(android->userData);
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
        if (AKeyEvent_getKeyCode(event) != AKEYCODE_BACK) return 0;
        if (AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_UP) ++g_backRequests;
        return 1;
    }
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int32_t action = AMotionEvent_getAction(event);
    int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
    size_t index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    double t = now();
    auto send = [&](size_t i, vc::TouchAction a) {
        app.game->onTouch(AMotionEvent_getPointerId(event, i), a, AMotionEvent_getX(event, i),
                          AMotionEvent_getY(event, i), t);
    };
    switch (masked) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: send(index, vc::TouchAction::Down); break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: send(index, vc::TouchAction::Up); break;
        case AMOTION_EVENT_ACTION_CANCEL:
            for (size_t i = 0; i < AMotionEvent_getPointerCount(event); ++i) send(i, vc::TouchAction::Up);
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            for (size_t i = 0; i < AMotionEvent_getPointerCount(event); ++i) send(i, vc::TouchAction::Move);
            break;
        default: return 0;
    }
    return 1;
}

}  // namespace

void android_main(android_app* android) {
    App app;
    app.game = std::make_unique<vc::Game>(android->activity->internalDataPath);
    app.game->onTexturePackButton = [android] { showTexturePackMenu(android); };
    app.game->onTextPrompt = [android](int id, const std::string& title, const std::string& initial) {
        promptText(android, id, title, initial);
    };
    app.game->onEditText = [android](int id, const std::string& title, const std::string& text) {
        promptText(android, id, title, text, "editText");
    };
    app.game->onImportPack = [android] {
        callActivity(android, "importPack", "()V", [](JNIEnv* env, jobject obj, jmethodID m) { env->CallVoidMethod(obj, m); });
    };
    app.game->onVibrate = [android](int ms) {
        callActivity(android, "vibrate", "(I)V",
                     [ms](JNIEnv* env, jobject obj, jmethodID m) { env->CallVoidMethod(obj, m, (jint)ms); });
    };
    app.game->onQuit = [android] { ANativeActivity_finish(android->activity); };
    app.game->onPlaySound = [android](const std::string& name, float volume) {
        callActivity(android, "playSound", "(Ljava/lang/String;F)V", [&](JNIEnv* env, jobject obj, jmethodID m) {
            jstring n = toJavaString(env, name);
            env->CallVoidMethod(obj, m, n, (jfloat)volume);
            env->DeleteLocalRef(n);
        });
    };
    app.game->onMusicVolume = [android](float volume) {
        callActivity(android, "setMusicVolume", "(F)V",
                     [volume](JNIEnv* env, jobject obj, jmethodID m) { env->CallVoidMethod(obj, m, (jfloat)volume); });
    };
    app.game->onPlayMusic = [android](const std::string& source, bool loop) {
        callActivity(android, "playMusic", "(Ljava/lang/String;Z)V", [&](JNIEnv* env, jobject obj, jmethodID m) {
            jstring s = toJavaString(env, source);
            env->CallVoidMethod(obj, m, s, (jboolean)loop);
            env->DeleteLocalRef(s);
        });
    };
    app.game->onImportSkin = [android] {
        callActivity(android, "importSkin", "()V", [](JNIEnv* env, jobject obj, jmethodID m) { env->CallVoidMethod(obj, m); });
    };
    app.game->onLanguage = [android](const std::string& code) {
        callActivity(android, "setLanguage", "(Ljava/lang/String;)V", [&](JNIEnv* env, jobject obj, jmethodID m) {
            jstring c = toJavaString(env, code);
            env->CallVoidMethod(obj, m, c);
            env->DeleteLocalRef(c);
        });
    };
    app.game->onLanActive = [android](bool active) {
        callActivity(android, "setLanActive", "(Z)V",
                     [&](JNIEnv* env, jobject obj, jmethodID m) { env->CallVoidMethod(obj, m, (jboolean)active); });
    };
    app.game->onPasswordPrompt = [android](int id, const std::string& title) {
        promptText(android, id, title, "", "promptPassword");
    };
    app.game->onImportStructure = [android] {
        callActivity(android, "importStructure", "()V", [](JNIEnv* env, jobject obj, jmethodID m) { env->CallVoidMethod(obj, m); });
    };
    app.game->onImportWorld = [android] {
        callActivity(android, "importWorld", "()V", [](JNIEnv* env, jobject obj, jmethodID m) { env->CallVoidMethod(obj, m); });
    };
    app.game->onExportWorld = [android](const std::string& file, const std::string& name) {
        callActivity(android, "exportWorld", "(Ljava/lang/String;Ljava/lang/String;)V", [&](JNIEnv* env, jobject obj, jmethodID m) {
            jstring f = toJavaString(env, file), n = toJavaString(env, name);
            env->CallVoidMethod(obj, m, f, n);
            env->DeleteLocalRef(f);
            env->DeleteLocalRef(n);
        });
    };
    g_game = app.game.get();
    android->userData = &app;
    android->onAppCmd = onCmd;
    android->onInputEvent = onInput;

    while (!android->destroyRequested) {
        int events;
        android_poll_source* source;
        bool active = app.focused && app.surface != EGL_NO_SURFACE;
        while (ALooper_pollOnce(active ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(android, source);
            if (android->destroyRequested) break;
            active = app.focused && app.surface != EGL_NO_SURFACE;
        }
        if (android->destroyRequested) break;
        for (int n = g_backRequests.exchange(0); n > 0; --n)
            if (!app.game->onBack()) ANativeActivity_finish(android->activity);
        if (app.surface == EGL_NO_SURFACE) continue;

        EGLint w, h;
        eglQuerySurface(app.display, app.surface, EGL_WIDTH, &w);
        eglQuerySurface(app.display, app.surface, EGL_HEIGHT, &h);
        if (w != app.width || h != app.height) {
            app.width = w;
            app.height = h;
            app.game->onResize(w, h);
        }
        app.game->frame(now());
        eglSwapBuffers(app.display, app.surface);
    }
    g_game = nullptr;
    app.game->onSurfaceDestroyed();
    termEgl(app);
    app.game.reset();  // saves the open world
}

extern "C" JNIEXPORT void JNICALL Java_cz_voxelcraft_VoxelActivity_nativeTexturePackChanged(JNIEnv*, jobject) {
    if (vc::Game* game = g_game.load()) game->requestTextureReload();
}

extern "C" JNIEXPORT void JNICALL Java_cz_voxelcraft_VoxelActivity_nativeTextResult(JNIEnv* env, jobject, jint id,
                                                                                    jbyteArray utf8) {
    vc::Game* game = g_game.load();
    if (!game || !utf8) return;
    jsize n = env->GetArrayLength(utf8);
    std::string text((size_t)n, '\0');
    env->GetByteArrayRegion(utf8, 0, n, reinterpret_cast<jbyte*>(text.data()));
    game->onTextResult(id, text);
}

extern "C" JNIEXPORT void JNICALL Java_cz_voxelcraft_VoxelActivity_nativeBack(JNIEnv*, jobject) { ++g_backRequests; }

extern "C" JNIEXPORT void JNICALL Java_cz_voxelcraft_VoxelActivity_nativePacksChanged(JNIEnv*, jobject) {
    if (vc::Game* game = g_game.load()) game->requestPacksReload();
}
