// The draw list on Android: the typed commands and the reader of the word buffer (spec/draw.h).
// Colors are RGBA floats as on the wire; the painter converts to ARGB ints.
package io.letary.anycanvas

import io.letary.anycanvas.spec.DrawCmd
import io.letary.anycanvas.spec.FillRule
import io.letary.anycanvas.spec.LineCap
import io.letary.anycanvas.spec.LineJoin
import io.letary.anycanvas.spec.Paint as PaintKind
import io.letary.anycanvas.spec.Spread
import io.letary.anycanvas.spec.TextAlign
import io.letary.anycanvas.spec.TextBaseline

/** A draw list as the core produced it: f32 words + the string table. */
class DrawList(val words: FloatArray, val strings: Array<String>) {
    /** Decode into commands. A command this build does not know is skipped by its length. */
    fun commands(): List<DrawCommand> = DrawReader(this).readAll()
}

class Stop(val offset: Float, val color: FloatArray)

sealed class PaintData(val alpha: Float) {
    class Solid(alpha: Float, val color: FloatArray) : PaintData(alpha)
    /** `matrix` = [a b c d e f], gradient space → user space; the axis runs (0,0) → (1,0). */
    class Linear(alpha: Float, val matrix: FloatArray, val spread: Spread, val stops: List<Stop>) : PaintData(alpha)
    /** The unit end circle at the origin of gradient space; the start circle at (fx, fy), radius r0. */
    class Radial(alpha: Float, val matrix: FloatArray, val fx: Float, val fy: Float, val r0: Float, val spread: Spread, val stops: List<Stop>) : PaintData(alpha)
}

class StrokeData(val width: Float, val join: LineJoin, val cap: LineCap, val miterLimit: Float, val dashOffset: Float, val dash: FloatArray)

class FontData(val family: String, val size: Float, val weight: Int, val italic: Boolean)

/** A path: the verb words of the wire format (PathVerb + coordinates), decoded by the painter. */
class PathData(val words: FloatArray)

sealed class DrawCommand {
    class SetTransform(val matrix: FloatArray) : DrawCommand()
    object Save : DrawCommand()
    object Restore : DrawCommand()
    class Clip(val rule: FillRule, val path: PathData) : DrawCommand()
    class FillPath(val rule: FillRule, val paint: PaintData, val path: PathData) : DrawCommand()
    class StrokePath(val stroke: StrokeData, val paint: PaintData, val path: PathData) : DrawCommand()
    class FillText(
        val text: String, val x: Float, val y: Float, val maxWidth: Float, val font: FontData,
        val align: TextAlign, val baseline: TextBaseline, val letterSpacing: Float, val paint: PaintData,
    ) : DrawCommand()
    class StrokeText(
        val text: String, val x: Float, val y: Float, val maxWidth: Float, val font: FontData,
        val align: TextAlign, val baseline: TextBaseline, val letterSpacing: Float, val stroke: StrokeData, val paint: PaintData,
    ) : DrawCommand()
    class DrawImage(
        val surface: Int, val sx: Float, val sy: Float, val sw: Float, val sh: Float,
        val dx: Float, val dy: Float, val dw: Float, val dh: Float, val alpha: Float,
    ) : DrawCommand()
    class ClearRect(val x: Float, val y: Float, val w: Float, val h: Float) : DrawCommand()
}

class DrawListFormatException(message: String) : RuntimeException(message)

/** Walks a draw list. */
class DrawReader(private val list: DrawList) {
    private val w = list.words
    private var pos = 0

    private fun f(): Float { if (pos >= w.size) throw DrawListFormatException("truncated at $pos"); return w[pos++] }
    private fun i(): Int = f().toInt()
    private fun str(): String { val k = i(); if (k < 0 || k >= list.strings.size) throw DrawListFormatException("bad string index $k"); return list.strings[k] }
    private fun color(): FloatArray = floatArrayOf(f(), f(), f(), f())
    private fun matrix(): FloatArray = floatArrayOf(f(), f(), f(), f(), f(), f())

    private fun paint(): PaintData {
        val kind = i(); val alpha = f()
        if (kind == PaintKind.COLOR.id) return PaintData.Solid(alpha, color())
        val m = matrix()
        var fx = 0f; var fy = 0f; var r0 = 0f
        if (kind == PaintKind.RADIAL.id) { fx = f(); fy = f(); r0 = f() }
        val spread = Spread.of(i())
        val n = i()
        val stops = ArrayList<Stop>(n)
        repeat(n) { stops.add(Stop(f(), color())) }
        return if (kind == PaintKind.RADIAL.id) PaintData.Radial(alpha, m, fx, fy, r0, spread, stops) else PaintData.Linear(alpha, m, spread, stops)
    }

    private fun stroke(): StrokeData {
        val width = f(); val join = LineJoin.of(i()); val cap = LineCap.of(i())
        val miter = f(); val offset = f(); val n = i()
        val dash = FloatArray(n) { f() }
        return StrokeData(width, join, cap, miter, offset, dash)
    }

    private fun font(): FontData = FontData(str(), f(), i(), i() != 0)

    private fun path(): PathData {
        val n = i()
        if (n < 0 || pos + n > w.size) throw DrawListFormatException("truncated path at $pos")
        val words = w.copyOfRange(pos, pos + n)
        pos += n
        return PathData(words)
    }

    fun readAll(): List<DrawCommand> {
        val out = ArrayList<DrawCommand>()
        while (pos + 2 <= w.size) {
            val id = i(); val len = i()
            val end = pos + len
            if (len < 0 || end > w.size) throw DrawListFormatException("truncated command $id at $pos")
            when (DrawCmd.of(id)) {
                DrawCmd.SET_TRANSFORM -> out.add(DrawCommand.SetTransform(matrix()))
                DrawCmd.SAVE -> out.add(DrawCommand.Save)
                DrawCmd.RESTORE -> out.add(DrawCommand.Restore)
                DrawCmd.CLIP -> out.add(DrawCommand.Clip(FillRule.of(i()), path()))
                DrawCmd.FILL_PATH -> { val rule = FillRule.of(i()); val p = paint(); out.add(DrawCommand.FillPath(rule, p, path())) }
                DrawCmd.STROKE_PATH -> { val s = stroke(); val p = paint(); out.add(DrawCommand.StrokePath(s, p, path())) }
                DrawCmd.FILL_TEXT -> {
                    val text = str(); val x = f(); val y = f(); val mw = f(); val font = font()
                    val align = TextAlign.of(i()); val baseline = TextBaseline.of(i()); val ls = f()
                    out.add(DrawCommand.FillText(text, x, y, mw, font, align, baseline, ls, paint()))
                }
                DrawCmd.STROKE_TEXT -> {
                    val text = str(); val x = f(); val y = f(); val mw = f(); val font = font()
                    val align = TextAlign.of(i()); val baseline = TextBaseline.of(i()); val ls = f()
                    val s = stroke()
                    out.add(DrawCommand.StrokeText(text, x, y, mw, font, align, baseline, ls, s, paint()))
                }
                DrawCmd.DRAW_IMAGE -> out.add(DrawCommand.DrawImage(i(), f(), f(), f(), f(), f(), f(), f(), f(), f()))
                DrawCmd.CLEAR_RECT -> out.add(DrawCommand.ClearRect(f(), f(), f(), f()))
                null -> {}   // unknown: skipped
            }
            if (pos > end) throw DrawListFormatException("command $id overran its length")
            pos = end
        }
        return out
    }
}
