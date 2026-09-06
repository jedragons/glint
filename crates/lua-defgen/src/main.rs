//! lua_typegen
//!
//! Scans each `<sources>/<lib>/src` directory (recursively -- bindings are
//! not confined to a single `bindings/` subdirectory, and live at different
//! depths in different libraries) for C++ source files, extracts `/*@ ... */`
//! comment blocks, dedents them, and writes one `<definitions>/<lib>/<relative-path>.lua`
//! file per source file containing all of its extracted blocks (prefixed with
//! `---@meta`). Source files with no such blocks are simply skipped, so it is
//! safe to point this at a whole library's `src/` tree.
//!
//! `<definitions>` is fully regenerated on each run (wiped up front) so
//! definitions left behind by since-renamed or since-removed sources don't
//! linger.
//!
//! This tool does no validation or sync-checking -- it only extracts text.
//!
//! Usage:
//!     lua_typegen [SOURCES_DIR] [DEFINITIONS_DIR]
//!
//! Defaults: SOURCES_DIR=sources, DEFINITIONS_DIR=definitions

use std::env;
use std::fs;
use std::path::{Path, PathBuf};

use anyhow::{Context, Result, bail};
use walkdir::WalkDir;

/// File extensions treated as C++ sources/headers worth scanning.
const CPP_EXTENSIONS: &[&str] = &["cpp", "cc", "cxx", "hpp", "hh", "h"];

/// Marks the start of an extractable block. Must appear alone on its line
/// (whitespace-trimmed).
const BLOCK_OPEN: &str = "/*@";

/// Marks the end of an extractable block. Must appear alone on its line
/// (whitespace-trimmed).
const BLOCK_CLOSE: &str = "*/";

fn main() -> Result<()> {
    let mut args = env::args().skip(1);
    let sources_dir = PathBuf::from(args.next().unwrap_or_else(|| "sources".to_string()));
    let definitions_dir = PathBuf::from(args.next().unwrap_or_else(|| "definitions".to_string()));

    if !sources_dir.is_dir() {
        bail!(
            "sources directory `{}` does not exist",
            sources_dir.display()
        );
    }

    if definitions_dir.is_dir() {
        fs::remove_dir_all(&definitions_dir).with_context(|| {
            format!(
                "clearing stale definitions dir `{}`",
                definitions_dir.display()
            )
        })?;
    }

    for lib_entry in fs::read_dir(&sources_dir)
        .with_context(|| format!("reading sources dir `{}`", sources_dir.display()))?
    {
        let lib_entry = lib_entry?;
        if !lib_entry.file_type()?.is_dir() {
            continue;
        }

        let lib_name = lib_entry.file_name().to_string_lossy().into_owned();
        let src_dir = lib_entry.path().join("src");

        if !src_dir.is_dir() {
            continue;
        }

        process_src_dir(&src_dir, &lib_name, &definitions_dir)?;
    }

    Ok(())
}

/// Walks `src_dir` recursively, extracting blocks from every C++ source
/// file found and writing the corresponding `.lua` definition file.
fn process_src_dir(src_dir: &Path, lib_name: &str, definitions_dir: &Path) -> Result<()> {
    for entry in WalkDir::new(src_dir).into_iter().filter_map(|e| e.ok()) {
        if !entry.file_type().is_file() {
            continue;
        }

        let path = entry.path();
        let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");
        if !CPP_EXTENSIONS.contains(&ext) {
            continue;
        }

        let content =
            fs::read_to_string(path).with_context(|| format!("reading `{}`", path.display()))?;

        let blocks = extract_blocks(&content);
        if blocks.is_empty() {
            continue;
        }

        let relative = path.strip_prefix(src_dir).unwrap_or(path);
        let mut out_path = definitions_dir.join(lib_name).join(relative);
        out_path.set_extension("lua");

        if let Some(parent) = out_path.parent() {
            fs::create_dir_all(parent)
                .with_context(|| format!("creating dir `{}`", parent.display()))?;
        }

        let mut output = String::from("---@meta\n\n");
        output.push_str(&blocks.join("\n\n"));
        output.push('\n');

        fs::write(&out_path, output)
            .with_context(|| format!("writing `{}`", out_path.display()))?;

        println!("wrote {}", out_path.display());
    }

    Ok(())
}

/// Extracts each `/*@ ... */` block (delimiters must sit alone on their own
/// line) from `content`, dedents its contents, and returns the blocks in
/// source order. A block left unterminated at EOF is silently dropped.
fn extract_blocks(content: &str) -> Vec<String> {
    let mut blocks = Vec::new();
    let mut current: Option<Vec<&str>> = None;

    for line in content.lines() {
        let trimmed = line.trim();

        match &mut current {
            None => {
                if trimmed == BLOCK_OPEN {
                    current = Some(Vec::new());
                }
            }
            Some(buf) => {
                if trimmed == BLOCK_CLOSE {
                    blocks.push(dedent(buf));
                    current = None;
                } else {
                    buf.push(line);
                }
            }
        }
    }

    blocks
}

/// Removes the smallest common leading-space indentation shared by all
/// non-blank lines. Blank lines are emitted as empty strings.
fn dedent(lines: &[&str]) -> String {
    let min_indent = lines
        .iter()
        .filter(|l| !l.trim().is_empty())
        .map(|l| l.len() - l.trim_start_matches(' ').len())
        .min()
        .unwrap_or(0);

    lines
        .iter()
        .map(|l| {
            if l.trim().is_empty() {
                ""
            } else if l.len() >= min_indent {
                &l[min_indent..]
            } else {
                l.trim_start()
            }
        })
        .collect::<Vec<_>>()
        .join("\n")
}
