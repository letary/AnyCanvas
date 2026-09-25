//! The CSS color parser of AnyCanvas — the Rust twin of `core/include/anycanvas/css_color.h`: the
//! same grammar, the same name table, the same float32 operations in the header's order, so both
//! give the same bits (`tests/golden/colors` holds the three twins equal; the third is the
//! TypeScript one in `recorders/ts/src/cssColor.ts`). No dependencies, `no_std`, no heap: a build
//! tool that folds color literals at compile time (LeCodes' bundler) gets exactly the floats the
//! runtime would compute.
//!
//! Grammar — case-insensitive, surrounding ASCII whitespace ignored:
//!   `#rgb #rgba #rrggbb #rrggbbaa` · `rgb()` / `rgba()` (three or four arguments separated by
//!   commas, spaces or "/": numbers 0..255 or percentages, the fourth the alpha 0..1 or a
//!   percentage) · `hsl()` / `hsla()` (the hue in degrees, bare or deg / rad / grad / turn,
//!   saturation and lightness as percentages with or without the "%", the same alpha) · the 148
//!   CSS Color 4 names · `transparent` · `clear` (the ONE non-CSS alias, = transparent).
//! Numbers are CSS number tokens (`nan`, `inf`, `0x..` reject), a unit other than deg / rad / grad /
//! turn rejects, a non-finite value rejects, out-of-range values clamp to 0..1. Anything else is
//! not a color. A string stops at an embedded NUL, like the C string the header reads.
//!
//! Floats are straight (non-premultiplied) float32 0..1. Rust never contracts `a * b + c` into an
//! FMA on its own, and every `f32` operation rounds once, so the header's split statements are
//! plain expressions here — in the header's order.

#![no_std]

use core::cmp::Ordering;

/// A color: straight float32 RGBA, 0..1.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Rgba {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}

/// The CSS Color Module 4 named colors (148) as `(name, 0xRRGGBB)`, sorted by name. `transparent`
/// and the alias `clear` are keywords of [`parse`], not entries: they carry an alpha.
pub const NAMED_COLORS: [(&str, u32); 148] = [
    ("aliceblue", 0xf0f8ff), ("antiquewhite", 0xfaebd7), ("aqua", 0x00ffff), ("aquamarine", 0x7fffd4),
    ("azure", 0xf0ffff), ("beige", 0xf5f5dc), ("bisque", 0xffe4c4), ("black", 0x000000),
    ("blanchedalmond", 0xffebcd), ("blue", 0x0000ff), ("blueviolet", 0x8a2be2), ("brown", 0xa52a2a),
    ("burlywood", 0xdeb887), ("cadetblue", 0x5f9ea0), ("chartreuse", 0x7fff00), ("chocolate", 0xd2691e),
    ("coral", 0xff7f50), ("cornflowerblue", 0x6495ed), ("cornsilk", 0xfff8dc), ("crimson", 0xdc143c),
    ("cyan", 0x00ffff), ("darkblue", 0x00008b), ("darkcyan", 0x008b8b), ("darkgoldenrod", 0xb8860b),
    ("darkgray", 0xa9a9a9), ("darkgreen", 0x006400), ("darkgrey", 0xa9a9a9), ("darkkhaki", 0xbdb76b),
    ("darkmagenta", 0x8b008b), ("darkolivegreen", 0x556b2f), ("darkorange", 0xff8c00), ("darkorchid", 0x9932cc),
    ("darkred", 0x8b0000), ("darksalmon", 0xe9967a), ("darkseagreen", 0x8fbc8f), ("darkslateblue", 0x483d8b),
    ("darkslategray", 0x2f4f4f), ("darkslategrey", 0x2f4f4f), ("darkturquoise", 0x00ced1), ("darkviolet", 0x9400d3),
    ("deeppink", 0xff1493), ("deepskyblue", 0x00bfff), ("dimgray", 0x696969), ("dimgrey", 0x696969),
    ("dodgerblue", 0x1e90ff), ("firebrick", 0xb22222), ("floralwhite", 0xfffaf0), ("forestgreen", 0x228b22),
    ("fuchsia", 0xff00ff), ("gainsboro", 0xdcdcdc), ("ghostwhite", 0xf8f8ff), ("gold", 0xffd700),
    ("goldenrod", 0xdaa520), ("gray", 0x808080), ("green", 0x008000), ("greenyellow", 0xadff2f),
    ("grey", 0x808080), ("honeydew", 0xf0fff0), ("hotpink", 0xff69b4), ("indianred", 0xcd5c5c),
    ("indigo", 0x4b0082), ("ivory", 0xfffff0), ("khaki", 0xf0e68c), ("lavender", 0xe6e6fa),
    ("lavenderblush", 0xfff0f5), ("lawngreen", 0x7cfc00), ("lemonchiffon", 0xfffacd), ("lightblue", 0xadd8e6),
    ("lightcoral", 0xf08080), ("lightcyan", 0xe0ffff), ("lightgoldenrodyellow", 0xfafad2), ("lightgray", 0xd3d3d3),
    ("lightgreen", 0x90ee90), ("lightgrey", 0xd3d3d3), ("lightpink", 0xffb6c1), ("lightsalmon", 0xffa07a),
    ("lightseagreen", 0x20b2aa), ("lightskyblue", 0x87cefa), ("lightslategray", 0x778899), ("lightslategrey", 0x778899),
    ("lightsteelblue", 0xb0c4de), ("lightyellow", 0xffffe0), ("lime", 0x00ff00), ("limegreen", 0x32cd32),
    ("linen", 0xfaf0e6), ("magenta", 0xff00ff), ("maroon", 0x800000), ("mediumaquamarine", 0x66cdaa),
    ("mediumblue", 0x0000cd), ("mediumorchid", 0xba55d3), ("mediumpurple", 0x9370db), ("mediumseagreen", 0x3cb371),
    ("mediumslateblue", 0x7b68ee), ("mediumspringgreen", 0x00fa9a), ("mediumturquoise", 0x48d1cc), ("mediumvioletred", 0xc71585),
    ("midnightblue", 0x191970), ("mintcream", 0xf5fffa), ("mistyrose", 0xffe4e1), ("moccasin", 0xffe4b5),
    ("navajowhite", 0xffdead), ("navy", 0x000080), ("oldlace", 0xfdf5e6), ("olive", 0x808000),
    ("olivedrab", 0x6b8e23), ("orange", 0xffa500), ("orangered", 0xff4500), ("orchid", 0xda70d6),
    ("palegoldenrod", 0xeee8aa), ("palegreen", 0x98fb98), ("paleturquoise", 0xafeeee), ("palevioletred", 0xdb7093),
    ("papayawhip", 0xffefd5), ("peachpuff", 0xffdab9), ("peru", 0xcd853f), ("pink", 0xffc0cb),
    ("plum", 0xdda0dd), ("powderblue", 0xb0e0e6), ("purple", 0x800080), ("rebeccapurple", 0x663399),
    ("red", 0xff0000), ("rosybrown", 0xbc8f8f), ("royalblue", 0x4169e1), ("saddlebrown", 0x8b4513),
    ("salmon", 0xfa8072), ("sandybrown", 0xf4a460), ("seagreen", 0x2e8b57), ("seashell", 0xfff5ee),
    ("sienna", 0xa0522d), ("silver", 0xc0c0c0), ("skyblue", 0x87ceeb), ("slateblue", 0x6a5acd),
    ("slategray", 0x708090), ("slategrey", 0x708090), ("snow", 0xfffafa), ("springgreen", 0x00ff7f),
    ("steelblue", 0x4682b4), ("tan", 0xd2b48c), ("teal", 0x008080), ("thistle", 0xd8bfd8),
    ("tomato", 0xff6347), ("turquoise", 0x40e0d0), ("violet", 0xee82ee), ("wheat", 0xf5deb3),
    ("white", 0xffffff), ("whitesmoke", 0xf5f5f5), ("yellow", 0xffff00), ("yellowgreen", 0x9acd32),
];

// ---- the header's detail namespace ------------------------------------------------------------

fn is_space(c: u8) -> bool {
    c == b' ' || (9..=13).contains(&c)
}
fn is_digit(c: u8) -> bool {
    c.is_ascii_digit()
}
fn lower(c: u8) -> u8 {
    if c.is_ascii_uppercase() { c - b'A' + b'a' } else { c }
}
fn is_letter(c: u8) -> bool {
    lower(c).is_ascii_lowercase()
}
fn hex_nibble(c: u8) -> Option<u32> {
    match lower(c) {
        l @ b'0'..=b'9' => Some((l - b'0') as u32),
        l @ b'a'..=b'f' => Some((l - b'a' + 10) as u32),
        _ => None,
    }
}

/// `s` against a lower-case ASCII word, ignoring the case of `s`: like strcmp.
fn compare_lower(s: &[u8], word: &str) -> Ordering {
    let w = word.as_bytes();
    for (i, &c) in s.iter().enumerate() {
        let Some(&wc) = w.get(i) else { return Ordering::Greater };
        let a = lower(c);
        if a != wc {
            return a.cmp(&wc);
        }
    }
    if w.len() == s.len() { Ordering::Equal } else { Ordering::Less }
}
fn equals_lower(s: &[u8], word: &str) -> bool {
    compare_lower(s, word) == Ordering::Equal
}

/// The length of the CSS number token at the start of `s`: `[+-]? (digits (. digits*)? | . digits)
/// ([eE] [+-]? digits)?`; 0 when there is none. An "e" without digits after it is not part of the
/// token.
fn number_length(s: &[u8]) -> usize {
    let n = s.len();
    let mut i = 0;
    if i < n && (s[i] == b'+' || s[i] == b'-') {
        i += 1;
    }
    let (mut int_digits, mut frac_digits) = (0, 0);
    while i < n && is_digit(s[i]) {
        i += 1;
        int_digits += 1;
    }
    if i < n && s[i] == b'.' {
        let mut j = i + 1;
        while j < n && is_digit(s[j]) {
            j += 1;
            frac_digits += 1;
        }
        if int_digits > 0 || frac_digits > 0 {
            i = j;
        }
    }
    if int_digits == 0 && frac_digits == 0 {
        return 0;
    }
    if i < n && (s[i] == b'e' || s[i] == b'E') {
        let mut j = i + 1;
        if j < n && (s[j] == b'+' || s[j] == b'-') {
            j += 1;
        }
        if j < n && is_digit(s[j]) {
            while j < n && is_digit(s[j]) {
                j += 1;
            }
            i = j;
        }
    }
    i
}

#[derive(Clone, Copy, Default)]
struct Arg {
    v: f32,
    percent: bool,
}

/// The arguments of a functional notation, `s` between the parentheses: "1, 2, 3" / "1 2 3 / 0.5"
/// → 3 or 4 numbers with a % flag each. Separators (spaces, commas, slashes) are free-form. A
/// number is converted from the validated token (the header bounds it to a 64-byte buffer: a longer
/// token rejects here too).
fn parse_args(s: &[u8]) -> Option<([Arg; 4], usize)> {
    let n = s.len();
    let mut out = [Arg::default(); 4];
    let mut count = 0;
    let mut i = 0;
    while i < n {
        while i < n && (is_space(s[i]) || s[i] == b',' || s[i] == b'/') {
            i += 1;
        }
        if i >= n {
            break;
        }
        let len = number_length(&s[i..]);
        if len == 0 || len >= 64 {
            return None;
        }
        // The token is ASCII by construction; the parse is correctly rounded, like strtof, and an
        // overflow reads as an infinity, rejected below.
        let mut v: f32 = core::str::from_utf8(&s[i..i + len]).ok()?.parse().ok()?;
        i += len;
        let mut percent = false;
        if i < n && s[i] == b'%' {
            percent = true;
            i += 1;
        } else if i < n && is_letter(s[i]) {
            // an angle unit (on any argument, as it always was)
            let u = i;
            while i < n && is_letter(s[i]) {
                i += 1;
            }
            let unit = &s[u..i];
            if equals_lower(unit, "deg") {
                // degrees: the number as it is
            } else if equals_lower(unit, "rad") {
                v = v * 180.0;
                v = v / 3.14159265_f32;
            } else if equals_lower(unit, "turn") {
                v = v * 360.0;
            } else if equals_lower(unit, "grad") {
                v = v * 0.9_f32;
            } else {
                return None;
            }
        }
        if !v.is_finite() {
            return None;
        }
        if count == 4 {
            return None;
        }
        out[count] = Arg { v, percent };
        count += 1;
    }
    if count >= 3 { Some((out, count)) } else { None }
}

fn clamp01(v: f32) -> f32 {
    if v < 0.0 { 0.0 } else if v > 1.0 { 1.0 } else { v }
}

fn hue_to_rgb(p: f32, q: f32, mut t: f32) -> f32 {
    if t < 0.0 {
        t += 1.0;
    }
    if t > 1.0 {
        t -= 1.0;
    }
    let d = q - p;
    if t < 1.0_f32 / 6.0 {
        let mut m = d * 6.0;
        m = m * t;
        return p + m;
    }
    if t < 0.5 {
        return q;
    }
    if t < 2.0_f32 / 3.0 {
        let u = 2.0_f32 / 3.0 - t;
        let mut m = d * u;
        m = m * 6.0;
        return p + m;
    }
    p
}

fn from_bytes(r: u32, g: u32, b: u32, a: u32) -> Rgba {
    Rgba { r: r as f32 / 255.0, g: g as f32 / 255.0, b: b as f32 / 255.0, a: a as f32 / 255.0 }
}

// ---- the API ----------------------------------------------------------------------------------

/// A CSS color → straight float32 RGBA, or `None` when `s` is not a color.
pub fn parse(s: &str) -> Option<Rgba> {
    let s = s.as_bytes();
    // The header reads a C string: it ends at the first NUL.
    let s = match s.iter().position(|&c| c == 0) {
        Some(nul) => &s[..nul],
        None => s,
    };
    let (mut b, mut e) = (0, s.len());
    while b < e && is_space(s[b]) {
        b += 1;
    }
    while e > b && is_space(s[e - 1]) {
        e -= 1;
    }
    let s = &s[b..e];
    let n = s.len();
    if n == 0 {
        return None;
    }

    if s[0] == b'#' {
        let digits = n - 1;
        if digits != 3 && digits != 4 && digits != 6 && digits != 8 {
            return None;
        }
        let mut v = [0u32; 8];
        for i in 0..digits {
            v[i] = hex_nibble(s[i + 1])?;
        }
        return Some(if digits <= 4 {
            from_bytes(v[0] * 17, v[1] * 17, v[2] * 17, if digits == 4 { v[3] * 17 } else { 255 })
        } else {
            from_bytes(v[0] * 16 + v[1], v[2] * 16 + v[3], v[4] * 16 + v[5], if digits == 8 { v[6] * 16 + v[7] } else { 255 })
        });
    }

    if let Some(paren) = s.iter().position(|&c| c == b'(') {
        if s[n - 1] == b')' {
            let name = &s[..paren];
            let (args, count) = parse_args(&s[paren + 1..n - 1])?;
            let alpha = if count == 4 { clamp01(if args[3].percent { args[3].v / 100.0 } else { args[3].v }) } else { 1.0 };
            if equals_lower(name, "rgb") || equals_lower(name, "rgba") {
                let ch = |a: Arg| clamp01(if a.percent { a.v / 100.0 } else { a.v / 255.0 });
                return Some(Rgba { r: ch(args[0]), g: ch(args[1]), b: ch(args[2]), a: alpha });
            }
            if equals_lower(name, "hsl") || equals_lower(name, "hsla") {
                let mut h = args[0].v % 360.0;
                if h < 0.0 {
                    h += 360.0;
                }
                h /= 360.0;
                let sat = clamp01(args[1].v / 100.0);
                let l = clamp01(args[2].v / 100.0);
                if sat <= 0.0 {
                    return Some(Rgba { r: l, g: l, b: l, a: alpha });
                }
                let q = if l < 0.5 {
                    let k = 1.0 + sat;
                    l * k
                } else {
                    let sum = l + sat;
                    let prod = l * sat;
                    sum - prod
                };
                let l2 = 2.0 * l;
                let p = l2 - q;
                return Some(Rgba {
                    r: clamp01(hue_to_rgb(p, q, h + 1.0_f32 / 3.0)),
                    g: clamp01(hue_to_rgb(p, q, h)),
                    b: clamp01(hue_to_rgb(p, q, h - 1.0_f32 / 3.0)),
                    a: alpha,
                });
            }
            return None;
        }
    }

    // A name: looked up only when it can be one ('lightgoldenrodyellow' is the longest, 20).
    if n > 20 {
        return None;
    }
    if equals_lower(s, "transparent") || equals_lower(s, "clear") {
        return Some(from_bytes(0, 0, 0, 0));
    }
    let (mut lo, mut hi) = (0, NAMED_COLORS.len());
    while lo < hi {
        let mid = lo + (hi - lo) / 2;
        match compare_lower(s, NAMED_COLORS[mid].0) {
            Ordering::Equal => {
                let rgb = NAMED_COLORS[mid].1;
                return Some(from_bytes((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255));
            }
            Ordering::Less => hi = mid,
            Ordering::Greater => lo = mid + 1,
        }
    }
    None
}

/// A float channel → a byte: clamp (NaN → 0), then round half up in float32 (k/255 → k, 0.5 → 128).
pub fn to_byte(v: f32) -> u32 {
    if !(v > 0.0) {
        return 0;
    }
    if v >= 1.0 {
        return 255;
    }
    let s = v * 255.0;
    (s + 0.5) as u32
}

/// 0xRRGGBBAA — the one wire layout of the project.
pub fn to_rgba8(c: Rgba) -> u32 {
    (to_byte(c.r) << 24) | (to_byte(c.g) << 16) | (to_byte(c.b) << 8) | to_byte(c.a)
}

/// A CSS color → 0xRRGGBBAA, or `None` when `s` is not a color.
pub fn parse_rgba8(s: &str) -> Option<u32> {
    parse(s).map(to_rgba8)
}
