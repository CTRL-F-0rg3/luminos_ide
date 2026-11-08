#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

use std::ffi::CString;

pub enum Backend {
    Wayland,
    CoTanOS,
}

pub struct Window {
    ptr: *mut LuminosWindow,
}

impl Window {
    pub fn new(backend: Backend, title: &str, width: i32, height: i32) -> Option<Self> {
        println!("[RUST] Próba utworzenia okna: {} ({}x{})", title, width, height);
        
        let c_title = match CString::new(title) {
            Ok(c) => c,
            Err(e) => {
                eprintln!("[RUST] Błąd konwersji tytułu na CString: {}", e);
                return None;
            }
        };
        
        let backend_int = match backend {
            Backend::Wayland => 0,
            Backend::CoTanOS => 1,
        };

        println!("[RUST] Wywoływanie funkcji C luminos_window_create...");
        let ptr = unsafe { luminos_window_create(backend_int, c_title.as_ptr(), width, height) };
        println!("[RUST] Funkcja C zwróciła wskaźnik: {:?}", ptr);
        
        if ptr.is_null() {
            eprintln!("[RUST] Wskaźnik jest null! Sprawdź logi z kodu C powyżej.");
            None
        } else {
            Some(Window { ptr })
        }
    }
    
    pub fn should_close(&self) -> bool {
        unsafe { luminos_window_should_close(self.ptr) != 0 }
    }
    
    pub fn swap_buffers(&self) {
        unsafe { luminos_window_swap_buffers(self.ptr) }
    }
    
    pub fn clear(&self, r: f32, g: f32, b: f32) {
        unsafe { luminos_window_clear(self.ptr, r, g, b) }
    }
    
    pub fn poll_events(&self) -> bool {
        unsafe { luminos_window_poll_events(self.ptr) != 0 }
    }
}

impl Drop for Window {
    fn drop(&mut self) {
        unsafe { luminos_window_destroy(self.ptr); }
    }
}

unsafe impl Send for Window {}
