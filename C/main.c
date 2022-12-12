#include <locale.h>
#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W           32
#define H           32
#define SPAWN_RATIO 40
#define TIMEOUT_MS  1000

struct Rule {
  int birth[2];
  int survival[2];
};

typedef bool Map[H][W];

enum Request {
  INIT,
  PLAY,
  PAUSE,
  QUIT,

  CUR_LT,
  CUR_DN,
  CUR_UP,
  CUR_RT,

  TOGGLE_CELL,

  STEP,
  FAST_FWD,
};

struct Visual {
  int margin;
  int gap;
  char cell[50];
  char blank[50];
};

struct World {
  struct Rule rule;
  unsigned int seed;
  unsigned int iteration;
  Map curr;
  Map next;
  int cursor[2];
  bool loop;
  enum Request required;
  struct Visual visual;
};

int randint(int min, int max, bool isiclusive);
int randint(int min, int max, bool isinclusive) {
  if (isinclusive != false) return rand() % (max - min + 1) + min;

  return rand() % (max - min) + min + 1;
}

// int nspawn(int n, Map *map) {
//   int y, x;
//   for (int i = 0; i < n; i++) {
//     y = randint(0, H - 1, 1);
//     x = randint(0, W - 1, 1);
//
//     (*map)[y][x] = 1;
//   }
//   return 0;
// }

int genmap(int spawnratio, Map *curr, Map *next);
int genmap(int spawnratio, Map *curr, Map *next) {
  int rand;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      rand = randint(0, 100, 1);
      if (rand <= spawnratio) {
        (*curr)[y][x] = true;
      } else {
        (*curr)[y][x] = false;
      }
    }
  }

  memcpy(*next, *curr, sizeof(*curr));
  // vs. for-loop?

  return 0;
}

int initworld(struct World *world);
int initworld(struct World *world) {
  static const struct World WORLD_DEFAULT = {
    .rule = {
      .birth = {3, 3},
      .survival = {2, 3},
    },
    .seed = (unsigned int)0,
    .iteration = (unsigned)0,
    .curr = {},
    .next = {},
    .cursor = {0, 0},
    .loop = false,
    .required = INIT,
    .visual = {
      .margin = 1,
      .gap    = 1,
      .cell   = "@",
      .blank = " ",
    },
  };

  *world = WORLD_DEFAULT;

  genmap(SPAWN_RATIO, &world->curr, &world->next);

  world->seed = (unsigned int)time(NULL);
  srand(world->seed);

  return 0;
}

int printmap(Map *curr, struct Visual *visual);
int printmap(Map *curr, struct Visual *visual) {
  for (int y = 0; y < H; y++) {
    for (int i = 0; i < visual->margin; i++)
      printw("%s", visual->blank);  // Left margin

    for (int x = 0; x < W; x++) {
      if (x != 0)
        for (int i = 0; i < visual->gap; i++)
          printw("%s", visual->blank);  // Gap

      if ((*curr)[y][x] != false) {
        attron(COLOR_PAIR(2));
        printw("%s", visual->cell);
        attroff(COLOR_PAIR(2));
      } else {
        printw("%s", visual->blank);
      }
    }

    for (int i = 0; i < visual->margin; i++)
      printw("%s", visual->blank);  // Right margin

    printw("\n");  // Space is padding
  }

  return 0;
}

int cntcell(Map *map);
int cntcell(Map *map) {
  int n = 0;

  for (int y = 0; y < H; y++)
    for (int x = 0; x < H; x++)
      if ((*map)[y][x] != false) n += 1;

  return n;
}
int dcntcell(const int cellcnt);
int dcntcell(const int cellcnt) {
  static int pre = 0;

  int d = cellcnt - pre;
  pre   = cellcnt;
  return d;
}

int printstats(struct World *world);
int printstats(struct World *world) {
  static char stats[80];

  if (world->iteration == 0 || world->loop != false) {
    sprintf(stats, "SEED: %d,", world->seed);
    sprintf(stats, "%s ITER: %10u,", stats, world->iteration);

    int c = cntcell(&world->curr);
    int d = dcntcell(c);
    sprintf(stats, "%s SUM: %d (%+d)", stats, c, d);
  }

  printw("%s\n", stats);

  return 0;
}

int printworld(struct World *world);
int printworld(struct World *world) {
  printstats(world);

  printmap(&world->curr, &world->visual);  // TODO
  return 0;
}

int cntneighbors(Map *map, int y, int x);
int cntneighbors(Map *map, int y, int x) {
  int n = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dy == 0 && dx == 0) continue;
      if ((*map)[(y + H + dy) % H][(x + W + dx) % W] != false) n++;
    }
  }
  return n;
}

bool cellnextstate(bool currentstate, int nneighbors, struct Rule *rule);
bool cellnextstate(bool currentstate, int nneighbors, struct Rule *rule) {
  if (nneighbors < rule->survival[0] || rule->survival[1] < nneighbors) {
    return false;  // Kill
  } else if (rule->birth[0] <= nneighbors && nneighbors <= rule->birth[1]) {
    return true;  // Reproduce
  }
  return currentstate;  // Keep
}

int *updatemap(Map *curr, Map *next, struct Rule *rule) {
  int nneighbors = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      nneighbors    = cntneighbors(curr, y, x);
      (*next)[y][x] = cellnextstate((*curr)[y][x], nneighbors, rule);
    }
  }

  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      (*curr)[y][x] = (*next)[y][x];

  return 0;
}

struct World *iterateworld(struct World *world) {
  world->iteration += 1;
  updatemap(&world->curr, &world->next, &world->rule);
  return world;
}

enum Request waitkeyinput() {
  timeout(TIMEOUT_MS);
  switch (getch()) {
    case 'R':
      return INIT;
    case 'p':
      return PLAY;
    case 'P':
      return PAUSE;
    case 'q':
    case 'd' & 0x1f:  // Ctrl+d
      return QUIT;

    case 'h':
    case KEY_LEFT:
      return CUR_LT;
    case 'j':
    case KEY_DOWN:
      return CUR_DN;
    case 'k':
    case KEY_UP:
      return CUR_UP;
    case 'l':
    case KEY_RIGHT:
      return CUR_RT;

    case 't':
    case ' ':
    case KEY_ENTER:
      return TOGGLE_CELL;

    case 'n':
      return STEP;
    default:
      return FAST_FWD;
  }
}

inline void mvcursor(enum Request required, int (*cursor)[2]);
inline void mvcursor(enum Request required, int (*cursor)[2]) {
  switch (required) {
    case CUR_LT:
      (*cursor)[1] = ((*cursor)[1] + W - 1) % W;
      return;
    case CUR_DN:
      (*cursor)[0] = ((*cursor)[0] + H + 1) % H;
      return;
    case CUR_UP:
      (*cursor)[0] = ((*cursor)[0] + H - 1) % H;
      return;
    case CUR_RT:
      (*cursor)[1] = ((*cursor)[1] + W + 1) % W;
      return;
    default:
      return;
  }
}

inline bool togglecell(Map *curr, int (*cursor)[2]);
inline bool togglecell(Map *curr, int (*cursor)[2]) {
  bool b = (*curr)[(*cursor)[0]][(*cursor)[1]] == false;
  (*curr)[(*cursor)[0]][(*cursor)[1]] = b;
  return b;
}

inline int pause(enum Request request, enum Request *required) {
  *required = request;
  return 0;
}

int main(int argc, char *argv[]) {
  struct World world;

  initworld(&world);
  world.visual = (struct Visual){
      .margin = 0,
      .gap    = 1,
      .cell   = " ",
      //.cell = "@",
      //.cell  = "🦠",
      .blank = " ",
  };

  setlocale(LC_ALL, "");

  initscr();

  start_color();
  init_pair(1, COLOR_BLACK, COLOR_WHITE);
  init_pair(2, COLOR_BLACK, COLOR_GREEN);

  // attron(COLOR_PAIR(1));

  cbreak();
  noecho();
  nonl();
  intrflush(stdscr, FALSE);
  keypad(stdscr, TRUE);
  // curs_set(0); // Hide cursor

  while (world.iteration < UINT32_MAX) {
    clear();
    printworld(&world);
    move(
        1 + world.cursor[0],
        world.visual.margin + world.visual.gap * world.cursor[1]
            + world.cursor[1]);
    refresh();

    if (world.loop != false) iterateworld(&world);

    // Event
    world.required = waitkeyinput();
    switch (world.required) {
      case INIT:
        initworld(&world);
        world.loop = false;
        break;
      case PLAY:
        world.loop = true;
        break;
      case PAUSE:
        world.loop = false;
        break;
      case QUIT:
        clear();
        endwin();
        return 0;

      case CUR_LT:
      case CUR_DN:
      case CUR_UP:
      case CUR_RT:
        if (world.loop != false) break;
        mvcursor(world.required, &world.cursor);
        break;

      case TOGGLE_CELL:
        if (world.loop != false) break;
        togglecell(&world.curr, &world.cursor);
        break;

      case STEP:
        if (world.loop != false) break;
        iterateworld(&world);
        break;
      default:
        break;
    }
  }

  endwin();
  // delscreen();

  return 0;
}
