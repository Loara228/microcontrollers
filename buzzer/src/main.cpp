#include <Arduino.h>
#include <Preferences.h>

#include "songs/babyelephantwalk.h"
#include "songs/doom.h"
#include "songs/keyboardcat.h"
#include "songs/miichannel.h"
#include "songs/nevergonnagiveyouup.h"
#include "songs/nokia.h"
#include "songs/starwars.h"
#include "songs/supermariobros.h"
#include "songs/takeonme.h"

#define PIN_BUZZER 0
#define PIN_LED 1

Preferences prefs;

struct Song
{
    const int *melody;
    int tempo;
    int notes;
};

const Song songs[] = {
    {babyelephantwalk_melody, babyelephantwalk_tempo, sizeof(babyelephantwalk_melody) / sizeof(babyelephantwalk_melody[0]) / 2},
    {doom_melody2, doom_tempo, sizeof(doom_melody2) / sizeof(doom_melody2[0]) / 2},
    {keyboardcat_melody, keyboardcat_tempo, sizeof(keyboardcat_melody) / sizeof(keyboardcat_melody[0]) / 2},
    {miichannel_melody, miichannel_tempo, sizeof(miichannel_melody) / sizeof(miichannel_melody[0]) / 2},
    {nevergonnagiveyouup_melody, nevergonnagiveyouup_tempo, sizeof(nevergonnagiveyouup_melody) / sizeof(nevergonnagiveyouup_melody[0]) / 2},
    {nokia_melody, nokia_tempo, sizeof(nokia_melody) / sizeof(nokia_melody[0]) / 2},
    {starwars_melody, starwars_tempo, sizeof(starwars_melody) / sizeof(starwars_melody[0]) / 2},
    {supermariobros_melody, supermariobros_tempo, sizeof(supermariobros_melody) / sizeof(supermariobros_melody[0]) / 2},
    {takeonme_melody, takeonme_tempo, sizeof(takeonme_melody) / sizeof(takeonme_melody[0]) / 2},
};
const int SONG_COUNT = sizeof(songs) / sizeof(songs[0]);


void led(int note) {
  if (note == 0) {
    analogWrite(PIN_LED, 0);
  } else {
    analogWrite(PIN_LED, map(constrain(note, 100, 2000), 100, 2000, 10, 255));
  }
}

void playSong(const Song &s) {
  int wholenote = (60000 * 4) / s.tempo;

  for (int thisNote = 0; thisNote < s.notes * 2; thisNote += 2) {
    int note    = s.melody[thisNote];
    int divider = s.melody[thisNote + 1];
    int noteDuration;

    if (divider > 0) {
      noteDuration = wholenote / divider;
    } else if (divider < 0) {
      noteDuration = wholenote / abs(divider);
      noteDuration *= 1.5;
    } else {
      continue;
    }

    tone(PIN_BUZZER, note, noteDuration * 0.9);
    led(note);
    delay(noteDuration);
    noTone(PIN_BUZZER);
    analogWrite(PIN_LED, 0);
  }
}

void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  
  prefs.begin("player", false);
  int last = prefs.getInt("last", -1);

  int pick;
  do {
    pick = random(SONG_COUNT);
  } while (pick == last && SONG_COUNT > 1);

  prefs.putInt("last", pick);
  prefs.end();

  playSong(songs[pick]);
}

void loop()
{
}