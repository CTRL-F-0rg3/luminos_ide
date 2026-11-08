use luminos_gfx::{Window, Backend};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

fn main() {
    // Używamy backendu CoTanOS, który komunikuje się z naszym kompozytorem
    let window = Window::new(Backend::CoTanOS, "Luminos IDE (Live!)", 1280, 720)
        .expect("Failed to create window");
    
    println!("Okno utworzone! Kompozytor powinien je teraz wyświetlić.");
    println!("Naciśnij Ctrl+C w tym terminalu, aby zamknąć okno i zakończyć.");

    // Prosta pętla, która utrzymuje aplikację przy życiu
    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();
    
    // Wątek do obsługi Ctrl+C
    ctrlc::set_handler(move || {
        println!("\nOtrzymano sygnał zamykania. Zamykanie okna...");
        r.store(false, Ordering::SeqCst);
    }).expect("Błąd ustawiania handlera Ctrl+C");

    let mut frame_count = 0;
    while running.load(Ordering::SeqCst) {
        window.poll_events();
        
        // Symulacja renderowania (w przyszłości tu będzie prawdziwy OpenGL)
        window.clear(0.1, 0.1, 0.2);
        window.swap_buffers();

        frame_count += 1;
        if frame_count % 60 == 0 {
            print!(".");
            use std::io::Write;
            std::io::stdout().flush().unwrap();
        }
        
        // Małe opóźnienie, żeby nie zjadać 100% CPU w trybie stub
        thread::sleep(Duration::from_millis(16));
    }
    
    println!("\nZamykanie aplikacji. Pa!");
}
