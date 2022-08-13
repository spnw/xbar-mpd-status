#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mpd/client.h>

const char *argv0;

struct
{
  struct mpd_connection *conn;
  struct mpd_status *status;
  struct mpd_song *song;
  enum mpd_state state;
  enum mpd_single_state single;
  bool active; /* True if a song is playing or paused. */
  unsigned int song_pos;
  unsigned int queue_length;
} mpd;

/* xbar offers no way to escape the pipe character, so we must replace
   it with a lookalike. */
static void
print_safely(const char *s)
{
  char c;

  while ((c = *s++))
    if (c == '|')
      printf("ǀ");
    else
      putchar(c);
}

static void
print_song_info(void)
{
  mpd.song = mpd_run_current_song(mpd.conn);

  if (!mpd.song)
    err(1, "Failed to retrieve current song");

  if (mpd_connection_get_error(mpd.conn) != MPD_ERROR_SUCCESS) {
    err(1, "%s", mpd_connection_get_error_message(mpd.conn));
  }

  if (mpd.state == MPD_STATE_PAUSE)
    printf("⏸ ");
  else if (mpd.single == MPD_SINGLE_ON || mpd.single == MPD_SINGLE_ONESHOT)
    printf("⏯ ");

  const char *artist = mpd_song_get_tag(mpd.song, MPD_TAG_ARTIST, 0);
  const char *title = mpd_song_get_tag(mpd.song, MPD_TAG_TITLE, 0);

  if (artist)
    print_safely(artist);
  if (artist && title)
    printf(" - ");
  if (title)
    print_safely(title);
  if (!(artist || title))
    print_safely(mpd_song_get_uri(mpd.song));
}

static void
print_time(unsigned int t)
{
  unsigned int
    hours = (t / 3600),
    minutes = (t % 3600 / 60),
    seconds = (t % 3600 % 60);

  if (hours)
    printf("%u:%02u:%02u", hours, minutes, seconds);
  else
    printf("%u:%02u", minutes, seconds);
}

static void
print_duration_info(void)
{
  unsigned elapsed = mpd_status_get_elapsed_time(mpd.status);
  unsigned dur = mpd_song_get_duration(mpd.song);

  print_time(elapsed);
  printf(" / ");
  if (dur)
    print_time(dur);
  else
    printf("?");
  puts("\n---");
}

static void
control(const char *label, const char *command, bool enable)
{
  if (enable)
    printf("%s | shell='%s' | param1=%s | refresh=true\n", label, argv0, command);
  else
    puts(label);
}

static void
print_controls(void)
{
  if (mpd.state == MPD_STATE_PLAY)
    control("⏸ Pause", "pause", true);
  else
    control("▶️ Play", "play", mpd.queue_length);

  control("⏹ Stop", "stop", mpd.active);

  puts("---");

  control("⏭ Next", "next", mpd.active && (mpd.song_pos < (mpd.queue_length - 1)));
  control("⏮ Previous", "previous", mpd.active && (mpd.song_pos > 0));

  puts("---");

  if (mpd.active && (mpd.song_pos < (mpd.queue_length - 1))) {
    if (mpd.single == MPD_SINGLE_OFF)
      control("⏯ Pause after this song", "single_oneshot", true);
    else
      control("⏯ Keep playing after this song", "single_off", true);
  } else
    control("⏯ Pause after this song", "single_oneshot", false);
}

static void
cleanup(void)
{
  if (mpd.song)
    mpd_song_free(mpd.song);
  if (mpd.status)
    mpd_status_free(mpd.status);
  if (mpd.conn)
    mpd_connection_free(mpd.conn);
}

int
main(int argc, char *argv[])
{
  argv0 = argv[0];

  atexit(cleanup);

  mpd.conn = mpd_connection_new(NULL, 0, 0);
  if (!mpd.conn)
    err(1, "Failed to create connection");

  mpd.status = mpd_run_status(mpd.conn);
  if (!mpd.status)
    err(1, "Failed to get status (is MPD running?)");

  mpd.state = mpd_status_get_state(mpd.status);
  mpd.single = mpd_status_get_single_state(mpd.status);
  mpd.active = (mpd.state == MPD_STATE_PLAY || mpd.state == MPD_STATE_PAUSE);
  mpd.song_pos = mpd_status_get_song_pos(mpd.status);
  mpd.queue_length = mpd_status_get_queue_length(mpd.status);

  if (argc < 2) {
    if (mpd.active)
      print_song_info();
    else
      printf("No song playing");

    if (mpd.song_pos == -1)
      printf(" (%d)", mpd.queue_length);
    else
      printf(" (%d/%d)", mpd.song_pos + 1, mpd.queue_length);

    puts(" | emojize=false\n---");

    if (mpd.active)
      print_duration_info();

    print_controls();

    return 0;
  }

#define CMD(X) (strcmp(argv[1], (X)) == 0)

  if (CMD("play"))
    mpd_run_play(mpd.conn);
  else if (CMD("pause"))
    mpd_run_pause(mpd.conn, true);
  else if (CMD("stop"))
    mpd_run_stop(mpd.conn);
  else if (CMD("next"))
    mpd_run_next(mpd.conn);
  else if (CMD("previous"))
    mpd_run_previous(mpd.conn);
  else if (CMD("single_oneshot"))
    mpd_run_single_state(mpd.conn, MPD_SINGLE_ONESHOT);
  else if (CMD("single_off"))
    mpd_run_single_state(mpd.conn, MPD_SINGLE_OFF);

  return 0;
}
