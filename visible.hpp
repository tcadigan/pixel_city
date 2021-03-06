#ifndef VISIBLE_HPP_
#define VISIBLE_HPP_

#include "gl-vector3.hpp"

#define GRID_RESOLUTION 32
#define GRID_CELL (GRID_RESOLUTION / 2)
#define GRID_SIZE (WORLD_SIZE / GRID_RESOLUTION)
#define WORLD_TO_GRID(x) (int)(x / GRID_RESOLUTION)
#define GRID_TO_WORLD(x) ((float)x * GRID_RESOLUTION)

void VisibleUpdate(void);
bool Visible(gl_vector3 pos);
bool Visible(int x, int z);

#endif /* VISIBLE_HPP_ */
