/*********************************************
***    USBDMX.COM CURSES SIMPLE PROGRAM    ***
*** http://sourceforge.net/p/usbdmxcurses/ ***
***    Author: cLx <clx@sdf.lonestar.org>  ***
***   Licence:   GNU PUBLIC LICENCE v3     ***
***                                        ***
***    Wanna see my rebuilt DMX adapter?   ***
***  http://clx.freeshell.org/usbdmx.html  ***
*********************************************/


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <curses.h>

// **** curses code section ****
// For Debian: # apt-get install ncurses-dev

#define maxrows LINES
#define maxcols COLS

#ifndef bool
#define bool int
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

void destroy_win(WINDOW *lwin){
	wborder(lwin, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
	wrefresh(lwin);
	delwin(lwin);
}

typedef struct {
	WINDOW *decoration;
	WINDOW *content;
	int drows;
	int dcols;
	int crows;
	int ccols;
} dwin;
dwin rxw, txw, msgw;

void create_dwin(dwin *w, int rows, int cols, int startrow, int startcol, const char *title){
	w->decoration = newwin(               rows,   cols,   startrow,   startcol);
	w->content    = subwin(w->decoration, rows-2, cols-4, startrow+1, startcol+2);
	w->drows = rows;     w->dcols = cols;
	w->crows = rows - 2; w->ccols = cols - 4;
	box(w->decoration, 0, 0);
	if (title) {
		mvwprintw(w->decoration, 0, (cols-(int)strlen(title)-4), " %s ", title);
	}
	wrefresh(w->decoration);
}

void destrow_dwin(dwin *w){
	if (w->content)    { destroy_win(w->content); }
	if (w->decoration) { destroy_win(w->decoration); }
}

void display_init(void){
	initscr(); // start curses mode
	cbreak();  // line input buffering disabled ("raw" mode)
	keypad(stdscr, TRUE); // I need that nifty F1 ?
	noecho();  // curses call set to no echoing
	refresh(); // this one made me scratching my head

	msgw.drows = maxrows/4;
	if (msgw.drows < 3) { msgw.drows = 0; }
	msgw.dcols = maxcols;

	txw.drows  = (maxrows-msgw.drows-1)/2;             txw.dcols  = maxcols;
	rxw.drows  = (maxrows-msgw.drows-txw.drows-1);     rxw.dcols  = maxcols;

	create_dwin(&rxw,  rxw.drows,  rxw.dcols,  0,                   0, "DMX RX Signal");
	create_dwin(&txw,  txw.drows,  txw.dcols,  rxw.drows,           0, "DMX TX Signal");
	create_dwin(&msgw, msgw.drows, msgw.dcols, rxw.drows+txw.drows, 0, "Status Messages");
	scrollok(msgw.content, TRUE);
	meta(msgw.content, TRUE);
	wtimeout(msgw.content, 100);
	{
		int i;
		for(i=0; i<msgw.drows; i++){
			wprintw(msgw.content, "\n");
		}
	}
	keypad(msgw.content, TRUE);
}

unsigned char rxc[512], txc[512];
unsigned int selected_channel = 0, display_start_channel = 0;

struct tstatus {
	bool tx;
	bool rxenabled;
	bool rxreceiving;
	bool txvaluespolling;
	int  txvaluespolling_channel;
	bool txvaluespolling_atrun;
	bool blackout;
};
struct tstatus status;

void statusbar(void){
	wmove(stdscr, maxrows-1, 0);
	if (status.tx){
		attron (A_REVERSE); wprintw(stdscr, "TX ACTIVE");
		attroff(A_REVERSE); wprintw(stdscr, " ");
	}
	if (status.rxenabled){
		attron (A_REVERSE); wprintw(stdscr, "RX ENABLED");
		attroff(A_REVERSE); wprintw(stdscr, " ");
	}
	if (status.rxreceiving){
		attron (A_REVERSE); wprintw(stdscr, "RECEIVING DMX");
		attroff(A_REVERSE); wprintw(stdscr, " ");
	}
	if (status.blackout){
		attron (A_REVERSE); wprintw(stdscr, "BLACKOUT");
		attroff(A_REVERSE); wprintw(stdscr, " ");
	}
	if (status.txvaluespolling){
		attron (A_REVERSE);
		if (status.txvaluespolling_atrun){
			wprintw(stdscr, "INITIAL ");
		}
		wprintw(stdscr, "POLLING TX VALUES");
		attroff(A_REVERSE);
		wprintw(stdscr, " ");
		wtimeout(msgw.content, 3);
	}
	else {
		wtimeout(msgw.content, 100);
	}

	clrtoeol();
	wrefresh(stdscr);
}

void display_message(int nonewline, char *text){
	if (!nonewline) { wprintw(msgw.content, "\n"); }
	wprintw(msgw.content, "%s", text);
	wrefresh(msgw.content);
}

void redraw_channel(dwin *w, unsigned int channel){
	unsigned char *a;
	int n;
	bool selected = FALSE;

	if ((channel == selected_channel) && (w == &txw)) { selected = TRUE; }
	if       (w == &txw) { a = txc; }
	else if  (w == &rxw) { a = rxc; }
	else return;

	if (selected_channel>display_start_channel+maxcols/4-2) { return; }
	if (selected_channel<display_start_channel) { return; }

	for (n=0; n<=((w->crows)-3); n++){
		wmove(w->content, n, (channel-display_start_channel)*4);
		wprintw(w->content, " |  ");
	}
	wmove(w->content, w->crows-1, (channel-display_start_channel)*4);
	if (selected) { wattron (w->content, A_REVERSE); }
	if (channel<10)  { wprintw(w->content, " "); }
	wprintw(w->content, "%u", channel+1);
	if (channel<100) { wprintw(w->content, " "); }
	if (selected) { wattroff(w->content, A_REVERSE); }

	wmove(w->content, ((255-a[channel])*(w->crows-3)/255), (channel-display_start_channel)*4);
	if (selected) { wattron (w->content, A_REVERSE); }
	wprintw(w->content, "===");
	wmove(w->content, w->crows-2, (channel-display_start_channel)*4);
	wprintw(w->content, "%03u", a[channel]);
	if (selected) { wattroff(w->content, A_REVERSE); }
}

void update_channel(dwin *w, unsigned int channel, int value){
	unsigned char *a;
	bool selected = FALSE;

	if ((channel == selected_channel) && (w == &txw)) { selected = TRUE; }
	if       (w == &txw) { a = txc; }
	else if  (w == &rxw) { a = rxc; }
	else return;

	if (value == a[channel]) { return; }

	if ((channel>=display_start_channel) &&
	    (channel<=display_start_channel+(maxcols/4-2))) {

		wmove(w->content, ((255-a[channel])*(w->crows-3)/255),   (channel-display_start_channel)*4);
		wprintw(w->content, " | ");
		wmove(w->content, ((255-value       )*(w->crows-3)/255), (channel-display_start_channel)*4);
		if (selected) { wattron (w->content, A_REVERSE); }
		wprintw(w->content, "===");
		wmove(w->content, w->crows-2, (channel-display_start_channel)*4);
		wprintw(w->content, "%03u", value);
		if (selected) { wattroff(w->content, A_REVERSE); }
		wrefresh(w->content);
	}
	a[channel] = value;
}

int wait;
int fd;

int send1(unsigned char byte){
	unsigned char buf[2];
	buf[0] = byte; buf[1] = 0;
	return write(fd, buf, 1);
}

int send2(unsigned char byte1, unsigned char byte2){
	unsigned char buf[3];
	buf[0] = byte1; buf[1] = byte2; buf[2] = 0;
	return write(fd, buf, 2);
}

int send3(unsigned char byte1, unsigned char byte2, unsigned char byte3){
	unsigned char buf[4];
	buf[0] = byte1; buf[1] = byte2; buf[2] = byte3; buf[3] = 0;
	return write(fd, buf, 3);
}

#define BUFSIZE    1000
unsigned char* getdmxmessages(unsigned char attended_message){
	unsigned char message;
	static unsigned char buf[BUFSIZE+1];
	ssize_t l;
	int loop;
	for (loop=0; loop<100; loop++){
		l = read(fd, &message, 1);
		if (l<=0){ usleep(100); }
		else {
			switch(message){
				case 0xa4: //version request (0x24)
					usleep(200000);
					l = read(fd, buf, BUFSIZE);
					buf[BUFSIZE] = 0;
					wprintw(msgw.content, "\n[v%01u.%01u] %s", buf[0]>>4, buf[0]&0x0f, buf+1);
					wrefresh(msgw.content);
					break;

				case 0xc4: //check tx status (0x50) : is on
					status.tx = TRUE;
					display_message(0, "TX is ACTIVE");
					statusbar();
					break;

				case 0xc6: //check tx status (0x50) : is off
					status.tx = FALSE;
					display_message(0, "TX is NOT ACTIVE!");
					statusbar();
					break;

				case 0xd2: //read tx channel value (0x52)
				case 0xd3: //read tx channel value (0x53)
					usleep(1000);
					l = read(fd, buf, 2);
					break;

				case 0xc9:
					display_message(0, "DMX set channel value ACK (obsolete!)");
					break;

				case 0xca:
					status.blackout = TRUE;
					display_message(0, "Blackout ON!");
					statusbar();
					break;

				case 0xcc:
					status.blackout = FALSE;
					display_message(0, "Blackout OFF");
					statusbar();
					break;

				case 0xce:
					display_message(0, "Last tx code channel setted!");
					break;

				case 0xe4:
					status.rxenabled = TRUE;
					display_message(0, "RX Enabled");
					statusbar();
					break;

				case 0xe6:
					status.rxenabled = FALSE;
					display_message(0, "RX Disabled!");
					statusbar();
					break;

				case 0xea:
					l = read(fd, buf, 1);
					wprintw(msgw.content, "\nInput refresh rate: %u FPS", buf[0]);
					wrefresh(msgw.content);
					break;

				case 0xe8:
				case 0xe9:
					l = read(fd, buf, 1);
					wprintw(msgw.content, "\nLast channel from the last frame received: %3u", (((unsigned int)message&1)<<8)+(unsigned int)buf[0]);
					wrefresh(msgw.content);
					break;

				case 0xec:
					l = read(fd, buf, 1);
					wprintw(msgw.content, "\nLast startcode read in: %3u", buf[0]);
					wrefresh(msgw.content);
					break;

				case 0xf4:
					display_message(0, "DMX monitor not started or no frames received");
					break;

				case 0xf6:
					status.rxreceiving = TRUE;
					display_message(0, "Started receiving DMX");
					statusbar();
					break;

				case 0xf8:
					status.rxreceiving = FALSE;
					display_message(0, "No longer receiving DMX");
					statusbar();
					break;

				case 0xfa:
					display_message(0, "Hardware overflow!");
					break;

				case 0xfc:
					display_message(0, "DMX RX framing error!");
					break;

				case 0xfe:
				case 0xff:
					usleep(1000);
					l = read(fd, buf, 2);
					{
					unsigned int rxchannel, rxvalue;
					rxchannel = ((message&1)<<8)+buf[0];
					rxvalue = buf[1];
					wprintw(msgw.content, "\nRX DMX channel %03u = %3u", rxchannel, rxvalue);
					update_channel(&rxw, rxchannel, rxvalue);
					}
					wrefresh(msgw.content);
					break;

				case 0x84:
					display_message(0, "Unknow command received by the interface!");
					break;

				default:
					wprintw(msgw.content, "\nNot handled message 0x%02X", message);
					wrefresh(msgw.content);
			}
			if (attended_message == message) { return buf; }
		}
	}
	wait=0;
	return NULL;
}

unsigned char read_tx_channel_value(unsigned int channel){
	unsigned char *buf;
	unsigned offset = 0;
	if (channel < 256) {
		send2(0x52, channel);
		buf = getdmxmessages(0xd2);
	}
	else {
		send2(0x53, 255&channel);
		offset=256;
		buf = getdmxmessages(0xd3);
	}
	if (buf) {
		update_channel(&txw, buf[0]+offset, buf[1]);
		if(buf[0] == (255&channel)) { return buf[1]; }
	}
	return 0;
}

void toggle(bool *var){
	if (*var == TRUE) { *var = FALSE; }
	else { *var = TRUE; }
}

void initserial(void){
	struct termios options;
	tcgetattr(fd, &options);
	options.c_oflag = 0;
	options.c_lflag = 0;
	options.c_iflag = 0;
	options.c_cflag = 0;

	cfsetospeed(&options, B115200);
	cfsetispeed(&options, B115200);

	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~CSTOPB; // 1 bit stop
	options.c_cflag &= ~CSIZE; // mask the character size bits
	options.c_cflag |= CS8; // 8 bits
	options.c_cflag &= (~PARENB & ~PARODD); // no parity
	options.c_cc[VMIN] = 1;
	options.c_cc[VTIME] = 0;

	options.c_oflag &= ~OPOST; // raw mode for output

	options.c_iflag |= IGNBRK; // ignore breaks
	options.c_iflag &= ~(IXON | IXOFF | IXANY); // no software flow control
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // raw mode for input

	tcsetattr(fd, TCSANOW, &options);
}

int main(int argc, char** argv){
	if (argc != 2) {
		fprintf(stderr, "Usage: %s /dev/your-usbdmx-tty\n", argv[0]);
		return -1;
	}

	fd = open(argv[1], O_RDWR|O_NOCTTY|O_NDELAY);
	if (fd == -1) {
		perror(argv[1]);
		return -1;
	}
	initserial();
	display_init();
	statusbar();
	display_start_channel = 0;
	status.txvaluespolling_atrun = 1;
	status.txvaluespolling = 1;
	status.txvaluespolling_channel = 0;
	{
	int i;
	for(i=0; i<=maxcols/4-2; i++){
		redraw_channel(&txw, i+display_start_channel);
		redraw_channel(&rxw, i+display_start_channel);
	}}
	wrefresh(rxw.content);
	wrefresh(txw.content);
	send1(0x50); //check tx status
	send1(0x4c); //blackout OFF

	for(;;){
		getdmxmessages(0);

		if (status.txvaluespolling){
			if (status.txvaluespolling_atrun) {
				read_tx_channel_value(status.txvaluespolling_channel);
				if (++status.txvaluespolling_channel>=512) {
					status.txvaluespolling_atrun = 0;
					status.txvaluespolling = 0;
					status.txvaluespolling_channel=0;
					statusbar();
				}
			}
			else {
				read_tx_channel_value(display_start_channel+status.txvaluespolling_channel);
				if (++status.txvaluespolling_channel>=maxcols/4-2) {
					status.txvaluespolling_channel=0;
				}
			}
		}

		{
			int a;
			static int r = 0;
			a = wgetch(msgw.content);
			switch (a){
				case ERR:
					r=0;
					break;

				case KEY_LEFT:
					if (selected_channel>0){
						selected_channel--;
						if (selected_channel<display_start_channel) {
							display_start_channel=selected_channel;
							{
								int i;
								for(i=0; i<=maxcols/4-2; i++){
									redraw_channel(&txw, i+display_start_channel);
									redraw_channel(&rxw, i+display_start_channel);
								}
							}
							wrefresh(rxw.content);
						}
						else {
							redraw_channel(&txw, selected_channel+1);
							redraw_channel(&txw, selected_channel);
						}
						wrefresh(txw.content);
					}
					break;

				case KEY_RIGHT:
					if (selected_channel<512) {
						selected_channel++;
						if (selected_channel>display_start_channel+maxcols/4-2) {
							display_start_channel=selected_channel-(maxcols/4-2);
							{
								int i;
								for(i=0; i<=maxcols/4-2; i++){
									redraw_channel(&txw, i+display_start_channel);
									redraw_channel(&rxw, i+display_start_channel);
								}
							}
							wrefresh(rxw.content);
						}
						else {
							redraw_channel(&txw, selected_channel-1);
							redraw_channel(&txw, selected_channel);
						}
						wrefresh(txw.content);
					}
					break;

				case KEY_UP:
					if (txc[selected_channel] < 255) {
						r++;
						{
							int v = (int)txc[selected_channel]+r;
							if (v>255) { v = 255; }
							update_channel(&txw, selected_channel, v);
						}
						send3(0x48|((selected_channel>255)?1:0), selected_channel%256, txc[selected_channel]);
					}
					break;

				case KEY_DOWN:
					if (txc[selected_channel]) {
						r++;
						{
							int v = (int)txc[selected_channel]-r;
							if (v<0) { v = 0; }
							update_channel(&txw, selected_channel, v);
						}
						send3(0x48|((selected_channel>255)?1:0), selected_channel%256, txc[selected_channel]);
					}
					break;

				case KEY_PPAGE:
					update_channel(&txw, selected_channel, 255);
					send3(0x48|((selected_channel>255)?1:0), selected_channel%256, txc[selected_channel]);
					break;

				case KEY_NPAGE:
					update_channel(&txw, selected_channel, 0);
					send3(0x48|((selected_channel>255)?1:0), selected_channel%256, txc[selected_channel]);
					break;

				case ' ':
					if (txc[selected_channel] == 255) {
						update_channel(&txw, selected_channel, 0);
						send3(0x48|((selected_channel>255)?1:0), selected_channel%256, txc[selected_channel]);
					}
					else {
						update_channel(&txw, selected_channel, 255);
						send3(0x48|((selected_channel>255)?1:0), selected_channel%256, txc[selected_channel]);
					}
					break;

				case 12:
					clear();
					touchwin(stdscr);
					refresh();
					touchwin(rxw.decoration); wclear(rxw.content);
					touchwin(txw.decoration); wclear(txw.content);
					touchwin(msgw.decoration);
					{
						int i;
						for(i=0; i<msgw.drows; i++){
							wprintw(msgw.content, "\n");
						}
						for(i=0; i<=maxcols/4-2; i++){
							redraw_channel(&txw, i+display_start_channel);
							redraw_channel(&rxw, i+display_start_channel);
						}
					}
					wrefresh(rxw.decoration);
					wrefresh(txw.decoration);
					wrefresh(msgw.decoration);
					statusbar();
					break;

				case 'B':
					if (!status.blackout){ send1(0x4a); } // blackout on
					else { send1(0x4c); } // blackout off
					break;
				case 't':
					send1(0x50); //check tx status
					break;

				case 'T':
					if (!status.tx){ send1(0x44); } // TX On
					else { send1(0x46); } // TX Off
					break;

				case 'p':
				case 'P':
					toggle(&status.txvaluespolling);
					status.txvaluespolling_atrun = 0;
					statusbar();
					break;

				case 'v':
				case 'V':
					send1(0x24); //ask version informations
					break;

				case 's':
					send1(0x6c); // ask last rx start code
					break;

				case 'r':
					send1(0x6c); // ask last read code
					send1(0x68); // ask last channel number
					send1(0x6a); // ask rx fps
					break;

				case 'R':
					if (!status.rxenabled){ send1(0x64); } // RX On
					else { send1(0x66); } // RX Off
					break;

				default:
					wprintw(msgw.content, "[%d]", a);
					wrefresh(msgw.content);
			}
			if (a == 'q' || a == 'Q'){
				destrow_dwin(&rxw);
				destrow_dwin(&txw);
				destrow_dwin(&msgw);
				clear();
				refresh();
				endwin();
				break;
			}
		}
	}

	endwin();
	close(fd);
	return 0;
}
