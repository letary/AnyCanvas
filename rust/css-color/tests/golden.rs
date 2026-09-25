//! The Rust twin against the C++ header: tests/golden/colors/expected.json is what
//! anycanvas/css_color.h makes of the corpus and of every name (written by `anycanvas-golden
//! --update`); this crate must produce the same bytes and the same float32 bits, and know the same
//! names. The TS twin runs the same check in tests/ts/cssColor.test.ts.

use std::path::Path;

use anycanvas_css_color::{parse, to_byte, to_rgba8, NAMED_COLORS};
use serde::Deserialize;

#[derive(Deserialize)]
struct Case {
    #[serde(rename = "in")]
    input: String,
    rgba8: Option<String>,
    rgba: Option<[f64; 4]>,
}

#[derive(Deserialize)]
struct Golden {
    names: Vec<String>,
    cases: Vec<Case>,
}

fn golden() -> Golden {
    let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../tests/golden/colors/expected.json");
    let text = std::fs::read_to_string(&path).unwrap_or_else(|e| panic!("{}: {e}", path.display()));
    serde_json::from_str(&text).expect("expected.json")
}

/// JSON carries the shortest decimal that reads back to the float32; -0 folds to 0 (the dump folds it).
fn bits(v: [f32; 4]) -> [u32; 4] {
    v.map(|x| if x == 0.0 { 0 } else { x.to_bits() })
}

fn hex8(v: u32) -> String {
    format!("#{v:08x}")
}

#[test]
fn the_golden_covers_the_corpus_and_every_name_in_both_cases() {
    let g = golden();
    assert!(g.cases.len() > g.names.len() * 2);
}

#[test]
fn every_case_the_same_bytes_and_the_same_float32_bits() {
    let g = golden();
    let mut mismatches = Vec::new();
    for c in &g.cases {
        let got = parse(&c.input);
        let got_hex = got.map(|v| hex8(to_rgba8(v)));
        let got_bits = got.map(|v| bits([v.r, v.g, v.b, v.a]));
        let want_bits = c.rgba.map(|v| bits(v.map(|x| x as f32)));
        if got_hex != c.rgba8 || got_bits != want_bits {
            mismatches.push(format!("{:?}: Rust {got_hex:?} {got_bits:?}, C++ {:?} {want_bits:?}", c.input, c.rgba8));
        }
    }
    assert!(mismatches.is_empty(), "{}", mismatches.join("\n"));
}

#[test]
fn the_same_name_table() {
    let g = golden();
    // The golden lists the keywords with the names.
    let mut ours: Vec<&str> = NAMED_COLORS.iter().map(|(n, _)| *n).chain(["transparent", "clear"]).collect();
    let mut theirs: Vec<&str> = g.names.iter().map(String::as_str).collect();
    ours.sort_unstable();
    theirs.sort_unstable();
    assert_eq!(ours, theirs);
    // The table is binary-searched: it must be sorted as the header sorts it.
    assert!(NAMED_COLORS.windows(2).all(|w| w[0].0 < w[1].0), "NAMED_COLORS is not sorted");
    for (name, rgb) in NAMED_COLORS {
        assert_eq!(parse(name).map(to_rgba8), Some((rgb << 8) | 0xff), "{name}");
    }
}

#[test]
fn a_string_stops_at_an_embedded_nul_like_the_c_string() {
    assert_eq!(parse("red\0junk"), parse("red"));
    assert_eq!(parse("\0red"), None);
}

#[test]
fn bytes_float32_clamp_round_half_up() {
    assert_eq!(to_byte(0.5), 128);
    assert_eq!(to_byte(0.8), 204);
    assert_eq!(to_byte(0.3), 77);
    assert_eq!(to_byte(f32::NAN), 0);
    assert_eq!(to_byte(-1.0), 0);
    assert_eq!(to_byte(2.0), 255);
    for k in 0..=255u32 {
        assert_eq!(to_byte(k as f32 / 255.0), k);
    }
}
