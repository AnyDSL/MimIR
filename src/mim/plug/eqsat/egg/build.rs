fn main() {
    cxx_build::bridge("src/lib.rs").compile("mimffi");

    println!("cargo:rerun-if-changed=src/lib.rs");
}
