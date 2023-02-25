#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>
#include <iostream>
#include <cstdlib>

#include <alsa/asoundlib.h>
#include <xmp.h>
#include <ncurses.h>

#define VERSION "0.0.2.1"
#define SAMPLERATE 48000
#define BUFFER_SIZE 250000

static char *note_name[] = { "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };

void updateTrack(char* name, char* type);
void renderRows(WINDOW* win, xmp_module_info *mi, xmp_frame_info *fi);

static char *device = "default";
int chanOffset = 0; int detail = 0; int vol = 100;
bool looper = false, is_stopped = false;
int main(int argc, char *argv[]) {
	printf("Trakker %s (with libxmp %s)\n", VERSION, xmp_version);
	int time, key, err, row, pos;
	bool fupdate = false;
	snd_pcm_t *handle;
	snd_pcm_sframes_t frames;
	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	if ((err = snd_pcm_set_params(handle, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, SAMPLERATE, 1, BUFFER_SIZE)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	printf("Using Audio Driver: %s\n", "ALSA");
	
	xmp_context c;
	c = xmp_create_context();
	if (xmp_load_module(c, argv[1]) != 0) {
			fprintf(stderr, "Failed to load Module: %s\n", argv[1]);
			exit(EXIT_FAILURE);
	}
	
	struct xmp_module_info mi;
	struct xmp_frame_info fi;
	
	printf("Loaded module: %s\n", argv[1]);
	
	WINDOW *win;
	initscr();
	
	if (LINES < 5 || COLS < 5) {
		endwin();
		fprintf(stderr, "ncurses failed: Display is too small.\n");
		exit(EXIT_FAILURE);
	}
	
	if(has_colors() == FALSE) { 
		endwin();
		fprintf(stderr, "ncurses failed: No color support.\n");
		exit(EXIT_FAILURE);
	}
	start_color();
	
	if (can_change_color() == TRUE) {
		init_color(COLOR_BLACK, 0, 0, 0);
	} else { printf("Color changing not supported!\n"); }
	
	init_pair(1, COLOR_WHITE, COLOR_BLUE); // PLAYHEAD
	init_pair(2, COLOR_WHITE, COLOR_RED); // PAUSED
	
	cbreak();
	noecho();
	curs_set(0);
	
	win = newwin(LINES-2, COLS-2, 1, 1);
	keypad(win, TRUE);
	wmove(win, 0, 0);
	wprintw(win, "");
	printf("Loaded ncurses display.\n");
	
	refresh();
	
	xmp_get_module_info(c, &mi);
	row = pos = -1;
	chanOffset = 0;
	xmp_start_player(c, SAMPLERATE, 0);
	
	updateTrack(mi.mod->name, mi.mod->type);
	while (true) {
		xmp_get_frame_info(c, &fi);
		if (xmp_play_frame(c) != 0 && !is_stopped) break;
		if (!looper && fi.loop_count > 0) break;
		
		mvprintw(
			0, COLS-11, 
			"%02u:%02u/%02u:%02u", 
			((fi.time / 1000) / 60) % 60, 
			(fi.time / 1000) % 60, 
			((fi.total_time / 1000) / 60) % 60, 
			(fi.total_time / 1000) % 60
		);
		
		keys:
		wtimeout(win, is_stopped?-1:0);
		if ((key = wgetch(win)) != 0) {
			vol = xmp_get_player(c, XMP_PLAYER_VOLUME);
			switch (key) {
				case ' ': // Pause/Play
					time = fi.time;
					is_stopped = !is_stopped; 
					break;
				case KEY_LEFT: // Move Channels Left
					if (chanOffset > 0) chanOffset--;
					break;
				case KEY_RIGHT: // Move Channels Right
					if (chanOffset < mi.mod->chn-1) chanOffset++;
					break;
				case KEY_UP: // Seek Up
					if (is_stopped) fi.row--;
					else xmp_set_position(c, fi.pos-1);
					break;
				case KEY_DOWN: // Seek Down
					if (is_stopped) fi.row++;
					else xmp_set_position(c, fi.pos+1);
					break;
				case '+':
					if (vol < 100) xmp_set_player(c, XMP_PLAYER_VOLUME, vol+=5);
					break;
				case '-':
					if (vol > 0) xmp_set_player(c, XMP_PLAYER_VOLUME, vol-=5);
					break;
				case 'l':
					looper = !looper;
					break;
				case '1': detail = 0; break;
				case '2': detail = 1; break;
				case '3': detail = 2; break;
				case '4': detail = 3; break;
			};
			renderRows(win, &mi, &fi);
		}

		if (!is_stopped) {
			frames = snd_pcm_bytes_to_frames(handle, fi.buffer_size);
			if (snd_pcm_writei(handle, fi.buffer, frames) < 0) {
				snd_pcm_prepare(handle);
			}
		
			if (fi.pos != pos) {
				pos = fi.pos;
				row = -1;
			}
			if (fi.row != row) {
				renderRows(win, &mi, &fi);
				row = fi.row;
				fupdate = false;
			}
 		} else goto keys;
	}
	printf("Closing ncurses...\n");
	clrtoeol();
	refresh();
	endwin();
	printf("Releasing libxmp...\n");
	xmp_end_player(c);
	xmp_release_module(c);
		xmp_free_context(c);
	
	printf("Shutting down audio driver...\n");
	err = snd_pcm_drain(handle);
	if (err < 0)
		printf("snd_pcm_drain failed: %s\n", snd_strerror(err));
	snd_pcm_close(handle);
	return 0;
}

void updateTrack(char* name, char* type) {
	mvprintw(
		0, 0, 
		"%s (%s)",
		name,
		type
	);
}

void renderRows(WINDOW* win, xmp_module_info *mi, xmp_frame_info *fi) {
	werase(win);
	move(LINES-1, 0);
	wclrtoeol(stdscr);
	mvprintw(
 		LINES-1, 0, 
 		"[%c] PAT:%02x:%02x/%02x BPM:%02u SPD:%02u CHAN:%02u/%02u VOL:%03u d%i %s",
 		is_stopped?'x':'>',
 		fi->pos, 
 		fi->pattern,
 		mi->mod->pat,
 		fi->bpm,
 		fi->speed,
 		chanOffset,
 		mi->mod->chn-1,
 		vol,
 		detail+1,
 		looper?"LOOP ":""
 	);
	int view_chanOffset = chanOffset;
	int view_detail = detail;
	int dlvl = (7+(detail*4));
	for (int y = 0; y < LINES - 2; y++) {
		int trow = (fi->row - ((LINES - 2) / 2))+y;
		if (trow > fi->num_rows || trow < 0) { continue;}
		if (trow == fi->row) {
			wattron(win, COLOR_PAIR(is_stopped?2:1));
			wattron(win, A_BOLD);
		} else {
			wattroff(win, COLOR_PAIR(is_stopped?2:1));
			wattroff(win, A_BOLD);
		}
		wmove(win, y, 0);
		wprintw(win, "%02X", trow);
		int coff = ((COLS - 2) % dlvl);
		int maxcol = -1;
		for (int i = chanOffset; i < mi->mod->chn; i++) {
			if (coff+(((i-chanOffset)+1)*dlvl)+dlvl > COLS) {
				maxcol = i;
				break;
			}
			wmove(win, y, coff+((i-chanOffset)*dlvl)+2);
			int track = mi->mod->xxp[fi->pattern]->index[i];
			struct xmp_event event = mi->mod->xxt[track]->event[trow];
			if (i > 0 && i == chanOffset) wprintw(win, "<");
			else wprintw(win, "|");
			if (event.note > 0x80) {
				wprintw(win, "=== ");
			} else if (event.note > 0) {
				int note = event.note - 1;
				wprintw(win, "%s%d ", note_name[note % 12], note / 12);
			} else {
				wprintw(win, "--- ");
			}
			if (event.ins > 0) {
				wprintw(win, "%02X", event.ins);
			} else {
				wprintw(win, "--");
			}
					
			if (detail >= 1) {
				if (event.vol > 0) {
					wprintw(win, " v%02X", event.vol);
				} else {
					wprintw(win, " ---");
				}
			
				if (detail >= 2) {
					if (event.fxt > 0) {
						char t = event.fxt;
						char t2[0];
						if (t > 0x0f) t = 'F' + (t - 0x0f);
						else { sprintf(t2, "%X", t); t = t2[0]; }
						wprintw(win, " %c%02X", t, event.fxp);
					} else {
						wprintw(win, " ---");
					}
					
					if (detail >= 3) {
						if (event.f2t > 0) {
							char t = event.f2t;
							char t2[0];
							if (t > 0x0f) t = 'F' + (t - 0x0f);
							else { sprintf(t2, "%X", t); t = t2[0]; }
							wprintw(win, " %c%02X", t, event.f2p);
						} else {
							wprintw(win, " ---");
						}
					}
				}
			}
			view_chanOffset = chanOffset; 
			view_detail = detail;
		}
		if (maxcol < mi->mod->chn && maxcol > 0) wprintw(win, ">");
		else wprintw(win, "|");
	}
	refresh();
	wrefresh(win);
}