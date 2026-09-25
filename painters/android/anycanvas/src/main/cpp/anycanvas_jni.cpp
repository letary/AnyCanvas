// JNI trampolines over the C API for io.letary.anycanvas.AnyCanvas. Strings travel as NUL-separated
// UTF-8 byte arrays (see AnyCanvas.kt); a draw list comes back as Object[]{ float[] words, byte[] strings }.
#include <jni.h>

#include "anycanvas/anycanvas.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

// Splits a NUL-separated buffer into C strings (pointers into `storage`).
void splitStrings(JNIEnv* env, jbyteArray packed, std::vector<char>& storage, std::vector<const char*>& out) {
  out.clear();
  const jsize n = packed ? env->GetArrayLength(packed) : 0;
  storage.assign((size_t)n + 1, 0);
  if (n > 0) env->GetByteArrayRegion(packed, 0, n, reinterpret_cast<jbyte*>(storage.data()));
  size_t start = 0;
  for (size_t i = 0; i < (size_t)n; i++) {
    if (storage[i] == 0) { out.push_back(storage.data() + start); start = i + 1; }
  }
}

jobjectArray packDrawList(JNIEnv* env, const ac_drawlist& list) {
  jfloatArray words = env->NewFloatArray(list.wordCount);
  if (list.wordCount > 0) env->SetFloatArrayRegion(words, 0, list.wordCount, list.words);
  std::string packed;
  for (int32_t i = 0; i < list.stringCount; i++) { packed += list.strings[i] ? list.strings[i] : ""; packed.push_back('\0'); }
  jbyteArray bytes = env->NewByteArray((jsize)packed.size());
  if (!packed.empty()) env->SetByteArrayRegion(bytes, 0, (jsize)packed.size(), reinterpret_cast<const jbyte*>(packed.data()));
  jclass objectClass = env->FindClass("java/lang/Object");
  jobjectArray result = env->NewObjectArray(2, objectClass, nullptr);
  env->SetObjectArrayElement(result, 0, words);
  env->SetObjectArrayElement(result, 1, bytes);
  return result;
}

}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL Java_io_letary_anycanvas_AnyCanvas_nativeCreate(JNIEnv*, jclass) {
  return reinterpret_cast<jlong>(ac_context_create());
}

JNIEXPORT void JNICALL Java_io_letary_anycanvas_AnyCanvas_nativeDestroy(JNIEnv*, jclass, jlong ctx) {
  ac_context_destroy(reinterpret_cast<ac_context*>(ctx));
}

JNIEXPORT jobjectArray JNICALL Java_io_letary_anycanvas_AnyCanvas_nativeInterpret(JNIEnv* env, jclass, jlong ctx, jfloatArray cmd, jbyteArray refs, jfloat scale) {
  const jsize n = cmd ? env->GetArrayLength(cmd) : 0;
  std::vector<float> words((size_t)n);
  if (n > 0) env->GetFloatArrayRegion(cmd, 0, n, words.data());
  std::vector<char> storage;
  std::vector<const char*> refPtrs;
  splitStrings(env, refs, storage, refPtrs);
  ac_drawlist list = {};
  ac_interpret(reinterpret_cast<ac_context*>(ctx), words.data(), n, refPtrs.data(), (int32_t)refPtrs.size(), scale, &list);
  return packDrawList(env, list);
}

JNIEXPORT jlong JNICALL Java_io_letary_anycanvas_AnyCanvas_nativeSvgParse(JNIEnv* env, jclass, jbyteArray markup) {
  const jsize n = markup ? env->GetArrayLength(markup) : 0;
  if (n <= 0) return 0;
  std::vector<char> bytes((size_t)n);
  env->GetByteArrayRegion(markup, 0, n, reinterpret_cast<jbyte*>(bytes.data()));
  return reinterpret_cast<jlong>(ac_svg_parse(bytes.data(), (size_t)n));
}

JNIEXPORT jfloatArray JNICALL Java_io_letary_anycanvas_AnyCanvas_nativeSvgSize(JNIEnv* env, jclass, jlong svg) {
  float wh[2] = { 0, 0 };
  ac_svg_size(reinterpret_cast<const ac_svg*>(svg), &wh[0], &wh[1]);
  jfloatArray out = env->NewFloatArray(2);
  env->SetFloatArrayRegion(out, 0, 2, wh);
  return out;
}

JNIEXPORT jobjectArray JNICALL Java_io_letary_anycanvas_AnyCanvas_nativeSvgDraw(JNIEnv* env, jclass, jlong ctx, jlong svg, jfloat w, jfloat h, jboolean hasTint, jint tintArgb) {
  const float tint[4] = {
    ((tintArgb >> 16) & 0xff) / 255.0f, ((tintArgb >> 8) & 0xff) / 255.0f, (tintArgb & 0xff) / 255.0f, ((tintArgb >> 24) & 0xff) / 255.0f,
  };
  ac_drawlist list = {};
  ac_svg_draw(reinterpret_cast<ac_context*>(ctx), reinterpret_cast<const ac_svg*>(svg), w, h, hasTint ? 1 : 0, tint, &list);
  return packDrawList(env, list);
}

JNIEXPORT void JNICALL Java_io_letary_anycanvas_AnyCanvas_nativeSvgFree(JNIEnv*, jclass, jlong svg) {
  ac_svg_free(reinterpret_cast<ac_svg*>(svg));
}

JNIEXPORT jboolean JNICALL Java_io_letary_anycanvas_AnyCanvas_nativeLooksLikeSvg(JNIEnv* env, jclass, jbyteArray bytes) {
  const jsize n = bytes ? env->GetArrayLength(bytes) : 0;
  if (n <= 0) return JNI_FALSE;
  std::vector<char> buf((size_t)n);
  env->GetByteArrayRegion(bytes, 0, n, reinterpret_cast<jbyte*>(buf.data()));
  return ac_looks_like_svg(buf.data(), (size_t)n) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jbyteArray JNICALL Java_io_letary_anycanvas_AnyCanvas_nativeEncode(JNIEnv* env, jclass, jbyteArray rgba, jint w, jint h, jint format, jint quality) {
  const jsize n = rgba ? env->GetArrayLength(rgba) : 0;
  if (n < w * h * 4 || w <= 0 || h <= 0) return nullptr;
  std::vector<unsigned char> px((size_t)n);
  env->GetByteArrayRegion(rgba, 0, n, reinterpret_cast<jbyte*>(px.data()));
  size_t len = 0;
  uint8_t* out = ac_encode(px.data(), w, h, format, quality, &len);
  if (!out) return nullptr;
  jbyteArray result = env->NewByteArray((jsize)len);
  env->SetByteArrayRegion(result, 0, (jsize)len, reinterpret_cast<const jbyte*>(out));
  ac_free(out);
  return result;
}

JNIEXPORT jstring JNICALL Java_io_letary_anycanvas_AnyCanvas_nativeJson(JNIEnv* env, jclass, jfloatArray words, jbyteArray strings) {
  const jsize n = words ? env->GetArrayLength(words) : 0;
  std::vector<float> w((size_t)n);
  if (n > 0) env->GetFloatArrayRegion(words, 0, n, w.data());
  std::vector<char> storage;
  std::vector<const char*> ptrs;
  splitStrings(env, strings, storage, ptrs);
  ac_drawlist list = { w.data(), n, ptrs.data(), (int32_t)ptrs.size() };
  char* json = ac_drawlist_json(&list);
  jstring out = env->NewStringUTF(json ? json : "[]");
  ac_free(json);
  return out;
}

}  // extern "C"
