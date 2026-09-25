// The AnyCanvas Android library: the Kotlin painter over android.graphics + the JNI binding of core.
// Standalone: `./gradlew :anycanvas:assembleRelease` here. Inside another Gradle build (LeCodes):
// `include(":anycanvas"); project(":anycanvas").projectDir = file("<AnyCanvas>/painters/android/anycanvas")`.
//
// `-Panycanvas.externalCore=true` (or the same line in the host's gradle.properties): no native build
// here, no libanycanvas.so. For a host that links the core statically into its OWN native library and
// compiles src/main/cpp/anycanvas_jni.cpp into it too — AnyCanvas.kt then resolves its natives from that
// library, and the app carries one core instead of two.
plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
}

val externalCore = (findProperty("anycanvas.externalCore") as String?)?.toBoolean() == true

android {
    namespace = "io.letary.anycanvas"
    compileSdk = 36

    defaultConfig {
        minSdk = 23
        if (!externalCore) {
            externalNativeBuild {
                cmake {
                    cppFlags("-std=c++17")
                    arguments("-DANDROID_STL=c++_static")
                }
            }
            ndk { abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64") }
        }
    }

    if (!externalCore) {
        externalNativeBuild {
            cmake {
                path = file("src/main/cpp/CMakeLists.txt")
                version = "3.22.1"
            }
        }
    }

    sourceSets {
        getByName("main") {
            // The generated spec enums are shared with every language: Kotlin reads them from spec/gen.
            kotlin.srcDir("../../../spec/gen")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

kotlin {
    compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) }
}
