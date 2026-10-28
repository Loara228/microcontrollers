use std::{
    env, fs,
    io::{self, Error, ErrorKind},
    path::PathBuf,
    process,
    sync::{Mutex as StdMutex, OnceLock},
    time::Duration,
};

use dota::{
    ServerBuilder,
    components::{GameState, heroes::GameHeroes, players::GamePlayers},
};
use led_client::{Color, Command, LedClient};
use tokio::sync::Mutex;
use tracing::{error, info, level_filters::LevelFilter, warn};

const LED_BAUD: u32 = 115200;
const LED_PORT: &str = "/dev/cu.usbmodem101";
const LED_TIMEOUT: Duration = Duration::from_millis(500);

const GSI_CONF_FILENAME: &str = "gamestate_integration_loara.cfg";
const GSI_CONF_CONTENT: &str = r#""Dota 2 Game State Integration Configuration"
{
   "uri"               "http://localhost:7076/"
   "timeout"           "5.0"
   "buffer"            "0.1"
   "throttle"          "0.1"
   "heartbeat"         "30"
   "data"
   {
       "map"           "1"
       "player"        "1"
       "hero"          "1"
       "provider"      "1"
   }
}
"#;

static LED: OnceLock<Mutex<LedClient>> = OnceLock::new();

static PREV: StdMutex<Option<Snapshot>> = StdMutex::new(None);

#[derive(Debug, Clone, Copy, Default, PartialEq)]
struct Snapshot {
    kills: Option<u16>,
    assists: Option<u16>,
    alive: Option<bool>,
    smoked: Option<bool>,
    magic_immune: Option<bool>,
    level: Option<u8>,
    daytime: Option<bool>,
    paused: Option<bool>,
}

impl Snapshot {
    fn from_state(state: &GameState) -> Self {
        let mut s = Self::default();

        if let Some(GamePlayers::Playing(player)) = &state.players {
            s.kills = Some(player.kills);
            s.assists = Some(player.assists)
        }
        if let Some(GameHeroes::Playing(hero)) = &state.heroes {
            s.alive = hero.alive;
            s.smoked = hero.smoked;
            s.magic_immune = hero.magicimmune;
            s.level = hero.level;
        }
        if let Some(map) = &state.map {
            s.daytime = Some(map.daytime);
            s.paused = Some(map.paused);
        }
        s
    }

    fn merge(&mut self, new: &Snapshot) {
        fn set<T: Copy>(dst: &mut Option<T>, src: Option<T>) {
            if src.is_some() {
                *dst = src;
            }
        }
        set(&mut self.kills, new.kills);
        set(&mut self.assists, new.assists);
        set(&mut self.alive, new.alive);
        set(&mut self.smoked, new.smoked);
        set(&mut self.magic_immune, new.magic_immune);
        set(&mut self.level, new.level);
        set(&mut self.daytime, new.daytime);
        set(&mut self.paused, new.paused);
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum Change {
    Kill { total: u16 },
    Assist { total: u16 },
    Died,
    Respawned,
    LevelUp(u8),
    Smoked(bool),
    MagicImmune(bool),
    DayTime(bool),
    Paused(bool),
}

fn changed<T: Copy + PartialEq>(old: Option<T>, new: Option<T>) -> Option<T> {
    match (old, new) {
        (Some(o), Some(n)) if o != n => Some(n),
        _ => None,
    }
}

fn diff(old: &Snapshot, new: &Snapshot) -> Vec<Change> {
    let mut out = Vec::new();

    // Только если появляются фраги, а не уменьшаются
    match (old.kills, new.kills) {
        (Some(o), Some(n)) if n > o => out.push(Change::Kill { total: n }),
        _ => {}
    }
    match (old.assists, new.assists) {
        (Some(o), Some(n)) if n > o => out.push(Change::Assist { total: n }),
        _ => {}
    }
    match (old.level, new.level) {
        (Some(o), Some(n)) if n > o => out.push(Change::LevelUp(n)),
        _ => {}
    }

    match changed(old.alive, new.alive) {
        Some(true) => out.push(Change::Respawned),
        Some(false) => out.push(Change::Died),
        None => {}
    }

    if let Some(v) = changed(old.smoked, new.smoked) {
        out.push(Change::Smoked(v));
    }
    if let Some(v) = changed(old.magic_immune, new.magic_immune) {
        out.push(Change::MagicImmune(v));
    }
    if let Some(v) = changed(old.daytime, new.daytime) {
        out.push(Change::DayTime(v));
    }
    if let Some(v) = changed(old.paused, new.paused) {
        out.push(Change::Paused(v));
    }

    out
}

fn detect_changes(new: Snapshot) -> Vec<Change> {
    let mut guard = PREV.lock().unwrap_or_else(|e| e.into_inner());
    match guard.as_mut() {
        Some(prev) => {
            let changes = diff(prev, &new);
            prev.merge(&new);
            changes
        }
        None => {
            *guard = Some(new);
            Vec::new()
        }
    }
}
fn base_color() -> Color {
    Color::new(122, 253, 194) // обычная подсветка + DayTime(true)
}

fn command_for(change: Change) -> Option<Command> {
    match change {
        Change::Kill { .. } => Some(Command::FX {
            color: Color::new(0, 0, 0),
            attack: Duration::from_millis(60),
            hold: Duration::from_millis(30),
            release: Duration::from_millis(60),
            repeats: 4,
        }),
        Change::Assist { .. } => Some(Command::FX {
            color: Color::new(35, 255, 22),
            attack: Duration::from_millis(200),
            hold: Duration::from_millis(300),
            release: Duration::from_millis(400),
            repeats: 1,
        }),
        Change::Died => Some(Command::FX {
            color: Color::new(255, 0, 0),
            attack: Duration::from_millis(200),
            hold: Duration::from_secs(60),
            release: Duration::from_secs(60),
            repeats: 99,
        }),
        Change::Respawned => Some(Command::FX {
            color: Color::new(35, 255, 22),
            attack: Duration::from_millis(200),
            hold: Duration::from_secs(1),
            release: Duration::from_secs(3),
            repeats: 1,
        }),
        Change::LevelUp(_) => Some(Command::FX {
            color: Color::new(255, 255, 30),
            attack: Duration::from_millis(100),
            hold: Duration::from_secs(1),
            release: Duration::from_secs(2),
            repeats: 1,
        }),
        Change::DayTime(true) => Some(Command::BASE {
            color: base_color(),
            duration: Duration::from_secs(2),
        }),
        Change::DayTime(false) => Some(Command::BASE {
            color: Color::new(100, 15, 255),
            duration: Duration::from_secs(2),
        }),
        Change::Smoked(true) => Some(Command::FX {
            color: Color::new(255, 0, 255),
            attack: Duration::from_millis(200),
            hold: Duration::from_secs(60),
            release: Duration::from_millis(200),
            repeats: 99,
        }),
        Change::Smoked(false) => Some(Command::FX {
            color: Color::new(255, 0, 0),
            attack: Duration::from_millis(100),
            hold: Duration::from_secs(1),
            release: Duration::from_millis(500),
            repeats: 1,
        }),
        Change::MagicImmune(_) => None,
        Change::Paused(true) => None,
        Change::Paused(false) => None,
    }
}

#[tokio::main]
async fn main() -> io::Result<()> {
    tracing_subscriber::fmt()
        .with_max_level(LevelFilter::INFO)
        .init();

    if let Err(e) = init_gsi_conf() {
        error!("{e}");
        process::exit(1);
    }

    let mut led = led_client::new(LED_BAUD, LED_PORT, LED_TIMEOUT)
        .map_err(|e| Error::other(format!("Failed to open LED port {LED_PORT}: {e:?}")))?;

    rgb_fade(&mut led).await?;

    led.write_command(Command::BASE {
        color: base_color(),
        duration: Duration::from_secs(3),
    })?;

    if LED.set(Mutex::new(led)).is_err() {
        return Err(Error::other("LED already initialized"));
    }

    ServerBuilder::new("127.0.0.1:7076")
        .register(handler)
        .start()
        .expect("failed to start GSI server")
        .run_forever()
        .await;
    Ok(())
}

async fn handler(event: bytes::Bytes) -> Result<(), serde_json::Error> {
    let state: GameState = serde_json::from_slice(&event)?;
    let changes = detect_changes(Snapshot::from_state(&state));
    if changes.is_empty() {
        return Ok(());
    }

    let Some(led) = LED.get() else {
        warn!(
            "LED is not initialized, skipping {} change(s)",
            changes.len()
        );
        return Ok(());
    };
    let mut led = led.lock().await;

    for change in changes {
        info!(?change, "game state changed");
        if let Some(change) = command_for(change) {
            if let Err(e) = led.write_command(change) {
                error!("LED write failed: {e:?}");
            }
        }
    }

    Ok(())
}

async fn rgb_fade(led: &mut LedClient) -> io::Result<()> {
    let t = Duration::from_millis(350);
    led.write_command(Command::BASE {
        color: Color::new(255, 0, 0),
        duration: t,
    })?;
    tokio::time::sleep(t).await;
    led.write_command(Command::BASE {
        color: Color::new(255, 255, 0),
        duration: t,
    })?;
    tokio::time::sleep(t).await;
    led.write_command(Command::BASE {
        color: Color::new(0, 255, 0),
        duration: t,
    })?;
    tokio::time::sleep(t).await;
    led.write_command(Command::BASE {
        color: Color::new(0, 255, 255),
        duration: t,
    })?;
    tokio::time::sleep(t).await;
    led.write_command(Command::BASE {
        color: Color::new(0, 0, 255),
        duration: t,
    })?;
    tokio::time::sleep(t).await;
    led.write_command(Command::BASE {
        color: Color::new(255, 0, 255),
        duration: t,
    })?;
    tokio::time::sleep(t).await;

    Ok(())
}

fn init_gsi_conf() -> io::Result<()> {
    let cfg_dir = get_gsi_conf_path()
        .ok_or_else(|| Error::new(ErrorKind::NotFound, "Failed to get path to GSI config file"))?;

    if !cfg_dir.exists() {
        return Err(Error::new(
            ErrorKind::NotFound,
            format!("Directory not found '{}'", cfg_dir.display()),
        ));
    }

    let gsi_dir = cfg_dir.join("gamestate_integration");
    let gsi_conf_path = gsi_dir.join(GSI_CONF_FILENAME);

    match fs::read_to_string(&gsi_conf_path) {
        Ok(existing) if existing == GSI_CONF_CONTENT => {
            info!("GSI conf is up to date: {:?}", gsi_conf_path);
            return Ok(());
        }
        Ok(_) => warn!("GSI conf is outdated, rewriting. Restart Dota 2!"),
        Err(_) => {}
    }

    fs::create_dir_all(&gsi_dir)?;
    fs::write(&gsi_conf_path, GSI_CONF_CONTENT)?;
    fs::write(cfg_dir.join(GSI_CONF_FILENAME), GSI_CONF_CONTENT)?;
    info!("GSI conf created: {:?}", gsi_conf_path);
    Ok(())
}

fn get_gsi_conf_path() -> Option<PathBuf> {
    let relative_dota_cfg = "steamapps/common/dota 2 beta/game/dota/cfg";

    if cfg!(target_os = "windows") {
        let program_files = env::var("ProgramFiles(x86)").ok()?;
        let mut path = PathBuf::from(program_files);
        path.push("Steam");
        path.push(relative_dota_cfg);
        Some(path)
    } else if cfg!(target_os = "linux") {
        let home = env::var("HOME").ok()?;
        let mut path = PathBuf::from(home);
        path.push(".local/share/Steam");
        path.push(relative_dota_cfg);
        Some(path)
    } else if cfg!(target_os = "macos") {
        let home = env::var("HOME").ok()?;
        let mut path = PathBuf::from(home);
        path.push("Library/Application Support/Steam");
        path.push(relative_dota_cfg);
        Some(path)
    } else {
        None
    }
}
