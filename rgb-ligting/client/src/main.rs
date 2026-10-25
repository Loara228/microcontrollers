use std::{io::{BufRead, BufReader, ErrorKind}, thread, time::Duration};



// Настройки
const BAUD: u32 = 115200;
const PATH: &'static str = "/dev/ttyACM0";
const DELAY: Duration = Duration::from_millis(50);

fn main() -> std::io::Result<()> {
    let mut port = serialport::new(PATH, BAUD).timeout(Duration::from_secs(1)).dtr_on_open(false).open()?;
    _= port.write_request_to_send(true);
    thread::sleep(DELAY);
    port.clear(serialport::ClearBuffer::Input)?;

    let mut reader = BufReader::new(port.try_clone()?);

    let command = "BASE 255 0 0 1000";
    port.write_all(format!("{command}\n").as_bytes())?;
    port.flush()?;

    let mut replay = String::new();
    let read_result = match reader.read_line(&mut replay) {
        Ok(0) => return Err(ErrorKind::UnexpectedEof.into()),
        Ok(_) => Ok(()),
        Err(e) => Err(e),
    };

    read_result?;

    let replay = replay.trim();
    println!("{replay}");
    Ok(())
}
