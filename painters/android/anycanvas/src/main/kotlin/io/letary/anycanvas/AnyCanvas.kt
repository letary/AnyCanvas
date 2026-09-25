// The Kotlin binding of the core (libanycanvas.so, the C API of core/include/anycanvas/anycanvas.h).
// Strings cross JNI as NUL-separated UTF-8 byte arrays in both directions, so text is exact (JNI's
// modified UTF-8 is not).
package io.letary.anycanvas

import java.io.Closeable

class AnyCanvas : Closeable {
    private var ctx: Long = nativeCreate()

    /** An opcode stream (f32 words + string table) → its draw list, at `scale` device px per unit. */
    fun interpret(cmd: FloatArray, refs: List<String>, scale: Float = 1f): DrawList {
        check(ctx != 0L) { "closed" }
        val raw = nativeInterpret(ctx, cmd, packStrings(refs), scale)
        return unpack(raw)
    }

    /** Parses SVG markup; null when nanosvg rejects it or the document has no size. */
    fun parseSvg(markup: String): Svg? {
        val handle = nativeSvgParse(markup.toByteArray(Charsets.UTF_8))
        return if (handle == 0L) null else Svg(handle)
    }

    /** A parsed SVG document. */
    inner class Svg internal constructor(private var handle: Long) : Closeable {
        val width: Float
        val height: Float
        init { val s = nativeSvgSize(handle); width = s[0]; height = s[1] }

        /** The draw list aspect-fitted into w × h device px (0 × 0: the natural size); `tint` ARGB replaces every color. */
        fun draw(w: Float = 0f, h: Float = 0f, tint: Int? = null): DrawList {
            check(handle != 0L && ctx != 0L) { "closed" }
            return unpack(nativeSvgDraw(ctx, handle, w, h, tint != null, tint ?: 0))
        }

        override fun close() { if (handle != 0L) { nativeSvgFree(handle); handle = 0L } }
    }

    override fun close() { if (ctx != 0L) { nativeDestroy(ctx); ctx = 0L } }

    private fun unpack(raw: Array<Any>): DrawList {
        val words = raw[0] as FloatArray
        val bytes = raw[1] as ByteArray
        return DrawList(words, unpackStrings(bytes))
    }

    companion object {
        init {
            // The standalone AAR ships libanycanvas.so. A host that links the core into its own native
            // library (the Gradle property anycanvas.externalCore) has no such file and has loaded these
            // natives already — they resolve from its library; a host that has not fails on the first call.
            try { System.loadLibrary("anycanvas") } catch (_: UnsatisfiedLinkError) {}
        }

        /** True if the bytes look like an SVG document. */
        fun looksLikeSvg(bytes: ByteArray): Boolean = nativeLooksLikeSvg(bytes)

        /** Straight RGBA8 (top row first) → PNG or JPEG bytes; null on failure. */
        fun encode(rgba: ByteArray, width: Int, height: Int, jpeg: Boolean = false, quality: Int = 90): ByteArray? =
            nativeEncode(rgba, width, height, if (jpeg) 1 else 0, quality)

        /** The deterministic JSON of a draw list (debugging, hashing). */
        fun json(list: DrawList): String = nativeJson(list.words, packStrings(list.strings.asList()))

        internal fun packStrings(strings: List<String>): ByteArray {
            val out = java.io.ByteArrayOutputStream()
            for (s in strings) { out.write(s.toByteArray(Charsets.UTF_8)); out.write(0) }
            return out.toByteArray()
        }

        internal fun unpackStrings(bytes: ByteArray): Array<String> {
            val list = ArrayList<String>()
            var start = 0
            for (i in bytes.indices) {
                if (bytes[i] == 0.toByte()) { list.add(String(bytes, start, i - start, Charsets.UTF_8)); start = i + 1 }
            }
            return list.toTypedArray()
        }

        @JvmStatic private external fun nativeCreate(): Long
        @JvmStatic private external fun nativeDestroy(ctx: Long)
        @JvmStatic private external fun nativeInterpret(ctx: Long, cmd: FloatArray, refs: ByteArray, scale: Float): Array<Any>
        @JvmStatic private external fun nativeSvgParse(markup: ByteArray): Long
        @JvmStatic private external fun nativeSvgSize(svg: Long): FloatArray
        @JvmStatic private external fun nativeSvgDraw(ctx: Long, svg: Long, w: Float, h: Float, hasTint: Boolean, tintArgb: Int): Array<Any>
        @JvmStatic private external fun nativeSvgFree(svg: Long)
        @JvmStatic private external fun nativeLooksLikeSvg(bytes: ByteArray): Boolean
        @JvmStatic private external fun nativeEncode(rgba: ByteArray, w: Int, h: Int, format: Int, quality: Int): ByteArray?
        @JvmStatic private external fun nativeJson(words: FloatArray, strings: ByteArray): String
    }
}
