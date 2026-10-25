use std::{io, thread, time::Duration};

use led_client::{Color, Command};

// Настройки
const BAUD: u32 = 115200;
const PATH: &'static str = "/dev/ttyACM0";
const DELAY: Duration = Duration::from_millis(50);

fn main() -> io::Result<()> {

    let mut led = led_client::new(BAUD, PATH, DELAY)?;

    thread::sleep(Duration::from_secs(2));

    let result = led.write_command(Command::PING)?;
    println!("PING: {}", result);

    let result = led.write_command(Command::BASE {
        color: Color::new(255, 127, 55),
        duration: Duration::from_secs(1),
    })?;
    println!("BASE: {}", result);

    thread::sleep(Duration::from_secs(2));

    let result = led.write_command(Command::FX {
        color: Color::new(0, 255, 0),
        attack: Duration::from_millis(500),
        hold: Duration::from_secs(3),
        release: Duration::from_millis(500),
    })?;
    println!("FX: {}", result);

    thread::sleep(Duration::from_secs(4));
    Ok(())
}
