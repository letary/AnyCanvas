# painters/apple — not started

The Swift painter over CoreGraphics (iOS + macOS) as an SPM package: a `CNanyCanvas` target wrapping
`core/` (the C API of `core/include/anycanvas/anycanvas.h`), a Swift draw-list reader over the word
buffer, and a painter that switches exhaustively over `DrawCmd` of `spec/gen/Spec.swift`. It is
written on the Mac (LeCodes phase 6), never on Windows.

What it inherits from NanoSVGKit, the earlier iOS library: nothing as code. The gradient reading
there (`xform` as gradient → user with the axis on `(a, b)`) was a misreading of nanosvg's inverse
transform; the core resolves gradients now, so the painter only maps `matrix`, `fx fy r0` and the
stops onto `CGGradient` + a concatenated CTM inside a clip, exactly like `painters/web`.
