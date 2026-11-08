use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    // 1. Zbuduj bibliotekę D za pomocą dub
    println!("cargo:rerun-if-changed=../luminos-d-backend/source");
    println!("cargo:rerun-if-changed=../luminos-d-backend/dub.json");
    
    let status = Command::new("dub")
        .args(&["build", "--build=release", "--compiler=ldc2"])
        .current_dir("../luminos-d-backend")
        .status()
        .expect("Nie udało się uruchomić 'dub build'. Upewnij się, że 'ldc' jest zainstalowane.");

    if !status.success() {
        panic!("Kompilacja D nie powiodła się!");
    }

    // 2. Poinformuj Rusta, gdzie szukać skompilowanej biblioteki D
    // dub domyślnie umieszcza pliki .a w katalogu projektu przy użyciu --build=release
    let d_lib_path = PathBuf::from("../luminos-d-backend");
    println!("cargo:rustc-link-search=native={}", d_lib_path.display());
    println!("cargo:rustc-link-lib=static=luminos_d_backend");

    // 3. Kompilacja kodu C (Wayland backend)
    cc::Build::new()
        .file("csrc/luminos_gfx.c")
        .include("csrc")
        .flag("-std=c11")
        .compile("luminos_gfx_c");

    println!("cargo:rustc-link-lib=wayland-client");
    println!("cargo:rustc-link-lib=wayland-egl");
    println!("cargo:rustc-link-lib=EGL");
    println!("cargo:rustc-link-lib=GLESv2");

    // 4. Bindgen
    let bindings = bindgen::Builder::default()
        .header("csrc/luminos_gfx.h")
        .allowlist_function("luminos_.*")
        .allowlist_type("LuminosWindow")
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings.write_to_file(out_path.join("bindings.rs")).expect("Couldn't write bindings!");
}
