# painters/android

The Kotlin painter over `android.graphics.Canvas` and the JNI binding of the core, as one Gradle
library module (`:anycanvas`, package `io.letary.anycanvas`, `libanycanvas.so`).

```kotlin
val core = AnyCanvas()                                   // one per thread of use
val list = core.interpret(stream.cmd, stream.refs, scale = density)
val painter = Painter(object : PainterHooks {
    override fun typeface(font: FontData) = FontRegistry.get(font)   // registered fonts first
    override fun image(surface: Int) = surfaces[surface]              // your bitmaps
})
painter.paint(Canvas(bitmap), list)

core.parseSvg(markup)?.use { svg -> painter.paint(canvas, svg.draw(w, h, tint = Color.RED)) }
```

The generated enums come from `spec/gen/Spec.kt` (a source dir of this module, never a copy): the
painter's `when` over `DrawCmd` is exhaustive, so a new draw command fails to compile here until it
is handled.

Build standalone: `./gradlew :anycanvas:assembleRelease` (NDK + CMake 3.22.1 from the SDK). Inside
another Gradle build, include the module by path (see build.gradle.kts). Two copies of the core in
one app are avoided by linking the core statically into ONE `.so`: a host that already embeds the
core in its own native library uses the Kotlin painter only and does not load `libanycanvas.so`.
