use std::{
    fmt, io::{self, BufRead, BufReader}, thread, time::Duration,
};

use serialport::SerialPort;

pub struct LedClient {
    baud: u32,
    path: &'static str,
    delay: Duration,

    writer: Box<dyn SerialPort>,
    reader: BufReader<Box<dyn SerialPort>>,
}

pub struct Color {
    r: u8,
    g: u8,
    b: u8,
}

pub enum Command {
    PING,
    BASE {
        color: Color,
        duration: Duration,
    },
    FX {
        color: Color,
        attack: Duration,
        hold: Duration,
        release: Duration
    }
}

impl Color {
    pub fn new(r: u8, g: u8, b: u8) -> Self {
        Self {
            r,
            g,
            b,
        }
    } 
}

impl fmt::Display for Color {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {} {}", self.r, self.g, self.b)
    }
}

impl fmt::Display for Command {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Command::PING => write!(f, "PING"),
            Command::BASE { color, duration } => write!(f, "BASE {color} {}", duration.as_millis()),
            Command::FX { color, attack, hold, release } => write!(f, "FX {color}, {} {} {}", attack.as_millis(), hold.as_millis(), release.as_millis()),
        }
    }
}

impl LedClient {
    pub fn write_raw<S: AsRef<str>>(&mut self, cmd: S) -> Result<String, io::Error> {
        self.writer.write_all(format!("{}\n", cmd.as_ref()).as_bytes())?;
        self.writer.flush()?;

        let mut replay = String::new();
        let r = match self.reader.read_line(&mut replay) {
            Ok(0) => Err(io::ErrorKind::UnexpectedEof.into()),
            Ok(_) => Ok(replay.trim().to_owned()),
            Err(e) => Err(e),
        };
        r
    }

    pub fn write_command(&mut self, cmd: Command) -> Result<String, io::Error> {
        self.write_raw(cmd.to_string())
    }
}

pub fn new(baud: u32, path: &'static str, delay: Duration) -> io::Result<LedClient> {
    let mut port = serialport::new(path, baud)
        .timeout(Duration::from_secs(1))
        .dtr_on_open(false)
        .open()?;

    _ = port.write_request_to_send(true);
    thread::sleep(delay);
    port.clear(serialport::ClearBuffer::Input)?;
    let reader = BufReader::new(port.try_clone()?);

    Ok(LedClient {
        baud,
        path,
        delay,
        writer: port,
        reader,
    })
}