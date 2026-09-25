package io.letary.anycanvas.demo

import android.app.Activity
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import io.letary.anycanvas.AnyCanvas
import io.letary.anycanvas.DrawList
import io.letary.anycanvas.Painter
import org.json.JSONArray
import org.json.JSONObject

private const val TAG = "AnyCanvasDemo"

/** One golden: its name, the draw list the core produced on this device, and whether it matched the expected JSON. */
private class Golden(val name: String, val list: DrawList, val ok: Boolean, val detail: String)

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val goldens = runGoldens()
        val failed = goldens.filter { !it.ok }
        val summary = "AnyCanvas on-device golden check: ${goldens.size - failed.size} ok, ${failed.size} failed" +
            (if (failed.isEmpty()) "" else " [" + failed.joinToString { it.name } + "]")
        Log.i(TAG, summary)
        for (g in failed) Log.w(TAG, "${g.name}: ${g.detail}")

        val column = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(24, 48, 24, 48) }
        column.addView(TextView(this).apply {
            text = summary
            textSize = 16f
            setTextColor(if (failed.isEmpty()) Color.rgb(0, 120, 0) else Color.RED)
        })
        val painter = Painter()
        val height = (300 * resources.displayMetrics.density).toInt()
        for (g in goldens) {
            column.addView(TextView(this).apply {
                text = (if (g.ok) "✓ " else "✗ ") + g.name
                textSize = 14f
                setTextColor(if (g.ok) Color.DKGRAY else Color.RED)
                setPadding(0, 24, 0, 4)
            })
            column.addView(GoldenView(this, painter, g.list).apply {
                layoutParams = LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, height)
            })
        }
        setContentView(ScrollView(this).apply { addView(column) })
    }

    private fun runGoldens(): List<Golden> {
        val out = ArrayList<Golden>()
        AnyCanvas().use { core ->
            for (file in assets.list("streams").orEmpty().sorted()) {
                val name = "stream-" + file.removeSuffix(".json")
                val json = JSONObject(assets.open("streams/$file").bufferedReader().readText())
                val cmdArr = json.getJSONArray("cmd")
                val cmd = FloatArray(cmdArr.length()) { cmdArr.getDouble(it).toFloat() }
                val refsArr = json.getJSONArray("refs")
                val refs = List(refsArr.length()) { refsArr.getString(it) }
                val scale = json.optDouble("scale", 1.0).toFloat()
                val list = core.interpret(cmd, refs, scale)
                out.add(check(name, list))
            }
            for (file in assets.list("svg").orEmpty().sorted()) {
                if (!file.endsWith(".svg")) continue
                val name = "svg-" + file.removeSuffix(".svg")
                val markup = assets.open("svg/$file").readBytes().toString(Charsets.UTF_8)
                var fitW = 0f; var fitH = 0f; var tint: Int? = null
                val sidecar = "svg/$file.json"
                if (assets.list("svg").orEmpty().contains("$file.json")) {
                    val side = JSONObject(assets.open(sidecar).bufferedReader().readText())
                    side.optJSONArray("fit")?.let { fitW = it.getDouble(0).toFloat(); fitH = it.getDouble(1).toFloat() }
                    side.optString("tint", "").takeIf { it.isNotEmpty() }?.let { tint = cssHexToArgb(it) }
                }
                val svg = core.parseSvg(markup)
                val list = if (svg == null) DrawList(FloatArray(0), emptyArray()) else svg.use { it.draw(fitW, fitH, tint) }
                out.add(check(name, list))
            }
        }
        return out
    }

    private fun check(name: String, list: DrawList): Golden {
        val actual = AnyCanvas.json(list)
        val expected = try { assets.open("expected/$name.json").bufferedReader().readText() } catch (e: Exception) { null }
        if (expected == null) return Golden(name, list, false, "no expected file")
        if (expected == actual) return Golden(name, list, true, "")
        // The goldens were written on the desktop: another libm and FMA contraction move the last
        // digits of float trig, so the device check is structural with a numeric tolerance.
        if (numericallyEqual(JSONArray(expected), JSONArray(actual))) return Golden(name, list, true, "numerically equal")
        var i = 0
        while (i < expected.length && i < actual.length && expected[i] == actual[i]) i++
        val from = maxOf(0, i - 40)
        return Golden(name, list, false, "differs at $i: expected …${expected.substring(from, minOf(expected.length, i + 40))}… actual …${actual.substring(from, minOf(actual.length, i + 40))}…")
    }

    private fun numericallyEqual(a: Any?, b: Any?): Boolean = when {
        a is JSONArray && b is JSONArray -> a.length() == b.length() && (0 until a.length()).all { numericallyEqual(a.get(it), b.get(it)) }
        a is JSONObject && b is JSONObject -> a.length() == b.length() && a.keys().asSequence().all { b.has(it) && numericallyEqual(a.get(it), b.get(it)) }
        a is Number && b is Number -> { val x = a.toDouble(); val y = b.toDouble(); Math.abs(x - y) <= 1e-3 + 1e-4 * Math.max(Math.abs(x), Math.abs(y)) }
        else -> a == b
    }

    /** #rrggbb / #rrggbbaa (CSS order) → ARGB. */
    private fun cssHexToArgb(s: String): Int {
        val hex = s.removePrefix("#")
        val r = hex.substring(0, 2).toInt(16); val g = hex.substring(2, 4).toInt(16); val b = hex.substring(4, 6).toInt(16)
        val a = if (hex.length >= 8) hex.substring(6, 8).toInt(16) else 255
        return Color.argb(a, r, g, b)
    }
}

/** Paints one draw list, scaled so 720 device-px units of the golden fill the view width. */
private class GoldenView(context: Context, private val painter: Painter, private val list: DrawList) : View(context) {
    private val commands = list.commands()
    override fun onDraw(canvas: Canvas) {
        canvas.drawColor(Color.WHITE)
        val s = width / 720f
        canvas.save()
        canvas.scale(s, s)
        painter.paint(canvas, commands)
        canvas.restore()
    }
}
