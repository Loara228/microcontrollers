use std::time::Duration;

use axum::{Router, body::Bytes, extract::State, http::StatusCode, routing::post};
use tokio::sync::{mpsc, oneshot};

// Настройки
const BAUD: u32 = 115200;
const PATH: &'static str = "/dev/ttyACM0";
const DELAY: Duration = Duration::from_millis(50);
const ADDR: &'static str = "127.0.0.1:5678";

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let mut led = led_client::new(BAUD, PATH, DELAY)?;
    let (tx, mut rx) = mpsc::channel::<(String, oneshot::Sender<Result<(), String>>)>(32);

    tokio::task::spawn_blocking(move || {
        while let Some((command, reply)) = rx.blocking_recv() {
            let result = led.write_raw(&command).map(|_| ()).map_err(|e| e.to_string());

            let _ = reply.send(result);
        }
    });

    let app = Router::new().route("/", post(command)).with_state(tx);
    let listener = tokio::net::TcpListener::bind(ADDR).await?;

    axum::serve(listener, app).await
}

async fn command(
    State(tx): State<mpsc::Sender<(String, oneshot::Sender<Result<(), String>>)>>,
    body: Bytes,
) -> Result<StatusCode, (StatusCode, String)> {
    let command = String::from_utf8(body.to_vec()).map_err(|_| {
        (
            StatusCode::BAD_REQUEST,
            "Body should be UTF-8".into(),
        )
    })?;

    let (reply_tx, reply_rx) = oneshot::channel();
    tx.send((command, reply_tx)).await.map_err(|_| {
        (
            StatusCode::SERVICE_UNAVAILABLE,
            "LED client unavailable".into(),
        )
    })?;

    let result = reply_rx.await.map_err(|_| {
        (
            StatusCode::INTERNAL_SERVER_ERROR,
            "LED client died :(".into(),
        )
    })?;

    result
        .map(|()| StatusCode::OK)
        .map_err(|e| (StatusCode::INTERNAL_SERVER_ERROR, e))
}
