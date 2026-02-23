use std::env;

fn main() {
    cxx_build::bridge("src/lib.rs").compile("eqsat");
    println!("cargo:rerun-if-changed=src/lib.rs");

    if env::var("TARGET").is_ok_and(|s| s.contains("windows-msvc"))
        && env::var("CFLAGS").is_ok_and(|s| s.contains("/MDd"))
    {
        println!("cargo::rustc-link-arg=/nodefaultlib:msvcrt");
        println!("cargo::rustc-link-arg=/defaultlib:msvcrtd");
    }
}
