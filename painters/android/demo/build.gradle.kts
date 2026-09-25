// The demo / device check: bundles tests/golden as assets, runs every golden through libanycanvas.so on
// the device, compares the JSON with the expected files, and paints each draw list on screen.
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "io.letary.anycanvas.demo"
    compileSdk = 36

    defaultConfig {
        applicationId = "io.letary.anycanvas.demo"
        minSdk = 23
        targetSdk = 36
        versionCode = 1
        versionName = "0.1"
    }

    sourceSets {
        getByName("main") {
            assets.srcDir("../../../tests/golden")   // streams/, svg/, expected/
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

dependencies {
    implementation(project(":anycanvas"))
}
