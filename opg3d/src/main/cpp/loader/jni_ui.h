// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../obb_framework_dir.h"
#include <atomic>
#include <jni.h>
#include <string>
namespace launcher {
inline std::atomic<JavaVM *> vm{nullptr};
inline jobject dialog = nullptr;
inline JNIEnv *env() {
  JNIEnv *e = nullptr;
  auto *current = vm.load();
  if (!current) {
    current = obb_framework_dir::detail::find_created_vm();
    if (current)
      vm.store(current);
  }
  if (!current)
    return nullptr;
  if (current->GetEnv(reinterpret_cast<void **>(&e), JNI_VERSION_1_6) !=
          JNI_OK &&
      current->AttachCurrentThread(&e, nullptr) != JNI_OK)
    return nullptr;
  return e;
}
inline bool failed(JNIEnv *e) {
  if (e->ExceptionCheck()) {
    e->ExceptionDescribe();
    e->ExceptionClear();
    return true;
  }
  return false;
}
inline jstring str(JNIEnv *e, const std::string &s) {
  return e->NewStringUTF(s.c_str());
}
inline jmethodID method(JNIEnv *e, jobject o, const char *n, const char *sig) {
  jclass c = e->GetObjectClass(o);
  auto m = e->GetMethodID(c, n, sig);
  e->DeleteLocalRef(c);
  return m;
}
inline jobject activity(JNIEnv *e) {
  // The game keeps its Activity here even while onCreate has not returned.
  auto app = obb_framework_dir::detail::current_application(e);
  if (app) {
    auto cl = e->CallObjectMethod(
        app, method(e, app, "getClassLoader", "()Ljava/lang/ClassLoader;"));
    if (cl) {
      auto type = static_cast<jclass>(e->CallObjectMethod(
          cl,
          method(e, cl, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"),
          str(e, "com.unity3d.player.UnityPlayer")));
      if (!failed(e) && type) {
        auto f = e->GetStaticFieldID(type, "currentActivity",
                                     "Landroid/app/Activity;");
        if (f) {
          auto a = e->GetStaticObjectField(type, f);
          if (a)
            return a;
        }
        failed(e);
      }
    }
  }

  auto c = e->FindClass("android/app/ActivityThread");
  if (!c || failed(e))
    return nullptr;
  auto mid = e->GetStaticMethodID(c, "currentActivityThread",
                                  "()Landroid/app/ActivityThread;");
  if (!mid || failed(e))
    return nullptr;
  auto at = e->CallStaticObjectMethod(c, mid);
  if (!at || failed(e))
    return nullptr;
  auto f = e->GetFieldID(c, "mActivities", "Landroid/util/ArrayMap;");
  if (!f || failed(e))
    return nullptr;
  auto map = e->GetObjectField(at, f);
  auto size = e->CallIntMethod(map, method(e, map, "size", "()I"));
  auto get = method(e, map, "valueAt", "(I)Ljava/lang/Object;");
  for (int i = 0; i < size; i++) {
    auto r = e->CallObjectMethod(map, get, i);
    auto rc = e->GetObjectClass(r);
    auto af = e->GetFieldID(rc, "activity", "Landroid/app/Activity;");
    if (af) {
      auto a = e->GetObjectField(r, af);
      if (a)
        return a;
    }
    if (failed(e))
      return nullptr;
  }
  return nullptr;
}
inline bool show(JNIEnv *e, const std::string &url) {
  // Called ONLY at the main Looper boundary. Android screenshot/scroll-capture
  // callbacks also run there; a worker-owned WebView will crash on those
  // callbacks.
  auto a = activity(e);
  if (!a)
    return false;
  auto dc = e->FindClass("android/app/Dialog");
  auto d = e->NewObject(
      dc, e->GetMethodID(dc, "<init>", "(Landroid/content/Context;I)V"), a,
      16973840);
  if (!d || failed(e))
    return false;
  dialog = e->NewGlobalRef(d);
  e->CallBooleanMethod(d, method(e, d, "requestWindowFeature", "(I)Z"), 1);
  e->CallVoidMethod(d, method(e, d, "setCancelable", "(Z)V"), JNI_FALSE);
  auto wc = e->FindClass("android/webkit/WebView");
  auto web = e->NewObject(
      wc, e->GetMethodID(wc, "<init>", "(Landroid/content/Context;)V"), a);
  if (!web || failed(e))
    return false;
  auto settings = e->CallObjectMethod(
      web, method(e, web, "getSettings", "()Landroid/webkit/WebSettings;"));
  e->CallVoidMethod(
      settings, method(e, settings, "setJavaScriptEnabled", "(Z)V"), JNI_TRUE);
  e->CallVoidMethod(settings, method(e, settings, "setAllowFileAccess", "(Z)V"),
                    JNI_FALSE);
  e->CallVoidMethod(settings,
                    method(e, settings, "setAllowContentAccess", "(Z)V"),
                    JNI_FALSE);
  e->CallVoidMethod(settings, method(e, settings, "setSaveFormData", "(Z)V"),
                    JNI_FALSE);
  e->CallVoidMethod(web, method(e, web, "setBackgroundColor", "(I)V"),
                    static_cast<jint>(0xff0b1120));
  e->CallVoidMethod(d, method(e, d, "setContentView", "(Landroid/view/View;)V"),
                    web);
  auto w = e->CallObjectMethod(
      d, method(e, d, "getWindow", "()Landroid/view/Window;"));
  e->CallVoidMethod(w, method(e, w, "setSoftInputMode", "(I)V"), 16);
  e->CallVoidMethod(w, method(e, w, "setFlags", "(II)V"), 1024, 1024);
  auto decor = e->CallObjectMethod(
      w, method(e, w, "getDecorView", "()Landroid/view/View;"));
  e->CallVoidMethod(decor, method(e, decor, "setSystemUiVisibility", "(I)V"),
                    5894);
  e->CallVoidMethod(d, method(e, d, "show", "()V"));
  e->CallVoidMethod(w, method(e, w, "setLayout", "(II)V"), -1, -1);
  e->CallVoidMethod(web, method(e, web, "loadUrl", "(Ljava/lang/String;)V"),
                    str(e, url));
  if (failed(e))
    return false;
  LOGI("loader: native-only server picker visible");
  return true;
}
inline void dismiss() {
  auto e = env();
  if (e && dialog) {
    e->CallVoidMethod(dialog, method(e, dialog, "dismiss", "()V"));
    failed(e);
  }
}
} // namespace launcher
