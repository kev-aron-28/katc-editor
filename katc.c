#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>

// Defines
#define CTRL_KEY(k) ((k) & 0x1f)
#define KATC_VERSION "0.0.1"

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY
};

// Data

typedef struct erow {
  int size;
  char *chars;
} erow;

struct editorConfig {
  struct termios originalTerminalAttributes;
  int screenrows;
  int screencols;
  int rowoff;
  int numrows;
  erow *row;
  int cursorx, cursory;
};
struct editorConfig E;

// Terminal stuff
void die(const char *s) {
  
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.originalTerminalAttributes) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  // Read current atributes into a struct
  if(tcgetattr(STDIN_FILENO, &E.originalTerminalAttributes) == -1) die("tcgetattr");
  atexit(disableRawMode);
  

  struct termios raw = E.originalTerminalAttributes;
  // Disables control software flow control
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  // Set new attributes
  if(tcsetattr(STDERR_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int editorReadKey() {
  int nread;
  char c;

  while ((nread = read(STDERR_FILENO, &c, 1)) != 1)
  {
    if(nread == -1 && errno != EAGAIN) die("read");
  }

  if(c == '\x1b') {
    char seq[3];
    if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if(seq[0] == '[') {

      if(seq[1] >= '0' && seq[1] <= '9') {
        if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if(seq[2] == '~') {
          switch(seq[1]) {
            case '3': return DEL_KEY; 
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
        }
      }
    }
    return '\x1b';
  } else {
    return c;
  }
  
}

int getCursorPosition(int *rows, int *cols) {

  char buf[32];
  unsigned int i = 0;

  if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while(i < sizeof(buf) - 1) {
    if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if(buf[i] == 'R') break;
    i++;
  }

  buf[i] = '\0';

  printf("\n\r&buf[1]: '%s'\r\n", &buf[1]);

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

// row operations

void editorAppendRow (char *s, size_t len) {
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}


// File i/o

void editorOpen(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
}

// append buffer

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0};

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if(new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

// output

void editorScroll() {
  if(E.cursory < E.rowoff) {
    E.rowoff = E.cursory;
  }

  if(E.cursory >= E.rowoff + E.screenrows) {
    E.rowoff = E.cursory - E.screenrows + 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++)
  {
    int filerow = y + E.rowoff;
    if(filerow >= E.numrows) {
       if(E.numrows == 0 && y == E.screenrows / 3) {
          char welcome[80];
          int welcomeLen = snprintf(welcome, sizeof(welcome), "KATC editor -- version %s", KATC_VERSION);
          if(welcomeLen > E.screencols) welcomeLen = E.screencols;

          int padding = (E.screencols - welcomeLen) / 2;

          if(padding) {
            abAppend(ab, "~", 1);
            padding--;
          }

          while (padding--)
          {
            abAppend(ab, " ", 1);
          }
          abAppend(ab, welcome, welcomeLen);
      } else {
          abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].size;
      if(len > E.screenrows) len = E.screencols;
      abAppend(ab, E.row[filerow].chars, len);
    }

    abAppend(ab, "\x1b[K", 3);
    if(y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {

  editorScroll();
  struct abuf ab = ABUF_INIT;
  write(STDOUT_FILENO, "\x1b[?25l", 6);
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cursory - E.rowoff) + 1, E.cursorx + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

// input

void editorMoveCursor(int key) {
  switch (key)
  {
  case ARROW_LEFT:
    if(E.cursorx != 0) {
      E.cursorx--;  
    }
    break;
  case ARROW_RIGHT:
    if(E.cursorx != E.screencols - 1) {
      E.cursorx++;
    }
    break;
  case ARROW_UP:
    if(E.cursory != 0) {
      E.cursory--;
    }
    break;
  case ARROW_DOWN:
    if(E.cursory < E.numrows) {
      E.cursory++;
    }
    break;
  }
}

void editorProcessKeyPress() {
  int c = editorReadKey();

  switch (c)
  {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;
  }
}
 
// Init

void initEditor() {
  E.cursorx = 0;
  E.cursory = 0;
  E.numrows = 0;
  E.rowoff = 0;
  E.row = NULL;
  if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(int argc, char const *argv[])
{
  enableRawMode();
  initEditor();

  if(argc >= 2) {
    editorOpen(argv[1]);
  }

  while(1) {
    editorRefreshScreen();
    editorProcessKeyPress();
  }
  return 0;
}
