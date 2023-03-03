#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>
#include <iostream>
#include <cstdlib>

#include <alsa/asoundlib.h>
#include <xmp.h>
#include <ncurses.h>

#define VERSION "0.2.2"
#define SAMPLERATE 48000
#define BUFFER_SIZE 250000

static char *note_name[] = { "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };

void updateTrack(char* name, char* type);
void renderTrack(WINDOW* win, xmp_module_info *mi, xmp_frame_info *fi);
void renderRows(WINDOW* win, xmp_module_info *mi, xmp_frame_info *fi);
void renderChannels(WINDOW* win, xmp_module_info *mi, xmp_frame_info *fi);
void renderInstruments(WINDOW* win, xmp_module_info *mi, xmp_frame_info *fi);
char getEffectType(int i);

static char *device = "default";
int chanOffset = 0; int insOffset = 0; int detail = 2; int vol = 100; int loop_times = 0;
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
		if (fi.loop_count > loop_times)
			if (looper) loop_times = fi.loop_count;
			else break; 
		
		// Print Top-Right time
		mvprintw(
			0, COLS-11, 
			"%02u:%02u/%02u:%02u", 
			((fi.time / 1000) / 60) % 60, 
			(fi.time / 1000) % 60, 
			((fi.total_time / 1000) / 60) % 60, 
			(fi.total_time / 1000) % 60
		);
		
		
		/* - Handles Key Events
		 * Key timeout is set to 0 to become non-blocking if playing. This allows
		 * Continualy looping to play the track without requiring a key press.
		 * Once the playstate changes through the boolean 'is_stopped', the timeout
		 * changes to -1 for an infinite timeout in the 'keys' loop which will loop
		 * until is_stopped is false. The block state is for efficiency.
		 */
		keys:
		wtimeout(win, is_stopped?-1:0);
		if ((key = wgetch(win)) != 0) {
			vol = xmp_get_player(c, XMP_PLAYER_VOLUME);
			switch (key) {
				case KEY_RESIZE:
					werase(stdscr);
					updateTrack(mi.mod->name, mi.mod->type);
					wresize(win, LINES-2, COLS-2);
					break;
				case ' ': // Pause/Play
					time = fi.time;
					is_stopped = !is_stopped; 
					break;
				case KEY_LEFT: // Move Channels Left
					if (chanOffset > 0 && detail < 7) chanOffset--;
					break;
				case KEY_RIGHT: // Move Channels Right
					if (chanOffset < mi.mod->chn-1 && detail < 7) chanOffset++;
					break;
				case KEY_UP: // Seek Up
					if (insOffset > 0 && detail == 7) insOffset--;
					else if (detail < 7) fi.row--;
					break;
				case KEY_DOWN: // Seek Down
					if (insOffset < mi.mod->ins-1 && detail == 7) insOffset++;
					else if (detail < 7) fi.row++;
					break;
				case KEY_PPAGE:
					xmp_set_position(c, fi.pos-1);
					break;
				case KEY_NPAGE:
					xmp_set_position(c, fi.pos+1);
					break;
				case '+':
					if (vol < 200) xmp_set_player(c, XMP_PLAYER_VOLUME, vol+=5);
					break;
				case '-':
					if (vol > 0) xmp_set_player(c, XMP_PLAYER_VOLUME, vol-=5);
					break;
				case 'l':
					looper = !looper;
					break;
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					detail = key-48;
					break;
			};
			renderTrack(win, &mi, &fi);
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
			if (fi.row != row || detail >= 6) {
				renderTrack(win, &mi, &fi);
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

void renderTrack(WINDOW* win, xmp_module_info *mi, xmp_frame_info *fi) {
	werase(win);
	move(LINES-1, 0);
	wclrtoeol(stdscr);
	mvprintw(
 		LINES-1, 0, 
 		"[%c] PAT:%02x:%02x/%02x BPM:%02u SPD:%02u CHN:%02u/%02u INS:%02u/%02u VOL:%03u WIN:%i %s",
 		is_stopped?'x':'>',
 		fi->pos, 
 		fi->pattern,
 		mi->mod->pat,
 		fi->bpm,
 		fi->speed,
 		chanOffset+1,
 		mi->mod->chn,
 		insOffset,
 		mi->mod->ins,
 		vol,
 		detail,
 		looper?"LOOP ":""
 	);
	if (detail < 6) // Row views
		renderRows(win, mi, fi);
	else if (detail == 6) //Channel views
		renderChannels(win, mi, fi);
	else if (detail == 7)
		renderInstruments(win, mi, fi);
	
	refresh();
	wrefresh(win);
}

void renderRows(WINDOW* win, xmp_module_info *mi, xmp_frame_info *fi) {
	int dlvl;
	// Set proper sizes for channels.
	if (detail == 5) dlvl = 19;
	else if (detail == 4) dlvl = 15;
	else if (detail == 3) dlvl = 11;
	else if (detail == 2) dlvl = 7;
	else dlvl = 2;
	for (int y = 0; y < LINES - 2; y++) {
		int trow = (fi->row - ((LINES - 2) / 2))+y;
		if (trow > fi->num_rows-1 || trow < 0) { continue;}
		if (trow == fi->row) {
			wattron(win, COLOR_PAIR(is_stopped?2:1));
			wattron(win, A_BOLD);
		} else {
			wattroff(win, COLOR_PAIR(is_stopped?2:1));
			wattroff(win, A_BOLD);
		}
		wmove(win, y, 0);
		wprintw(win, "%02X", trow);
		int maxcol = -1;
		for (int i = chanOffset; i < mi->mod->chn; i++) {
			if (1+(((i-chanOffset)+1)*dlvl)+dlvl > COLS) {
				maxcol = i;
				break;
			}
			wmove(win, y, 1+((i-chanOffset)*dlvl)+2);
			int track = mi->mod->xxp[fi->pattern]->index[i];
			struct xmp_event event = mi->mod->xxt[track]->event[trow];
			if (i > 0 && i == chanOffset) wprintw(win, "<");
			else wprintw(win, "|");
			if (detail >= 2) {
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
						
				if (detail >= 3) {
					int vol;
					if ((vol = event.vol) != 0) {
						// I made this wall...
						// Then I realized libxmp does not give this wall purpose...
						/*
 						char v = '?';
 						if (vol <= 0x50) { v = 'v'; vol--; }
 						else if (vol >= 0x60 && vol <= 0x6F) { v = 'd'; vol - 0x50; }
 						else if (vol >= 0x70 && vol <= 0x7F) { v = 'c'; vol - 0x60; }
 						else if (vol >= 0x80 && vol <= 0x8F) { v = 'b'; vol - 0x70; }
 						else if (vol >= 0x90 && vol <= 0x9F) { v = 'a'; vol - 0x80; }
 						else if (vol >= 0xA0 && vol <= 0xAF) { v = 'u'; vol - 0x90; }
 						else if (vol >= 0xB0 && vol <= 0xBF) { v = 'h'; vol - 0xA0; }
 						else if (vol >= 0xC0 && vol <= 0xCF) { v = 'p'; vol - 0xB0; }
 						else if (vol >= 0xD0 && vol <= 0xDF) { v = 'l'; vol - 0xC0; }
 						else if (vol >= 0xE0 && vol <= 0xEF) { v = 'r'; vol - 0xD0; }
 						else if (vol >= 0xF0 && vol <= 0xFF) { v = 'g'; vol - 0xE0; }
 						*/
 						char v = 'v';
						wprintw(win, " %c%02i", v, vol);
					} else {
						wprintw(win, " ---");
					}
				
					if (detail >= 4) {
						char f1;
						if ((f1 = getEffectType(event.fxt)) != 0)
							wprintw(win, " %c%02X", f1, event.fxp);
						else
							wprintw(win, " ---");
						
						if (detail >= 5) {
							char f2;
							if ((f2 = getEffectType(event.fxt)) != 0)
								wprintw(win, " %c%02X", f2, event.f2p);
							else
								wprintw(win, " ---");
						}
					}
				}
			} else {
				if (event.note > 0x80) {
					wprintw(win, "-");
				} else if (event.note > 0) {
					wprintw(win, "#");
				} else {
					wprintw(win, " ");
				}
			}
		}
		if (maxcol < mi->mod->chn && maxcol > 0) wprintw(win, ">");
		else wprintw(win, "|");
	}
}

void renderChannels(WINDOW* win, xmp_module_info *mi, xmp_frame_info *fi) {
	int chns = mi->mod->chn;
	for (int y = chanOffset; y < chns; y++) {
		struct xmp_channel_info cinfo = fi->channel_info[y];
		if (y > (LINES - 3)+chanOffset) break;
		wmove(win, y-chanOffset, 0);
		int cvol = (cinfo.volume * (COLS - 7)) / (64 * (vol / 100));
		wprintw(win, "%02X [", y);
		for (int c = 0; c < COLS - 7; c++) {
			if (c < cvol) wprintw(win, "#");
			else wprintw(win, " ");
		}
		wprintw(win, "]");
	}
}

void renderInstruments(WINDOW* win, xmp_module_info *mi, xmp_frame_info *fi) {
	int ins = mi->mod->ins;
	for (int y = insOffset; y < ins; y++) {
		if (y > (LINES - 3)+insOffset) break;
		wmove(win, y-insOffset, 0);
		wprintw(win, "%02X [", y);
		for (int c = 0; c < mi->mod->chn; c++) {
			struct xmp_channel_info cinfo = fi->channel_info[c];
			int note = (cinfo.note * (COLS - 7)) / 144;
			if (cinfo.instrument != y) continue;
			wmove(win, y-insOffset, note);
			if (cinfo.volume >= 16) wprintw(win, "#");
			else if (cinfo.volume > 0) wprintw(win, "-");
		}
		wmove(win, y-insOffset, COLS-3);
		wprintw(win, "]");
	}
}

char getEffectType(int i) {
	// The effect type characters are so strange to me.
	// They make absolutely no sense in why it's set up this way.
	// Maybe I'm mega stupid right now but this is all of the IT
	// formats effects, so maybe this will cover everything enough.
	// Broken, but not crashy-broken anymore.
	switch(i) {
		case 1: return 'F';
		case 2: return 'E';
		case 3: return 'G';
		case 4: return 'H';
		case 5: return 'L';
		case 6: return 'K';
		case 7: return 'R';
		case 8: return 'X';
		case 9: return 'O';
		case 10: return 'D';
		case 11: return 'B';
		case 16: return 'V';
		case 17: return 'W';
		case 27: return 'Q';
		case 29: return 'I';
		case 128: return 'M';
		case 129: return 'N';
		case 132: return 'Z';
		case 135: return 'T';
		case 137: return 'P';
		case 138: return 'Y';
		case 142: return 'C';
		case 163: return 'A';
		case 172: return 'U';
		case 180: return 'J';
		default: return 0x00;
	}
}