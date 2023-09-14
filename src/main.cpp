#include <fstream>
#include <iostream>
#include <cstdlib>
#include <stdio.h>

#include <alsa/asoundlib.h>
#include <xmp.h>
#include <ncurses.h>
#include "trakker_version.h"

#define SAMPLERATE 48000
#define BUFFER_SIZE 250000

static char *note_name[] = { "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };
static char* pages[] = { "[1] Info", "[2] Pattern", "[3] Channel Bars", "[4] Piano Roll", "[5] About" };
static char* device = "default";

int display = 0;
int mode = 0;
int vol;
int hMin = 0; int hMax = 2048;
int hOffset = 0;
int vMin = 0; int vMax = 2048;
int vOffset = 0;
int looped = 0;
bool stopped;
bool loop;

WINDOW *dis;
WINDOW *tab;
void destroyWindows();
void createWindows();
void renderInfo(xmp_module_info *mi, xmp_frame_info *fi);
void renderAbout();
void renderTrack(xmp_module_info *mi, xmp_frame_info *fi);
void renderRows(xmp_module_info *mi, xmp_frame_info *fi);
void renderChannels(xmp_module_info *mi, xmp_frame_info *fi);
void renderInstruments(xmp_module_info *mi, xmp_frame_info *fi);

char getEffectType(int i);
int main(int argc, char *argv[]) {
    int err;
    snd_pcm_t *handle;
	snd_pcm_sframes_t frames;
	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	if ((err = snd_pcm_set_params(handle, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, SAMPLERATE, 1, BUFFER_SIZE)) < 0) {
		fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	printf("%s initialized.\n", "ALSA");
	
	xmp_context xc;
	xc = xmp_create_context();
	if (xmp_load_module(xc, argv[1]) != 0) {
		fprintf(stderr, "Failed to load Module: %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}
	
	struct xmp_module_info xmi;
	struct xmp_frame_info xfi;
	
	printf("Loading Module: \"%s\"\n", argv[1]);
	
	initscr();
	
	if (has_colors() == TRUE) {
		start_color();
		if (can_change_color() == TRUE) {	
	   		// Primary Background
	   		init_pair(1, COLOR_BLACK, 8);
	   		// Inactive Section
	   		init_pair(2, 8, 7);
	   		// Active Section
	   		init_pair(3, 0, 12);
	   		// Display
	   		init_pair(4, 7, COLOR_BLACK);
	   		// Display Row#
	   		init_pair(5, 3, COLOR_BLACK);
	   		// Display Playhead
	   		init_pair(6, COLOR_WHITE, COLOR_BLUE);
	   		// Display Playhead Row#
	   		init_pair(7, COLOR_YELLOW, COLOR_BLUE);
	   		// Display Stopped Playhead
	   		init_pair(8, COLOR_WHITE, COLOR_RED);
	   		// Display Stopped Playhead Row#
	   		init_pair(9, COLOR_YELLOW, COLOR_RED);
		} else {
			// Primary Background
	   		init_pair(1, COLOR_BLACK, 8);
	   		// Inactive Section
	   		init_pair(2, COLOR_BLACK, COLOR_WHITE);
	   		// Active Section
	   		init_pair(3, COLOR_BLACK, COLOR_CYAN);
	   		// Display
	   		init_pair(4, COLOR_WHITE, COLOR_BLACK);
	   		// Display Row#
	   		init_pair(5, COLOR_YELLOW, COLOR_BLACK);
	   		// Display Playhead
	   		init_pair(6, COLOR_WHITE, COLOR_BLUE);
	   		// Display Playhead Row#
	   		init_pair(7, COLOR_YELLOW, COLOR_BLUE);
	   		// Display Stopped Playhead
	   		init_pair(8, COLOR_WHITE, COLOR_RED);
	   		// Display Stopped Playhead Row#
	   		init_pair(9, COLOR_YELLOW, COLOR_RED);
		}
	} else {
	    // Primary Background
	   init_pair(1, COLOR_BLACK, COLOR_BLACK);
	   // Inactive Section
	   init_pair(2, COLOR_WHITE, COLOR_BLACK);
	   // Active Section
	   init_pair(3, COLOR_BLACK, COLOR_WHITE);
	   // Display
	   init_pair(4, COLOR_WHITE, COLOR_BLACK);
	   // Display Row#
	   init_pair(5, COLOR_WHITE, COLOR_BLACK);
	   // Display Playhead
	   init_pair(6, COLOR_BLACK, COLOR_WHITE);
	   // Display Playhead Row#
	   init_pair(7, COLOR_BLACK, COLOR_WHITE);
	   // Display Stopped Playhead
	   init_pair(8, COLOR_WHITE, COLOR_BLACK);
	   // Display Stopped Playhead Row#
	   init_pair(9, COLOR_WHITE, COLOR_BLACK);
	}
	
	cbreak();
	noecho();
	curs_set(0);
	bkgd(COLOR_PAIR(1));
	refresh();
	
	createWindows();
	
	keypad(stdscr, TRUE);
	
	int row, pos;
	xmp_get_module_info(xc, &xmi);
	row = pos = -1;
	xmp_start_player(xc, SAMPLERATE, 0);
	
	int key;
	bool displayChanged;
	display = 0; displayChanged = true;
	while (true) {
	    xmp_get_frame_info(xc, &xfi);
		if (xmp_play_frame(xc) != 0 && !stopped) break;
		if (xfi.loop_count > looped && !loop) break;
	    else looped = xfi.loop_count;
	    
	    keys:
	    timeout(stopped?-1:0);
		if ((key = getch()) != 0) {
			vol = xmp_get_player(xc, XMP_PLAYER_VOLUME);
			switch (key) {
				case KEY_RESIZE:
					destroyWindows();
					createWindows();
					displayChanged = true; // Update top section.
					break;
				case 'q':
					goto end; break;
				case KEY_LEFT: // Move Channels Left
					if (hOffset > hMin) hOffset--;
					break;
				case KEY_RIGHT: // Move Channels Right
					if (hOffset < hMax) hOffset++;
					break;
				case KEY_UP: // Seek Up
					if (vOffset > vMin) vOffset--;
					break;
				case KEY_DOWN: // Seek Down
					if (vOffset < vMax) vOffset++;
					break;
				case 10:
					hOffset = 0;
					vOffset = 0;
					break;
				case ' ': // Pause/Play
					xfi.time-=20;
					stopped = !stopped; 
					break;
				case '.':
					if (vol < 200) xmp_set_player(xc, XMP_PLAYER_VOLUME, vol+=5);
					break;
				case ',':
					if (vol > 0) xmp_set_player(xc, XMP_PLAYER_VOLUME, vol-=5);
					break;
				case 'l':
					loop = !loop;
					break;
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
					display = key-49;
					displayChanged = true;
					break;
			};
			renderTrack(&xmi, &xfi);
		}
	    
	    if (displayChanged) {
	    	werase(dis);
	    	werase(tab);
	        hOffset = 0;
			vOffset = 0;
	        move(0, 0);
	        for (int d = 0; d < sizeof(pages)/sizeof(*pages); d++) {
	            printw(" ");
	            chtype tpair;
	            if (display == d) tpair = COLOR_PAIR(3);
	            else tpair = COLOR_PAIR(2);
	            attron(tpair);
	            printw(" "); printw(pages[d]); printw(" ");
	            attroff(tpair);
	            printw(" ");
	        }
            mode = 0;
            wmove(tab, 0, 0);
            displayChanged = false;
	    }
	    
		if (!stopped) {
			frames = snd_pcm_bytes_to_frames(handle, xfi.buffer_size);
			if (snd_pcm_writei(handle, xfi.buffer, frames) < 0) {
				snd_pcm_prepare(handle);
			}
		
			if (xfi.pos != pos) {
				pos = xfi.pos;
				row = -1;
			}
			if (xfi.row != row) {
				row = xfi.row;
			}
			renderTrack(&xmi, &xfi);
			wrefresh(tab);
			wrefresh(dis);
 		} else goto keys;
	}
	end:
	clrtoeol();
	refresh();
	endwin();
	xmp_end_player(xc);
	xmp_release_module(xc);
    xmp_free_context(xc);
	if ((err = snd_pcm_drain(handle)) < 0) {
		printf("snd_pcm_drain failed: %s\n", snd_strerror(err));
	}
	snd_pcm_close(handle);
	printf("Goodbye!\n");
	return 0;
}

void destroyWindows() {
	delwin(tab);
	delwin(dis);
}

void createWindows() {
	tab = newwin(LINES-1, COLS, 1, 0);
	wmove(tab, 0, 0);
	wprintw(tab, "");
	wbkgd(tab, COLOR_PAIR(3));
	wrefresh(tab);
	
	dis = newwin(LINES-4, COLS-2, 3, 1);
	wmove(dis, 0, 0);
	wprintw(dis, "");
	wbkgd(dis, COLOR_PAIR(4));
	wrefresh(dis);
}

void renderTrack(xmp_module_info *mi, xmp_frame_info *fi) {
	werase(dis);
	mvwprintw(tab, 0, 1, mi->mod->name);
    mvwprintw(
    	tab, 
		0, COLS-12, 
		"%02u:%02u/%02u:%02u", 
		((fi->time / 1000) / 60) % 60, 
		(fi->time / 1000) % 60, 
		((fi->total_time / 1000) / 60) % 60, 
		(fi->total_time / 1000) % 60
	);
	mvwprintw(tab, 1, COLS-10, "VOL: %i%%", vol);
	mvwprintw(tab, 1, 1, "%i/%ibpm", fi->speed, fi->bpm);
	mvwprintw(tab, LINES-2, (COLS/2)-4, stopped?"STOPPED":"PLAYING");
	if (display == 0) {
		renderInfo(mi, fi);
	} else if (display == 1) {
		renderRows(mi, fi);
	} else if (display == 2) {
		renderChannels(mi, fi);
	} else if (display == 3) {
		renderInstruments(mi, fi);
	} else {
		renderAbout();
	}
	refresh();
	wrefresh(dis);
	wrefresh(tab);
}

void renderInfo(xmp_module_info *mi, xmp_frame_info *fi) {
	wattron(dis, A_BOLD);
	mvwprintw(dis, 3-vOffset, 0, "Format:");
	mvwprintw(dis, 4-vOffset, 0, "Instruments:");
	mvwprintw(dis, 5-vOffset, 0, "Channels:");
	mvwprintw(dis, 6-vOffset, 0, "Looping:");
	mvwprintw(dis, 8-vOffset, 0, "Comments:");
	wattroff(dis, A_BOLD);
	
	mvwprintw(dis, 3-vOffset, 16, mi->mod->type);
	mvwprintw(dis, 4-vOffset, 16, "%i", mi->mod->ins);
	mvwprintw(dis, 5-vOffset, 16, "%i", mi->mod->chn);
	mvwprintw(dis, 6-vOffset, 16, loop?"YES":"NO");
	
	if (mi->comment != NULL) {
		mvwprintw(dis, 9-vOffset, 0, "%s", mi->comment);
	}
}

void renderAbout() {
	wattron(dis, A_BOLD);
	mvwprintw(dis, 1-vOffset, 2, "========            ||  //  ||  //");
	mvwprintw(dis, 2-vOffset, 2, "   ||               || //   || //");
	mvwprintw(dis, 3-vOffset, 2, "   ||               ||//    ||//");
	mvwprintw(dis, 4-vOffset, 2, "   || //==\\\\ //===|| ||\\\\    ||\\\\   //===\\\\ //===\\\\");
	mvwprintw(dis, 5-vOffset, 2, "   || ||    ||    |  || \\\\   || \\\\  ||===// ||");
	mvwprintw(dis, 6-vOffset, 2, "   || ||     \\\\===|| ||  \\\\  ||  \\\\ \\\\___/  ||");
	mvwprintw(dis, 8-vOffset, 1, "TRAKKER    v%s", TRAKKER_VERSION);
	mvwprintw(dis, 9-vOffset, 1, "libXMP     v%s", xmp_version);
	
	mvwprintw(dis, 11-vOffset, 1, "[Spacebar]");
	mvwprintw(dis, 12-vOffset, 1, "Number Keys");
	mvwprintw(dis, 13-vOffset, 1, "Arrow Keys");
	mvwprintw(dis, 14-vOffset, 1, "[,] and [.]");
	mvwprintw(dis, 15-vOffset, 1, "[Return]");
	mvwprintw(dis, 16-vOffset, 1, "[L]");
	wattroff(dis, A_BOLD);
	
	mvwprintw(dis, 11-vOffset, 16, "Play/Pause");
	mvwprintw(dis, 12-vOffset, 16, "Change Tab");
	mvwprintw(dis, 13-vOffset, 16, "Change Hori or Vert Display Offset");
	mvwprintw(dis, 14-vOffset, 16, "Control volume");
	mvwprintw(dis, 15-vOffset, 16, "Reset Display");
	mvwprintw(dis, 16-vOffset, 16, "Toggle Loop");
}

void renderRows(xmp_module_info *mi, xmp_frame_info *fi) {
	int chnsize = 15;
	for (int j = 0; j < mi->mod->len; j++) {
		if (mi->mod->xxo[j] == 0xFF) continue;
		chtype patpair = (j == fi->pos)?COLOR_PAIR(6):COLOR_PAIR(4);
		wattron(dis, patpair);
		mvwprintw(dis, LINES-5, (COLS/2)+(j*3)-(fi->pos*3), "%02X", mi->mod->xxo[j]);
		wattroff(dis, patpair);
	}
	wattroff(tab, COLOR_PAIR(5));
	for (int y = 0; y < LINES - 5; y++) {
		int trow = (fi->row - ((LINES - 2) / 2))+y;
		if (trow > fi->num_rows-1 || trow < 0) { continue; }
		chtype numpair = COLOR_PAIR((trow==fi->row)?(stopped?9:7):5);
		chtype rowpair = COLOR_PAIR((trow==fi->row)?(stopped?8:6):4);
		if (trow == fi->row) wattron(dis, A_BOLD);
		wmove(dis, y, 0);
		wattron(dis, numpair);
		wprintw(dis, "%02X", trow);
		wattroff(dis, numpair);
		wattron(dis, rowpair);
		for (int i = 0; i < mi->mod->chn+1; i++) {
			if (i >= mi->mod->chn) {
				mvwaddch(dis, y, ((i*chnsize)+2)-hOffset, '|');
				break;
			}
			int track = mi->mod->xxp[fi->pattern]->index[i];
			struct xmp_event event = mi->mod->xxt[track]->event[trow];
			char *lnbuf = new char[chnsize+1];
			char *note = new char[4];
			char *ins = new char[3];
			char *vol = new char[4];
			char *efx = new char[4];
			
			if (event.note > 0x80)
				snprintf(note, 4, "===");
			else if (event.note > 0)
				snprintf(note, 4, "%s%d", note_name[event.note % 12], event.note / 12);
			else
				snprintf(note, 4, "...");
			
			if (event.ins > 0) snprintf(ins, 3, "%02X", event.ins);
			else snprintf(ins, 3, "..");
			
			if (event.vol != 0)
				snprintf(vol, 4, "v%02X", event.vol-1);
			else if (event.note != 0)
				snprintf(vol, 4, "v40");
			else snprintf(vol, 4, "...");
				
			char f1;
			if ((f1 = getEffectType(event.fxt)) != 0) snprintf(efx, 4, "%c%02X", f1, event.fxp);
			else snprintf(efx, 4, "...");
			sprintf(lnbuf, "|%s %s %s %s", note, ins, vol, efx);
			for (int z = 0; z < chnsize; z++) {
				if (((i*chnsize)+2+z)-hOffset < 2) continue;
				mvwaddch(dis, y, ((i*chnsize)+2+z)-hOffset, lnbuf[z]);
			}
			free(lnbuf);
			free(note);
			free(ins);
			free(vol);
			free(efx);
		}
		wattroff(dis, rowpair);
		if (trow == fi->row) wattroff(dis, A_BOLD);
	}
}

void renderChannels(xmp_module_info *mi, xmp_frame_info *fi) {
	int chns = mi->mod->chn;
	chtype no_pair = COLOR_PAIR(5);
	for (int y = vOffset; y < chns; y++) {
		if (y > (LINES - 4)+vOffset || y < 0) continue;
		struct xmp_channel_info cinfo = fi->channel_info[y];
		if (y > (LINES - 3)+vOffset) break;
		wmove(dis, y-vOffset, 0);
		int cvol = (cinfo.volume * (COLS - 5)) / (64 * (vol / 100));
		wattron(dis, no_pair);
		wprintw(dis, "%02X", y);
		wattroff(dis, no_pair);
		for (int c = 0; c < COLS - 5; c++) {
			if (c < cvol) wprintw(dis, "#");
			else wprintw(dis, " ");
		}
	}
}

void renderInstruments(xmp_module_info *mi, xmp_frame_info *fi) {
	int ins = mi->mod->ins;
	chtype no_pair = COLOR_PAIR(5);
	for (int y = vOffset; y < ins; y++) {
		if (y > (LINES - 5)+vOffset || y < 0) continue;
		wmove(dis, y-vOffset, 0);
		wattron(dis, no_pair);
		wprintw(dis, "%02X", y);
		wattroff(dis, no_pair);
		for (int c = 0; c < mi->mod->chn; c++) {
			struct xmp_channel_info cinfo = fi->channel_info[c];
			int note = (cinfo.note * (COLS - 4)) / 144;
			if (cinfo.instrument != y) continue;
			wmove(dis, y-vOffset, note+3);
			if (cinfo.volume >= 32) wprintw(dis, "#");
			else if (cinfo.volume >= 16) wprintw(dis, "=");
			else if (cinfo.volume > 0) wprintw(dis, "-");
		}
		wmove(dis, y, COLS-4);
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