// swift-tools-version:5.9
// AnyCanvas for Apple platforms. The manifest lives at the repo root because a SwiftPM target's path
// must be inside the package and the core (core/) and the generated spec (spec/gen) are shared with
// every other painter. What is Apple-specific lives in painters/apple.
//
// Products:
//   AnyCanvasPainter  the Swift draw-list reader + the CoreGraphics / CoreText painter. Swift only,
//                     no C dependency — for a host that links the core ITSELF (LeCodes links it once,
//                     inside its runtime) and hands the painter the words the core produced.
//   AnyCanvas         the core (C++17, target CAnyCanvas) + its Swift binding (interpret, SVG, encode)
//                     + the painter: the standalone library.
//   acpaint           (executable, macOS) a draw list or SVG → PNG through the painter.
//
// The painter is CoreGraphics + CoreText only (no UIKit / AppKit), so the package builds and its tests
// run on macOS as well as iOS: `swift test` on a Mac is the fast check, the iPhone simulator through
// `xcodebuild test -scheme AnyCanvas -destination 'platform=iOS Simulator,name=iPhone 17'` the real one.
import PackageDescription

let package = Package(
    name: "AnyCanvas",
    platforms: [.iOS(.v15), .macOS(.v12)],
    products: [
        .library(name: "AnyCanvasPainter", targets: ["AnyCanvasPainter"]),
        .library(name: "AnyCanvas", targets: ["AnyCanvas"]),
    ],
    targets: [
        // The generated enums (bun spec/generate.ts → spec/gen/Spec.swift), shared with Kotlin and TS.
        .target(
            name: "AnyCanvasSpec",
            path: "spec/gen",
            exclude: ["Spec.kt", "spec.ts"],
            sources: ["Spec.swift"]
        ),
        // The platform-free core. core/src includes the spec headers by relative path and the vendored
        // nanosvg / stb through the vendor search path; core/include/module.modulemap exposes only the
        // C API to Swift.
        .target(
            name: "CAnyCanvas",
            path: "core",
            sources: ["src"],
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("src"),
                .headerSearchPath("vendor"),
            ]
        ),
        .target(
            name: "AnyCanvasPainter",
            dependencies: ["AnyCanvasSpec"],
            path: "painters/apple/Sources/AnyCanvasPainter",
            linkerSettings: [
                .linkedFramework("CoreGraphics"),
                .linkedFramework("CoreText"),
            ]
        ),
        // The binding of the C API + the painter re-exported: `import AnyCanvas` is the whole library.
        .target(
            name: "AnyCanvas",
            dependencies: ["CAnyCanvas", "AnyCanvasPainter"],
            path: "painters/apple/Sources/AnyCanvas"
        ),
        // acpaint: a draw list → PNG through the painter, from the command line (macOS; the README images).
        .executableTarget(
            name: "acpaint",
            dependencies: ["AnyCanvas"],
            path: "painters/apple/Sources/acpaint"
        ),
        .testTarget(
            name: "AnyCanvasTests",
            dependencies: ["AnyCanvas"],
            path: "painters/apple/Tests/AnyCanvasTests"
        ),
    ],
    cxxLanguageStandard: .cxx17
)
