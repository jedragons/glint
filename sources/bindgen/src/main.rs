#![allow(dead_code)]

// TODO: Extract pointer information from struct fields
// TODO: Extract pointer information from function parameters
// TODO: Handle header annotations
// TODO: Handle struct annotatios
// TODO: Handle field annotatios
// TODO: Handle function annotatios
// TODO: Handle parameter annotatios
// TODO: ECS annotations
// TODO: Generate Luau bindings for functions
// TODO: Generate Luau bindings for structs
// TODO: Generate Luau bindings for ECS
// TODO: Generate wasm3 bindings for functions
// TODO: Generate wasm3 bindings for structs
// TODO: Generate wasm3 bindings for ECS
// TODO: Generate wasm2c bindings for functions
// TODO: Generate wasm2c bindings for structs
// TODO: Generate wasm2c bindings for ECS
// TODO: Generate QuickJS bindings for functions
// TODO: Generate QuickJS bindings for structs
// TODO: Generate QuickJS bindings for ECS

use crate::header::Header;

mod annotation;
mod header;

fn main() {
    let path = std::env::args().nth(1).expect("Must provide argument");
    let source = std::fs::read_to_string(&path).expect("Could not read");
    let header = Header::parse(&path, &source);
    println!("{header:#?}");
}
