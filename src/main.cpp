#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>
#include <iostream>
#include <cstdlib>

#include <alsa/asoundlib.h>
#include <xmp.h>
#include <ncurses.h>

#define SAMPLERATE 48000
#define BUFFER_SIZE 250000

static char *note_name[] = { "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };

static char *device = "default";
int main(int argc, char *argv[]) {
	printf("Trakker %s (with libxmp %s)\n", "v0.0.1", xmp_version);
	int adv, time, key, err, row, pos, chanoffset;
	bool looper = false;
	bool is_stopped = false;
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
	} else { printf("Color changing not supported!"); }
	
	init_pair(1, COLOR_WHITE, COLOR_BLUE); // PLAYHEAD
	
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
//	printf("%s (%s)\n", mi.mod->name, mi.mod->type);
	row = pos = -1;
	chanoffset = 0;
	xmp_start_player(c, SAMPLERATE, 0);
//	show_data(&mi);
	
// 	for (int x = 0; x < sizeof(mi.mod->xxo); x++) {
// 		std::cout << std::hex << (int)mi.mod->xxo[x] << ",";
// 	}
// 	std::cout << std::endl;
	int vchanoff = 0, vadv = 0;
	adv = 0;
	while (true) {
		int vol = xmp_get_player(c, XMP_PLAYER_VOLUME);
		mvprintw(0, 0, "%s %s (%s)", is_stopped?"[PAUSED]":"", mi.mod->name, mi.mod->type);
		int exa = 7+(adv*4);
		
		xmp_get_frame_info(c, &fi);
		if (xmp_play_frame(c) != 0 && !is_stopped) break;
		mvprintw(
			0, COLS-11, 
			"%02u:%02u/%02u:%02u", 
			((fi.time / 1000) / 60) % 60, 
			(fi.time / 1000) % 60, 
			((fi.total_time / 1000) / 60) % 60, 
			(fi.total_time / 1000) % 60
		);
		
		wtimeout(win, 0);
		if ((key = wgetch(win)) != 0) {
			switch (key) {
				case ' ': // Pause/Play
					time = fi.time;
					is_stopped = !is_stopped;
					while(is_stopped) {
						if ((key = wgetch(win)) == ' ') 
							is_stopped = !is_stopped;
					}
					break;
				case KEY_LEFT: // Move Channels Left
					if (chanoffset > 0) chanoffset--;
					break;
				case KEY_RIGHT: // Move Channels Right
					if (chanoffset < mi.mod->chn-1) chanoffset++;
					break;
				case KEY_UP: // Seek Up
					//xmp_set_position(c, fi.pos-1);
					fi.row--;
					break;
				case KEY_DOWN: // Seek Down
					//xmp_set_position(c, fi.pos+1);
					fi.row++;
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
				case '1': adv = 0; break;
				case '2': adv = 1; break;
				case '3': adv = 2; break;
				case '4': adv = 3; break;
			};
		}
		
		if (!looper && fi.loop_count > 0) break;

		if (!is_stopped) {
			frames = snd_pcm_bytes_to_frames(handle, fi.buffer_size);
			if (snd_pcm_writei(handle, fi.buffer, frames) < 0) {
				snd_pcm_prepare(handle);
			}
		}
		
 		if (fi.pos != pos) {
 			pos = fi.pos;
 			row = -1;
 		}
 		if (!is_stopped && (fi.row != row || vchanoff != chanoffset || vadv != adv)) {
 			mvprintw(
 				LINES-1, 0, 
 				"Pos:%02x Pat:%02x/%02x VOL:%03u%% LOOP:%i +%02u/%02u VIS:%i", 
 				fi.pos, 
 				fi.pattern,
 				mi.mod->pat,
 				vol,
 				looper?1:0,
 				chanoffset,
 				mi.mod->chn-1,
 				adv
 			);
 			werase(win);
 			for (int y = 0; y < LINES - 2; y++) {
                int trow = (fi.row - ((LINES - 2) / 2))+y;
                if (trow > fi.num_rows || trow < 0) { continue;}
                if (trow == fi.row) {
                	wattron(win, COLOR_PAIR(1));
                	wattron(win, A_BOLD);
                } else {
                	wattroff(win, COLOR_PAIR(1));
                	wattroff(win, A_BOLD);
                }
                wmove(win, y, 0);
                wprintw(win, "%02X", trow);
                int coff = ((COLS - 2) % exa);
                int maxcol = -1;
				for (int i = chanoffset; i < mi.mod->chn; i++) {
					if (coff+(((i-chanoffset)+1)*exa)+exa > COLS) {
						maxcol = i;
						break;
					}
					wmove(win, y, coff+((i-chanoffset)*exa)+3);
					int track = mi.mod->xxp[fi.pattern]->index[i];
					struct xmp_event event = mi.mod->xxt[track]->event[trow];
					if (i > 0 && i == chanoffset) wprintw(win, "<");
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
					
					if (adv >= 1) {
						if (event.vol > 0) {
							wprintw(win, " v%02X", event.vol);
						} else {
							wprintw(win, " ---");
						}
					
						if (adv >= 2) {
							if (event.fxt > 0) {
								char t = event.fxt;
								char t2[0];
								if (t >= 0x0f) t = 'F' + (t - 0x0f);
								else { sprintf(t2, "%X", t); t = t2[0]; }
								wprintw(win, " %c%02X", t, event.fxp);
							} else {
								wprintw(win, " ---");
							}
							
							if (adv >= 3) {
								if (event.f2t > 0) {
									char t = event.f2t;
									char t2[0];
									if (t >= 0x0f) t = 'F' + (t - 0x0f);
									else { sprintf(t2, "%X", t); t = t2[0]; }
									wprintw(win, " %c%02X", t, event.f2p);
								} else {
									wprintw(win, " ---");
								}
							}
						}
					}
					vchanoff = chanoffset; vadv = adv;
				}
				if (maxcol < mi.mod->chn && maxcol > 0) wprintw(win, ">");
				else wprintw(win, "|");
			}
 			row = fi.row;
 		}
 		refresh();
		wrefresh(win);
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
