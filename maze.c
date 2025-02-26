#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
struct direction {
signed char x;
signed char y;
};
#define DIRECTIONS (8)
const struct direction directions[DIRECTIONS] = {
{ -1, -1 },
{ -1, 0 },
{ -1, 1 },
{ 0, -1 },
{ 0, 1 },
{ 1, -1 },
{ 1, 0 },
{ 1, 1 }
};
struct position {
int x;
int y;
};
const struct position NO_POSITION = { -1, -1 };
static inline int
eqPosition(struct position p, struct position q)
{
return p.x == q.x && p.y == q.y;
}
#define WALL (0)
#define PATH (1)
#define CYCLE (2)
struct square {
int contents;
struct position parent; /* used by search routine */
};
struct maze {
struct position size; /* rows = size.x, columns = size.y */
struct square *a; /* packed array of squares */
};
/* look up a position in a maze */
#define Mref(m, pos) ((m)->a[(pos).y * (m)->size.x + (pos).x])
#define Mget(m, pos) (assert((pos).x >= 0 && (pos).y >= 0 && (pos).x < (m)->size.x && (pos).y < (m)->size.y), Mref((m), (pos)))
/* add direction to source to get target */
/* returns 1 if target is in range */
int
offset(const struct maze *m, struct position *target, struct position source, struct direction dir)
{
target->x = source.x + dir.x;
target->y = source.y + dir.y;
return target->x >= 0 && target->y >= 0 && target->x < m->size.x && target->y < m->size.y;
}
/* free a maze */
void
destroyMaze(struct maze *m)
{
free(m->a);
free(m);
}
/* load a maze in restricted PGM format */
struct maze *
loadMaze(FILE *f)
{
struct maze *m;
struct position i;
m = malloc(sizeof(*m));
assert(m);
fscanf(f, "P5 %d %d 255\n", &m->size.x, &m->size.y);
m->a = malloc(sizeof(struct square) * m->size.y * m->size.x);
for(i.y = 0; i.y < m->size.y; i.y++) {
for(i.x = 0; i.x < m->size.x; i.x++) {
Mref(m, i).contents = getchar();
assert(Mref(m, i).contents == 0 || Mref(m, i).contents == 1);
}
}
return m;
}
void
saveMaze(struct maze *m, FILE *f)
{
struct position i;
fprintf(f, "P5 %d %d 255\n", m->size.x, m->size.y);
for(i.y = 0; i.y < m->size.y; i.y++) {
for(i.x = 0; i.x < m->size.x; i.x++) {
putc(Mref(m, i).contents, f);
}
}
}
/* how many neighbors of position are PATH? */
int
countNeighbors(const struct maze *m, struct position p)
{
struct position q;
int i;
int count = 0;
for(i = 0; i < DIRECTIONS; i++) {
if(offset(m, &q, p, directions[i]) && Mget(m, q).contents == PATH) {
count++;
}
}
return count;
}
struct position
randomPosition(const struct maze *m)
{
struct position r;
r.x = rand() % m->size.x;
r.y = rand() % m->size.y;
return r;
}
#define PATIENCE_MULTIPLIER (4)
/* generate a random connected maze with no cycles */
struct maze *
generateMaze(struct position size)
{
struct maze *m;
struct position r;
struct position i;
size_t countdown; /* how long to run before we get tired of not making progress */
size_t maxCountdown; /* value to reset countdown to when we make progress */
m = malloc(sizeof(struct maze));
assert(m);
m->size = size;
m->a = malloc(sizeof(struct square) * m->size.x * m->size.y);
assert(m->a);
/* start with all WALL */
for(i.y = 0; i.y < m->size.y; i.y++) {
for(i.x = 0; i.x < m->size.x; i.x++) {
Mref(m, i).contents = WALL;
}
}
/* place a PATH on a random square */
r = randomPosition(m);
Mref(m, r).contents = PATH;
maxCountdown = PATIENCE_MULTIPLIER * size.x * size.y * log(size.x * size.y);
for(countdown = maxCountdown; countdown > 0; countdown--) {
/* pick a random square */
r = randomPosition(m);
/* add if we have exactly one neighbor already in the maze */
if(Mget(m, r).contents == WALL && countNeighbors(m, r) == 1) {
Mref(m, r).contents = PATH;
/* reset countdown */
countdown = maxCountdown;
}
}
return m;
}
/* create a cycle by adding one extra PATH square
* that connects two existing squares */
void
mazeAddCycle(struct maze *m)
{
struct position r;
do {
r = randomPosition(m);
} while(Mget(m, r).contents != WALL || countNeighbors(m, r) != 2);
Mref(m, r).contents = PATH;
}
/* Search for a cycle of PATH nodes.
* If found, mark all nodes on the cycle as CYCLE. */
void
mazeSearchForCycle(struct maze *m)
{
struct position root; /* root of tree */
struct position current; /* what we just popped */
struct position parent ; /* current's parent */
struct position neighbor; /* neighbor to push */
struct position ancestor; /* for filling in CYCLE */
int i;
struct position *queue;
size_t head; /* where to dequeue */
size_t tail; /* where to enqueue */
/* this is probably more space than we need */
queue = malloc(sizeof(struct position) * m->size.x * m->size.y);
assert(queue);
head = tail = 0;
/* clear out bookkeeping data */
for(current.y = 0; current.y < m->size.y; current.y++) {
for(current.x = 0; current.x < m->size.x; current.x++) {
Mref(m, current).parent = NO_POSITION;
/* probably not necessary but will avoid trouble
* if somebody calls this twice */
if(Mget(m, current).contents != WALL) {
Mref(m, current).contents = PATH;
}
}
}
/* find a root */
/* we don't care what this is, but it can't be a WALL */
do {
root = randomPosition(m);
} while(Mget(m, root).contents != PATH);
/* push root */
Mref(m, root).parent = root;
queue[tail++] = root;
/* now perform the BFS */
/* if we ever find a neighbor that is already in the tree and not our parent,
* we have found our cycle */
while(head < tail) {
current = queue[head++];
parent = Mget(m, current).parent;
/* push all neighbors not already in tree */
/* if one is in the tree, we win */
for(i = 0; i < DIRECTIONS; i++) {
if(offset(m, &neighbor, current, directions[i]) && Mget(m, neighbor).contents == PATH && !eqPosition(neighbor, parent)) {
/* is it already in the tree? */
if(!eqPosition(Mget(m, neighbor).parent, NO_POSITION)) {
/* we win */
/* cycle consists of all ancestors of neighbor and current
* up to common ancestor */
for(ancestor = neighbor; !eqPosition(ancestor, root); ancestor = Mget(m, ancestor).parent) {
Mref(m, ancestor).contents = CYCLE;
}
/* also mark root */
Mref(m, root).contents = CYCLE;
/* now work up from current */
for(ancestor = current; !eqPosition(ancestor, root); ancestor = Mget(m, ancestor).parent) {
if(Mget(m, ancestor).contents == PATH) {
/* add to the cycle */
Mref(m, ancestor).contents = CYCLE;
} else {
/* this is the common ancestor, which is not root */
/* mark all proper ancestors as PATH */
do {
ancestor = Mget(m, ancestor).parent;
Mref(m, ancestor).contents = PATH;
} while(!eqPosition(ancestor, root));
/* can't just break, too many loops */
goto doneWithSearch;
}
}
} else {
Mref(m, neighbor).parent = current;
queue[tail++] = neighbor;
}
}
}
}
doneWithSearch:
free(queue);
}
int
main(int argc, char **argv)
{
struct maze *m;
struct position size = { 80, 60 };
int seed;
switch(argc) {
case 1:
/* sample solution for the assignment */
m = loadMaze(stdin);
mazeSearchForCycle(m);
saveMaze(m, stdout);
destroyMaze(m);
break;
case 4:
/* generate a new test image */
/* usage is ./maze width height seed */
/* if seed is negative, use absolute value and don't put in cycle */
size.x = atoi(argv[1]);
size.y = atoi(argv[2]);
seed = atoi(argv[3]);
srand(seed < 0 ? -seed : seed);
m = generateMaze(size);
if(seed >= 0) { mazeAddCycle(m); }
saveMaze(m, stdout);
destroyMaze(m);
break;
default:
fprintf(stderr, "Usage %s or %s width height seed\n", argv[0], argv[0]);
return 1;
}
return 0;
}