#include <fstream>
#include <iostream>
#include <cstdlib>
#include <stdio.h>
#include <map>

#include <alsa/asoundlib.h>
#include <xmp.h>
#include <ncurses.h>
#include "trakker.h"
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
std::map<int, char> efxtable;
std::map<int, bool> efxmemtable;

WINDOW *dis;
WINDOW *tab;

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
	} else printf("Loaded Module: \"%s\"\n", argv[1]);
	
	struct xmp_module_info xmi;
	struct xmp_frame_info xfi;
	
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
	generateEffectsTable(xmi.mod->type);
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
				case KEY_PPAGE:
					if (vOffset > vMin + LINES-6) vOffset-=(LINES-5);
					else vOffset = vMin;
					break;
				case KEY_NPAGE:
					if (vOffset < vMax - LINES-6) vOffset+=(LINES-5);
					else vOffset = vMax;
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
				renderTrack(&xmi, &xfi);
			}
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
	wclrtoeol(tab);
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
	mvwprintw(tab, LINES-2, (COLS/2)-4, "%s%s", stopped?"STOPPED":"PLAYING", loop?" [L]":"");
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
	mvwprintw(dis, 1-vOffset, 1, "Format:");
	mvwprintw(dis, 2-vOffset, 1, "Channels:");
	mvwprintw(dis, 3-vOffset, 1, "Looping:");
	mvwprintw(dis, 5-vOffset, 1, "Instruments:");
	mvwprintw(dis, (7+mi->mod->ins)-vOffset, 1, "Samples:");
	wattroff(dis, A_BOLD);
	
	mvwprintw(dis, 1-vOffset, 16, mi->mod->type);
	mvwprintw(dis, 2-vOffset, 16, "%i", mi->mod->chn);
	mvwprintw(dis, 3-vOffset, 16, loop?"YES":"NO");
	mvwprintw(dis, 5-vOffset, 16, "%i", mi->mod->ins);
	mvwprintw(dis, (7+mi->mod->ins)-vOffset, 16, "%i", mi->mod->smp);
	
	for (int xi = 0; xi < mi->mod->ins; xi++) {
		mvwprintw(dis, xi+6-vOffset, 1, "[%02X] %s", xi, mi->mod->xxi[xi].name);
	}
	for (int xs = 0; xs < mi->mod->smp; xs++) {
		mvwprintw(dis, xs+(8+mi->mod->ins)-vOffset, 1, "[%02X] %s", xs, mi->mod->xxs[xs].name);
	}
}

void renderAbout() {
	wattron(dis, A_BOLD);
    mvwprintw(dis, 1-vOffset, 2, "######## ||##\\\\   //\\\\  || // || // ||### ||##\\\\");
    mvwprintw(dis, 2-vOffset, 2, "   ||    ||   || //  \\\\ ||//  ||//  ||    ||  ||");
    mvwprintw(dis, 3-vOffset, 2, "   ||    ||##//  ||##|| |#|   |#|   ||#   ||##//");
    mvwprintw(dis, 4-vOffset, 2, "   ||    ||\\\\    ||  || ||\\\\  ||\\\\  ||    ||\\\\  ");
    mvwprintw(dis, 5-vOffset, 2, "   ||    || \\\\   ||  || || \\\\ || \\\\ ||### || \\\\ ");
    mvwprintw(dis, 6-vOffset, 2, "=================================================");
	mvwprintw(dis, 8-vOffset, 1, "TRAKKER		v%s", TRAKKER_VERSION);
	mvwprintw(dis, 9-vOffset, 1, "libXMP		 v%s", xmp_version);
	
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
			if ((efxmemtable[event.fxt] || event.fxp != 0) && (f1 = efxtable[event.fxt]) != NULL) snprintf(efx, 4, "%c%02X", f1, event.fxp);
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

void generateEffectsTable(char* type) {
	if (isPartOf(type, "669")) {
		addToEffects(96, 'A', true);
		addToEffects(97, 'B', true);
		addToEffects(98, 'C', true);
		addToEffects(99, 'D', true);
		addToEffects(100, 'E', true);
		addToEffects(126, 'F', true);
	} else if (isPartOf(type, "Farandole")) {
		addToEffects(249, '1', true);
		addToEffects(248, '2', true);
		addToEffects(122, '3', true);
		addToEffects(251, '4', true);
		addToEffects(254, '5', true);
		addToEffects(4, '6', true);
		addToEffects(256, '7', true);
		addToEffects(252, '8', true);
		addToEffects(123, '9', true);
		addToEffects(250, 'C', true);
		addToEffects(15, 'F', true);
	} else if (isPartOf(type, "Imago Orpheus")) {
		addToEffects(1, '1', true);
		addToEffects(2, '2', true);
		addToEffects(3, '3', true);
		addToEffects(4, '4', true);
		addToEffects(5, '5', true);
		addToEffects(6, '6', true);
		addToEffects(7, '7', true);
		addToEffects(8, '8', true);
		addToEffects(9, '9', true);
		addToEffects(10, 'A', true);
		addToEffects(11, 'B', true);
		addToEffects(12, 'C', true);
		addToEffects(13, 'D', true);
		addToEffects(14, 'E', true);
		addToEffects(15, 'F', true);
		addToEffects(16, 'G', true);
		addToEffects(17, 'H', true);
		addToEffects(18, 'I', true);
		addToEffects(19, 'J', true);
		addToEffects(20, 'K', true);
		addToEffects(21, 'L', true);
		addToEffects(22, 'M', true);
		addToEffects(23, 'N', true);
		addToEffects(24, 'O', true);
		addToEffects(25, 'P', true);
		addToEffects(26, 'Q', true);
		addToEffects(27, 'R', true);
		addToEffects(28, 'S', true);
		addToEffects(29, 'T', true);
		addToEffects(30, 'U', true);
		addToEffects(31, 'V', true);
		addToEffects(32, 'W', true);
		addToEffects(33, 'X', true);
		addToEffects(34, 'Y', true);
		addToEffects(35, 'Z', true);
	} else if (isPartOf(type, "S3M")) {
		addToEffects(163, 'A', false);
		addToEffects(11, 'B', true);
		addToEffects(13, 'C', true);
		addToEffects(10, 'D', true);
		addToEffects(2, 'E', true);
		addToEffects(1, 'F', true);
		addToEffects(3, 'G', true);
		addToEffects(4, 'H', true);
		addToEffects(29, 'I', true);
		addToEffects(180, 'J', true);
		addToEffects(6, 'K', true);
		addToEffects(5, 'L', true);
		addToEffects(9, 'O', true);
		addToEffects(27, 'Q', true);
		addToEffects(7, 'R', true);
		addToEffects(254, 'S', true);
		addToEffects(171, 'T', false);
		addToEffects(172, 'U', true);
		addToEffects(16, 'V', true);
		addToEffects(8, 'X', true);
		addToEffects(141, 'X', true);
		addToEffects(14, 'S', true);
	} else if (isPartOf(type, "IT")) {
		addToEffects(163, 'A', false);
		addToEffects(11, 'B', true);
		addToEffects(142, 'C', true);
		addToEffects(10, 'D', true);
		addToEffects(2, 'E', true);
		addToEffects(1, 'F', true);
		addToEffects(3, 'G', true);
		addToEffects(4, 'H', true);
		addToEffects(29, 'I', true);
		addToEffects(180, 'J', true);
		addToEffects(6, 'K', true);
		addToEffects(5, 'L', true);
		addToEffects(128, 'M', true);
		addToEffects(129, 'N', true);
		addToEffects(9, 'O', false); // OpenMPT doc states this should be true but there is zero effect at 00 (0*256=0)
		addToEffects(137, 'P', true);
		addToEffects(27, 'Q', true);
		addToEffects(7, 'R', true);
		addToEffects(254, 'S', true);
		addToEffects(135, 'T', false);
		addToEffects(172, 'U', true);
		addToEffects(16, 'V', true);
		addToEffects(17, 'W', true);
		addToEffects(8, 'X', true);
		addToEffects(138, 'Y', true);
		addToEffects(141, 'S', true);
		addToEffects(136, 'S', true);
		addToEffects(14, 'S', true);
		addToEffects(192, 'c', true);
		addToEffects(193, 'd', true);
		addToEffects(194, 'a', true);
		addToEffects(195, 'b', true);
		addToEffects(132, 'S', true);
		addToEffects(139, 'S', true);
		addToEffects(140, 'S', true);
	} else if (isPartOf(type, "LIQ")) {
		addToEffects(0, 'A', true);
		addToEffects(171, 'B', true);
		addToEffects(13, 'C', true);
		addToEffects(2, 'D', true);
		addToEffects(172, 'F', true);
		addToEffects(11, 'J', true);
		addToEffects(10, 'L', true);
		addToEffects(14, 'M', true);
		addToEffects(3, 'N', true);
		addToEffects(9, 'O', true);
		addToEffects(163, 'S', true);
		addToEffects(7, 'T', true);
		addToEffects(1, 'U', true);
		addToEffects(4, 'V', true);
		addToEffects(5, 'X', true);
		addToEffects(6, 'Y', true);
	} else if (isPartOf(type, "Oktalyzer")) {
		addToEffects(1, '1', true);
		addToEffects(2, '2', true);
		addToEffects(112, '0', true);
		addToEffects(113, '0', true);
		addToEffects(114, '0', true);
		addToEffects(115, '6', true);
		addToEffects(116, '5', true);
		addToEffects(156, '6', true);
		addToEffects(11, 'B', true);
		addToEffects(15, 'F', true);
		addToEffects(157, '5', true);
		addToEffects(12, 'C', true);
		addToEffects(10, 'A', true);
		addToEffects(174, 'E', true);
		addToEffects(17, 'E', true);
		addToEffects(0, '0', true);
	} else if (isPartOf(type, "STX")) {
		addToEffects(15, 'A', true);
		addToEffects(11, 'B', true);
		addToEffects(13, 'C', true);
		addToEffects(10, 'D', true);
		addToEffects(2, 'E', true);
		addToEffects(1, 'F', true);
		addToEffects(3, 'G', true);
		addToEffects(4, 'H', true);
		addToEffects(29, 'I', true);
		addToEffects(0, 'J', true);
	} else if (isPartOf(type, "Funk")) {
		addToEffects(121, 'A', true);
		addToEffects(120, 'B', true);
		addToEffects(122, 'C', true);
		addToEffects(123, 'D', true);
		addToEffects(124, 'G', true);
		addToEffects(125, 'H', true);
		addToEffects(0, 'L', true);
		addToEffects(12, 'N', true);
		addToEffects(127, 'O', true);
		addToEffects(14, 'O', true);
		addToEffects(15, 'O', true);
	} else {
		addToEffects(0, '0', false);
		addToEffects(1, '1', false);
		addToEffects(2, '2', false);
		addToEffects(3, '3', true);
		addToEffects(4, '4', true);
		addToEffects(5, '5', false);
		addToEffects(6, '6', false);
		addToEffects(7, '7', true);
		addToEffects(8, '8', true);
		addToEffects(9, '9', true);
		addToEffects(10, 'A', false);
		addToEffects(11, 'B', true);
		addToEffects(12, 'C', true);
		addToEffects(13, 'D', true);
		addToEffects(14, 'E', false);
		addToEffects(15, 'F', true);
		addToEffects(16, 'G', true);
		addToEffects(27, 'Q', true);
		addToEffects(181, 'P', true);
		addToEffects(17, 'H', true);
		addToEffects(21, 'L', true);
		addToEffects(164, 'c', true);
		addToEffects(33, 'X', true);
		addToEffects(20, 'K', true);
		addToEffects(25, 'P', true);
		addToEffects(29, 'T', true);
		addToEffects(146, '4', true);
		addToEffects(160, 'x', true);
		addToEffects(161, 'x', true);
		addToEffects(171, 'F', true);
	}
}

// We do not care for previous effects (memory = true) because it's a tracker.
// 'mem' is only for discarding zero values.
void addToEffects(int id, char efx, bool mem) {
	efxtable[id] = efx;
	efxmemtable[id] = mem;
}

bool isPartOf(char* w1, char* w2) {
	int i=0;
	int j=0;
	while(w1[i]!='\0'){
		if(w1[i] == w2[j]) {
			int init = i;
			while (w1[i] == w2[j] && w2[j]!='\0') {
				j++;
				i++;
			}
			if(w2[j]=='\0') {
				return true;
			}
			j=0;
		}
		i++;
	}
	return false;
}