// The Android painter: replays a draw list on an android.graphics.Canvas. One `when` branch per
// command, exhaustive over the sealed DrawCommand, no state beyond the canvas' own save stack and
// the transform the list set last (Canvas.setMatrix is not absolute on every canvas kind, so the
// painter applies transform DELTAS with concat).
//
// Platform limits: RadialGradient is concentric (the focal point is ignored); everything else in
// the draw list maps one to one (Shader.setLocalMatrix carries the gradient matrix, DashPathEffect
// the dashes, Paint.letterSpacing the letter spacing).
package io.letary.anycanvas

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.LinearGradient
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PorterDuff
import android.graphics.PorterDuffXfermode
import android.graphics.RadialGradient
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.Typeface
import io.letary.anycanvas.spec.FillRule
import io.letary.anycanvas.spec.LineCap
import io.letary.anycanvas.spec.LineJoin
import io.letary.anycanvas.spec.PathVerb
import io.letary.anycanvas.spec.Spread
import io.letary.anycanvas.spec.TextAlign
import io.letary.anycanvas.spec.TextBaseline
import kotlin.math.roundToInt

/** What the painter needs from its host. The defaults make it usable standalone. */
interface PainterHooks {
    /** The typeface for a resolved font (a registered font by family first, then the system). */
    fun typeface(font: FontData): Typeface {
        val style = (if (font.weight >= 600) Typeface.BOLD else 0) or (if (font.italic) Typeface.ITALIC else 0)
        return Typeface.create(font.family, style)
    }

    /** The bitmap of a surface id, for DRAW_IMAGE; null skips the blit. */
    fun image(surface: Int): Bitmap? = null
}

class Painter(private val hooks: PainterHooks = object : PainterHooks {}) {
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val strokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }
    private val clearPaint = Paint().apply { xfermode = PorterDuffXfermode(PorterDuff.Mode.CLEAR) }
    private val imagePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { isFilterBitmap = true }
    private val path = Path()
    private val current = Matrix()
    private val target = Matrix()
    private val delta = Matrix()
    private val stack = ArrayDeque<Matrix>()
    private val srcRect = Rect()
    private val dstRect = RectF()

    /** Replay `list` on `canvas`. The canvas' current transform is the base; clearing is the caller's. */
    fun paint(canvas: Canvas, list: DrawList) = paint(canvas, list.commands())

    fun paint(canvas: Canvas, commands: List<DrawCommand>) {
        current.reset()
        stack.clear()
        val base = canvas.save()
        for (c in commands) {
            when (c) {
                is DrawCommand.SetTransform -> setTransform(canvas, c.matrix)
                DrawCommand.Save -> { canvas.save(); stack.addLast(Matrix(current)) }
                DrawCommand.Restore -> if (stack.isNotEmpty()) { canvas.restore(); current.set(stack.removeLast()) }
                is DrawCommand.Clip -> { buildPath(c.path, c.rule); canvas.clipPath(path) }
                is DrawCommand.FillPath -> { buildPath(c.path, c.rule); applyPaint(fillPaint, c.paint); canvas.drawPath(path, fillPaint) }
                is DrawCommand.StrokePath -> { buildPath(c.path, FillRule.NONZERO); applyStroke(strokePaint, c.stroke); applyPaint(strokePaint, c.paint); canvas.drawPath(path, strokePaint) }
                is DrawCommand.FillText -> { applyPaint(fillPaint, c.paint); drawText(canvas, fillPaint, c.text, c.x, c.y, c.maxWidth, c.font, c.align, c.baseline, c.letterSpacing) }
                is DrawCommand.StrokeText -> { applyStroke(strokePaint, c.stroke); applyPaint(strokePaint, c.paint); drawText(canvas, strokePaint, c.text, c.x, c.y, c.maxWidth, c.font, c.align, c.baseline, c.letterSpacing) }
                is DrawCommand.DrawImage -> {
                    val bmp = hooks.image(c.surface) ?: continue
                    imagePaint.alpha = (c.alpha.coerceIn(0f, 1f) * 255f).roundToInt()
                    srcRect.set(c.sx.toInt(), c.sy.toInt(), (c.sx + c.sw).toInt(), (c.sy + c.sh).toInt())
                    dstRect.set(c.dx, c.dy, c.dx + c.dw, c.dy + c.dh)
                    canvas.drawBitmap(bmp, srcRect, dstRect, imagePaint)
                }
                is DrawCommand.ClearRect -> canvas.drawRect(c.x, c.y, c.x + c.w, c.y + c.h, clearPaint)
            }
        }
        canvas.restoreToCount(base)
        // Paint objects must not keep a shader or effect alive between replays.
        fillPaint.shader = null; strokePaint.shader = null; strokePaint.pathEffect = null
    }

    private fun setTransform(canvas: Canvas, m: FloatArray) {
        target.setValues(floatArrayOf(m[0], m[2], m[4], m[1], m[3], m[5], 0f, 0f, 1f))
        if (current.invert(delta)) {
            delta.postConcat(target)
            canvas.concat(delta)
        } else {
            canvas.setMatrix(target)
        }
        current.set(target)
    }

    private fun buildPath(p: PathData, rule: FillRule) {
        path.rewind()
        path.fillType = if (rule == FillRule.EVENODD) Path.FillType.EVEN_ODD else Path.FillType.WINDING
        val w = p.words
        var i = 0
        while (i < w.size) {
            when (w[i++].toInt()) {
                PathVerb.MOVE.id -> { path.moveTo(w[i], w[i + 1]); i += 2 }
                PathVerb.LINE.id -> { path.lineTo(w[i], w[i + 1]); i += 2 }
                PathVerb.QUAD.id -> { path.quadTo(w[i], w[i + 1], w[i + 2], w[i + 3]); i += 4 }
                PathVerb.CUBIC.id -> { path.cubicTo(w[i], w[i + 1], w[i + 2], w[i + 3], w[i + 4], w[i + 5]); i += 6 }
                PathVerb.CLOSE.id -> path.close()
                else -> return
            }
        }
    }

    private fun argb(c: FloatArray, alpha: Float): Int = Color.argb(
        (c[3] * alpha * 255f).roundToInt().coerceIn(0, 255),
        (c[0] * 255f).roundToInt().coerceIn(0, 255),
        (c[1] * 255f).roundToInt().coerceIn(0, 255),
        (c[2] * 255f).roundToInt().coerceIn(0, 255),
    )

    private fun tileMode(s: Spread) = when (s) { Spread.PAD -> Shader.TileMode.CLAMP; Spread.REFLECT -> Shader.TileMode.MIRROR; Spread.REPEAT -> Shader.TileMode.REPEAT }

    private fun localMatrix(m: FloatArray): Matrix = Matrix().apply { setValues(floatArrayOf(m[0], m[2], m[4], m[1], m[3], m[5], 0f, 0f, 1f)) }

    private fun applyPaint(paint: Paint, p: PaintData) {
        paint.shader = null
        when (p) {
            is PaintData.Solid -> paint.color = argb(p.color, p.alpha)
            is PaintData.Linear -> {
                val colors = IntArray(p.stops.size) { argb(p.stops[it].color, 1f) }
                val positions = FloatArray(p.stops.size) { p.stops[it].offset }
                paint.shader = LinearGradient(0f, 0f, 1f, 0f, colors, positions, tileMode(p.spread)).apply { setLocalMatrix(localMatrix(p.matrix)) }
                paint.color = Color.WHITE
                paint.alpha = (p.alpha * 255f).roundToInt().coerceIn(0, 255)
            }
            is PaintData.Radial -> {
                // The start radius r0: the stops move onto [r0, 1] (Skia pads [0, r0) with the first
                // color) — exact for concentric circles.
                val r0 = if (p.r0 > 0f && p.r0 < 1f) p.r0 else 0f
                val colors = IntArray(p.stops.size) { argb(p.stops[it].color, 1f) }
                val positions = FloatArray(p.stops.size) { r0 + p.stops[it].offset * (1f - r0) }
                paint.shader = RadialGradient(0f, 0f, 1f, colors, positions, tileMode(p.spread)).apply { setLocalMatrix(localMatrix(p.matrix)) }
                paint.color = Color.WHITE
                paint.alpha = (p.alpha * 255f).roundToInt().coerceIn(0, 255)
            }
        }
    }

    private fun applyStroke(paint: Paint, s: StrokeData) {
        paint.strokeWidth = s.width
        paint.strokeJoin = when (s.join) { LineJoin.MITER -> Paint.Join.MITER; LineJoin.ROUND -> Paint.Join.ROUND; LineJoin.BEVEL -> Paint.Join.BEVEL }
        paint.strokeCap = when (s.cap) { LineCap.BUTT -> Paint.Cap.BUTT; LineCap.ROUND -> Paint.Cap.ROUND; LineCap.SQUARE -> Paint.Cap.SQUARE }
        paint.strokeMiter = s.miterLimit
        paint.pathEffect = if (s.dash.isEmpty()) null else DashPathEffect(s.dash, s.dashOffset)
    }

    private fun drawText(canvas: Canvas, paint: Paint, text: String, x: Float, y: Float, maxWidth: Float, font: FontData, align: TextAlign, baseline: TextBaseline, letterSpacing: Float) {
        if (text.isEmpty()) return
        paint.textSize = font.size
        paint.typeface = hooks.typeface(font)
        paint.letterSpacing = if (font.size > 0f) letterSpacing / font.size else 0f
        paint.textAlign = Paint.Align.LEFT
        val width = paint.measureText(text)
        val ax = when (align) {
            TextAlign.CENTER -> x - width / 2f
            TextAlign.RIGHT, TextAlign.END -> x - width
            TextAlign.LEFT, TextAlign.START -> x
        }
        val m = paint.fontMetrics
        val by = when (baseline) {
            TextBaseline.TOP -> y - m.ascent
            TextBaseline.MIDDLE -> y - (m.ascent + m.descent) / 2f
            TextBaseline.BOTTOM, TextBaseline.IDEOGRAPHIC -> y - m.descent
            TextBaseline.HANGING -> y - m.ascent * 0.8f
            TextBaseline.ALPHABETIC -> y
        }
        val condense = maxWidth > 0f && width > maxWidth
        if (condense) { canvas.save(); canvas.translate(ax, 0f); canvas.scale(maxWidth / width, 1f); canvas.translate(-ax, 0f) }
        canvas.drawText(text, ax, by, paint)
        if (condense) canvas.restore()
        paint.letterSpacing = 0f
    }
}
