/*
 * world.cpp
 *
 * 2009 Shamus Young
 *
 * This holds a bunch of variables used by other modules. It has the 
 * claim system, which tracks all of the "porperty" that is being
 * used: As roads, buildings, etc.
 *
 */

#include "world.hpp"

#include <SDL.h>

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <vector>

#include "building.hpp"
#include "camera.hpp"
#include "car.hpp"
#include "decoration.hpp"
#include "light.hpp"
#include "macro.hpp"
#include "math.hpp"
#include "mesh.hpp"
#include "random.hpp"
#include "render.hpp"
#include "sky.hpp"
#include "texture.hpp"
#include "visible.hpp"
#include "win.hpp"
#include "world.hpp"

#define LIGHT_COLOR_COUNT (sizeof(light_colors) / sizeof(HSL))

using namespace std;

struct plot {
    int x;
    int z;
    int width;
    int depth;
};

enum {
    FADE_IDLE,
    FADE_OUT,
    FADE_WAIT,
    FADE_IN,
};

struct HSL {
    float hue;
    float sat;
    float lum;
};

class CStreet {
public:
    int _x;
    int _y;
    int _width;
    int _depth;
    int _mesh;

    CStreet(int x, int y, int width, int depth);
    ~CStreet();
    void Render();
};

static HSL light_colors[] = {
    {0.04f, 0.9f, 0.93f}, // Amber/pink
    {0.055f, 0.95f, 0.93f}, // Slightly brighter amber
    {0.08f, 0.7f, 0.93f}, // Very pale amber
    {0.07f, 0.9f, 0.93f}, // Very pale orange
    {0.1f, 0.9f, 0.85f},  // Peach
    {0.13f, 0.9f, 0.93f}, // Pale yellow
    {0.15f, 0.9f, 0.93f}, // Yellow
    {0.17f, 1.0f, 0.85f}, // Saturated yellow
    {0.55f, 0.9f, 0.93f}, // Cyan
    {0.6f, 0.9f, 0.93f}, // Pale Blue
    {0.65f, 0.9f, 0.93f}, // Pale blue II, the palening
    {0.65f, 0.4f, 0.99f}, // Pure white. bo-ring.
    {0.65f, 0.0f, 0.8f}, // Dimmer white
    {0.65, 0.0f, 0.6f}, // Dimmest white
};

static gl_rgba bloom_color;
static long int last_update;
static char world[WORLD_SIZE][WORLD_SIZE];
static Sky *sky;
static int fade_state;
static unsigned int fade_start;
static float fade_current;
static int modern_count;
static int tower_count;
static int blocky_count;
static bool reset_needed;
static int skyscrapers;
static gl_bbox hot_zone;
static int logo_index;
static unsigned int start_time;
static int scene_begin;

static gl_rgba get_light_color(float sat, float lum)
{
    int index;

    index = RandomVal(LIGHT_COLOR_COUNT);
    gl_rgba temp;
    return temp.from_hsl(light_colors[index].hue, sat, lum);
}

static void claim(int x, int y, int width, int depth, int val)
{
    int xx;
    int yy;

    for(xx = x; xx < (x + width); ++xx) {
        for(yy = y; yy < (y + depth); ++yy) {
            int x_index = CLAMP(xx, 0, WORLD_SIZE - 1);
            int y_index = CLAMP(yy, 0, WORLD_SIZE - 1);

            world[x_index][y_index] |= val;
        }
    }
}

static bool claimed(int x, int y, int width, int depth)
{
    int xx;
    int yy;

    for(xx = x; xx < (x + width); ++xx) {
        for(yy = y; yy < (y + width); ++yy) {
            int x_index = CLAMP(xx, 0, WORLD_SIZE - 1);
            int y_index = CLAMP(yy, 0, WORLD_SIZE - 1);

            if(world[x_index][y_index]) {
                return true;
            }
        }
    }

    return false;
}

static void build_road(int x1, int y1, int width, int depth)
{
    int lanes;
    int divider;
    int sidewalk;

    // The given rectangle defines a street and its sidewalk. See which way
    // it goes.
    if(width > depth) {
        lanes = depth;
    }
    else {
        lanes = width;
    }

    // If we don't have room for both lanes and sidewalk, abort
    if(lanes < 4) {
        return;
    }

    // If we have an odd number of lanes give the extra to a divider
    if(lanes % 2) {
        lanes--;
        divider = 1;
    }
    else {
        divider = 0;
    }

    // No more than 10 traffic lanes, give the rest to sidewalks
    sidewalk = MAX(2, (lanes - 10));
    lanes -= sidewalk;
    sidewalk /= 2;

    // Take the remaining space and give half to each direction
    lanes /= 2;

    // Mark the entire rectangle as used
    claim(x1, y1, width, depth, CLAIM_WALK);

    // No place the directional roads
    if(width > depth) {
        claim(x1, y1 + sidewalk, width, lanes, CLAIM_ROAD | MAP_ROAD_WEST);
        claim(x1,
              y1 + sidewalk + lanes + divider,
              width,
              lanes,
              CLAIM_ROAD | MAP_ROAD_EAST);
    }
    else {
        claim(x1 + sidewalk, y1, lanes, depth, CLAIM_ROAD | MAP_ROAD_SOUTH);
        claim(x1 + sidewalk + lanes + divider,
              y1,
              lanes,
              depth,
              CLAIM_ROAD | MAP_ROAD_NORTH);
    }
}

static plot find_plot(int x, int z)
{
    plot p;
    int x1;
    int x2;
    int z1;
    int z2;

    // We've been given the location of an open bit of land, be we have
    // no idea how big it is. Find the boundary.
    x2 = x;
    x1 = x;
    
    while(!claimed(x1 - 1, z, 1, 1) && (x1 > 0)) {
        x1--;
    }

    while(!claimed(x2 + 1, z, 1, 1) && (x2 < WORLD_SIZE)) {
        x2++;
    }

    z2 = z;
    z1 = z;
    
    while(!claimed(x, z1 - 1, 1, 1) && (z1 > 0)) {
        z1--;
    }

    while(!claimed(x, z2 + 1, 1, 1) && (z2 < WORLD_SIZE)) {
        z2++;
    }

    p.width = x2 - x1;
    p.depth = z2 - z1;   
    p.x = x1;
    p.z = z1;
    
    return p;
}

static plot make_plot(int x, int z, int width, int depth)
{
    plot p = {x, z, width, depth};
    
    return p;
}

void do_building(plot p)
{
    int height;
    int seed;
    int area;
    int type;
    gl_rgba color;
    bool square;

    // Now we know how bit the rectangle plot is.
    area = p.width * p.depth;
    color = WorldLightColor(RandomVal());
    seed = RandomVal();

    // Make sure the plot is big enough for a building
    if((p.width < 10) || (p.depth < 10)) {
        return;
    }

    // If the area is too big for one building subdivide it
    if(area > 800) {
        if(COIN_FLIP) {
            p.width /= 2;
            if(COIN_FLIP) {
                do_building(make_plot(p.x, p.z, p.width, p.depth));
            }
            else {
                do_building(make_plot(p.x + p.width, p.z, p.width, p.depth));
            }

            return;
        }
        else {
            p.depth /= 2;
            if(COIN_FLIP) {
                do_building(make_plot(p.x, p.z, p.width, p.depth));
            }
            else {
                do_building(make_plot(p.x, p.z + p.depth, p.width, p.depth));
            }

            return;
        }
    }

    if(area < 100) {
        return;
    }

    // The plot is "square" if width and depth are close
    square = (abs(p.width - p.depth) < 10);

    // Mark the land as used so other buildings don't appear here,
    // even if we don't use it all.
    claim(p.x, p.z, p.width, p.depth, CLAIM_BUILDING);

    // The roundy mod buildings look best on square plots.
    if(square && (p.width > 20)) {
        height = 45 + RandomVal(10);
        modern_count++;
        skyscrapers++;
        new Building(BUILDING_MODERN, 
                     p.x,
                     p.z,
                     height,
                     p.width,
                     p.depth,
                     seed,
                     color);

        return;
    }

    // // Rectangular plots are a good place for Blocky style buildings
    // // to sprawl blockily
    // if((p.width > (p.depth * 2)) 
    //    || ((p.depth > (p.width * 2)) && (area > 800))) {
    //     height = 20 + RandomVal(10);
    //     blocky_count++;
    //     skyscrapers++;
    //     new CBuilding(BUILDING_BLOCKY,
    //                   p.x,
    //                   p.z,
    //                   height,
    //                   p.width,
    //                   p.depth,
    //                   seed,
    //                   color);

    //     return;
    // }

    // tower_count = -1;

    // This spot isn't ideal for any particular building, but try to keep
    // a good mix
    if((tower_count < modern_count) && (tower_count < blocky_count)) {
        type = BUILDING_TOWER;
        tower_count++;
    }
    else if(blocky_count < modern_count) {
        type = BUILDING_BLOCKY;
        blocky_count++;
    }
    else {
        type = BUILDING_MODERN;
        modern_count++;
    }

    height = 45 + RandomVal(10);

    new Building(type, p.x, p.z, height, p.width, p.depth, seed, color);
    skyscrapers++;
}

static int build_light_strip(int x1, int z1, int direction)
{
    Decoration *d;
    gl_rgba color;
    int x2;
    int z2;
    int length;
    int width;
    int depth;
    int dir_x;
    int dir_z;
    float size_adjust;

    // We adjust the size of the lights with this.
    size_adjust = 2.5f;
    gl_rgba temp;
    color = temp.from_hsl(0.09f, 0.99f, 0.85f);

    switch(direction) {
    case NORTH:
        dir_z = 1;
        dir_x = 0;
        break;
    case SOUTH:
        dir_z = 1;
        dir_x = 0;
        break;
    case EAST:
        dir_z = 0;
        dir_x = 1;
        break;
    case WEST:
        dir_z = 0;
        dir_x = 1;
        break;
    }

    // So we know we're on the corner of an intersection
    // look in the given until we reach the end of 
    // the sidewalk
    x2 = x1;
    z2 = z1;
    length = 0;

    while((x2 > 0) && (x2 < WORLD_SIZE) && (z2 > 0) && (z2 < WORLD_SIZE)) {
        if(world[x2][z2] & CLAIM_ROAD) {
            break;
        }

        length++;
        x2 += dir_x;
        z2 += dir_z;
    }

    if(length < 10) {
        return length;
    }

    width = MAX(abs(x2 - x1), 1);
    depth = MAX(abs(z2 - z1), 1);

    d = new Decoration();

    if(direction == EAST) {
        d->CreateLightStrip((float)x1,
                            (float)z1 - size_adjust,
                            (float)width,
                            (float)depth + size_adjust,
                            2,
                            color);
    }
    else if(direction == WEST) {
        d->CreateLightStrip((float)x1,
                            (float)z1,
                            (float)width,
                            (float)depth + size_adjust,
                            2,
                            color);
    }
    else if(direction == NORTH) {
        d->CreateLightStrip((float)x1,
                            (float)z1,
                            (float)width + size_adjust,
                            (float)depth,
                            2,
                            color);
    }
    else {
        d->CreateLightStrip((float)x1 - size_adjust,
                            (float)z1,
                            (float)width - size_adjust,
                            (float)depth,
                            2,
                            color);
    }

    return length;
}

static void do_reset(void)
{
    int x;
    int y;
    int width;
    int depth;
    int height;
    int attempts;
    bool broadway_done;
    bool road_left;
    bool road_right;
    gl_rgba light_color;
    gl_rgba building_color;
    float west_street;
    float north_street;
    float east_street;
    float south_street;

    // Re-init Random to make the same city each time.
    // Helpful when running tests.
    RandomInit(6);
    reset_needed = false;
    broadway_done = false;
    skyscrapers = 0;
    logo_index = 0;
    scene_begin = 0;
    modern_count = 0;
    blocky_count = 0;
    tower_count = 0;
    hot_zone.clear();
    EntityClear();
    LightClear();
    CarClear();
    TextureReset();

    // Pink a tint for the bloom
    bloom_color = get_light_color(0.5f + ((float)RandomVal(10) / 20.0f), 0.75f);
    gl_rgba temp;
    light_color = temp.from_hsl(0.11f, 1.0f, 0.65f);
    memset(world, '0', WORLD_SIZE * WORLD_SIZE);
    y = WORLD_EDGE;
    for(/* empty */; y < (WORLD_SIZE - WORLD_EDGE); y += RandomVal(25) + 25) {
        if(!broadway_done && (y > (WORLD_HALF - 20))) {
            build_road(0, y, WORLD_SIZE, 19);
            y += 20;
            broadway_done = true;
        }
        else {
            depth = 6 + RandomVal(6);
            if(y < (WORLD_HALF / 2)) {
                north_street = (float)(y + (depth / 2));
            }
            
            if(y < (WORLD_SIZE - (WORLD_HALF / 2))) {
                south_street = (float)(y + (depth / 2));
            }

            build_road(0, y, WORLD_SIZE, depth);
        }
    }

    broadway_done = false;
    x = WORLD_EDGE;
    for(/* empty */; x < (WORLD_SIZE - WORLD_EDGE); x += RandomVal(25) + 25) {
        if(!broadway_done && (x > (WORLD_HALF - 20))) {
            build_road(x, 0, 19, WORLD_SIZE);
            x += 20;
            broadway_done = true;
        }
        else {
            width = 6 + RandomVal(6);
            if(x <= (WORLD_HALF / 2)) {
                west_street = (float)(x + (width / 2));
            }
            if(x <= (WORLD_HALF + (WORLD_HALF / 2))) {
                east_street = (float)(x + (width / 2));
            }

            build_road(x, 0, width, WORLD_SIZE);
        }
    }

    // We kept track of the positions of streets that will outline the
    // high-detail hot zone in the middle of the world. Save this in a 
    // bounding box so that late we can have the camera fly around without
    // clipping through buildings.
    hot_zone.contain_point(gl_vector3(west_street, 0.0f, north_street));

    hot_zone.contain_point(gl_vector3(east_street, 0.0f, south_street));

    // Scan for places to put runs of streetlights on the east and west
    // size of the road
    for(x = 1; x < (WORLD_SIZE - 1); ++x) {
        for(y = 0; y < WORLD_SIZE; ++y) {
            // If this isn't a bit of sidewalk, then keep looking
            if(!(world[x][y] & CLAIM_WALK)) {
                continue;
            }

            // If it's used as a road, skip it.
            if((world[x][y] & CLAIM_ROAD)) {
                continue;
            }

            road_left = ((world[x + 1][y] & CLAIM_ROAD) != 0);
            road_right = ((world[x - 1][y] & CLAIM_ROAD) != 0);
            
            // If the cells to our east and west are not road,
            // then we're not on a corner.
            if(!road_left && !road_right) {
                continue;
            }

            // If the cell to our east and west is road, then we're on
            // a median. skip it
            if(road_left && road_right) {
                continue;
            }

            y += build_light_strip(x, y, road_right ? SOUTH : NORTH);
        }
    }

    // Scan for places to put runs of streetlights on the north
    // and south side of the road
    for(y = 1; y < (WORLD_SIZE - 1); ++y) {
        for(x = 1; x < (WORLD_SIZE - 1); ++x) {
            // If this isn't a bit of sidewalk, then keep looking
            if(!(world[x][y] & CLAIM_WALK)) {
                continue;
            }

            // If it's used as a road, skip it.
            if(world[x][y] & CLAIM_ROAD) {
                continue;
            }

            road_left = ((world[x][y + 1] & CLAIM_ROAD) != 0);
            road_right = ((world[x][y - 1] & CLAIM_ROAD) != 0);

            // If the cell to our east and west is road, then we're on
            // a median. skip it
            if(road_left && road_right) {
                continue;
            }

            // If the cells to our north and source are not road,
            // then we're not on a corner.
            if(!road_left && !road_right) {
                continue;
            }

            x += build_light_strip(x, y, road_right ? EAST : WEST);
        }
    }
        
    // Scan over the center area of the map and place the big buildings
    attempts = 0;
    while((skyscrapers < 50) && (attempts < 350)) {
        x = (WORLD_HALF / 2) + (RandomVal() % WORLD_HALF);
        y = (WORLD_HALF / 2) + (RandomVal() % WORLD_HALF);
        if(!claimed(x, y, 1, 1)) {
            do_building(find_plot(x, y));
            skyscrapers++;
        }

        attempts++;
    }

    // Now blanket the rest of the world with lesser buildings
    for(x = 0; x < WORLD_SIZE; ++x) {
        for(y = 0; y < WORLD_SIZE; ++y) {
            if(world[CLAMP(x, 0, WORLD_SIZE)][CLAMP(y, 0, WORLD_SIZE)]) {
                continue;
            }

            width = 12 + RandomVal(20);
            depth = 12 + RandomVal(20);
            height = MIN(width, depth);

            if((x < 30)
               || (y < 30)
               || (x > (WORLD_SIZE - 30))
               || (y > (WORLD_SIZE - 30))) {
                height = RandomVal(15 + 20);
            }
            else if(x < (WORLD_HALF / 2)) {
                height /= 2;
            }
            
            while((width > 8) && (depth > 8)) {
                if(!claimed(x, y, width, depth)) {
                    claim(x, y, width, depth, CLAIM_BUILDING);
                    building_color = WorldLightColor(RandomVal());

                    // If we're out of the host zone use simple builings
                    if((x < hot_zone.get_min().get_x())
                       || (x > hot_zone.get_max().get_x())
                       || (y < hot_zone.get_min().get_z())
                       || (y > hot_zone.get_max().get_z())) {
                        height = 5 + RandomVal(height) + RandomVal(height);

                        new Building(BUILDING_SIMPLE,
                                     x + 1,
                                     y + 1,
                                     height,
                                     width - 2,
                                     depth - 2,
                                     RandomVal(),
                                     building_color);
                    }
                    else {
                        // Use fancy buildings.
                        height = 15 + RandomVal(15);
                        width -= 2;
                        depth -= 2;
                        if(COIN_FLIP) {
                            new Building(BUILDING_TOWER,
                                         x + 1,
                                         y + 1,
                                         height,
                                         width,
                                         depth,
                                         RandomVal(),
                                         building_color);
                        }
                        else {
                            new Building(BUILDING_BLOCKY,
                                         x + 1,
                                         y + 1,
                                         height,
                                         width - 2,
                                         depth - 2,
                                         RandomVal(),
                                         building_color);
                        }
                    }
                    
                    break;
                }

                width--;
                depth--;
            }

            // Leave big gaps near the edge of the map, 
            // no need to pack detail there.
            if((y < WORLD_EDGE) || (y > (WORLD_SIZE - WORLD_EDGE))) {
                y += 32;
            }
        }

        // Leave big gaps near the edge of the map
        if((x < WORLD_EDGE) || (x > (WORLD_SIZE - WORLD_EDGE))) {
            x += 28;
        }
    }
}

// This will return a random color which is suitable for light sources, taken
// from a narrow group of hues. (Yellows, oranges, blues.)
gl_rgba WorldLightColor(unsigned int index)
{
    index %= LIGHT_COLOR_COUNT;
    gl_rgba temp;
    return temp.from_hsl(light_colors[index].hue,
                         light_colors[index].sat,
                         light_colors[index].lum);
}

char WorldCell(int x, int y)
{
    int x_index = CLAMP(x, 0, WORLD_SIZE - 1);
    int y_index = CLAMP(y, 0, WORLD_SIZE - 1);

    return world[x_index][y_index];
}

gl_rgba WorldBloomColor()
{
    return bloom_color;
}

int WorldLogoIndex()
{
    return logo_index++;
}


gl_bbox WorldHotZone()
{
    return hot_zone;
}

void WorldTerm(void)
{
}

void WorldReset(void)
{
    // If we're already fading out, then this is the developer
    // hammering on the "rebuild" button. Let's hurry up for the
    // nice man...
    if(fade_state == FADE_OUT) {
        do_reset();
    }

    // If reset is called but the world isn't ready, then don't
    // bother fading out. The program probably just started.
    fade_state = FADE_OUT;
    fade_start = SDL_GetTicks();
}

void WorldRender()
{
    if(!SHOW_DEBUG_GROUND) {
        return;
    }

    // Render a single texture over the city that shows
    // traffic lances
    glDepthMask(false);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBegin(GL_QUADS);

    glTexCoord2f(0, 0);
    glVertex3f(0, 0, 0);
    
    glTexCoord2f(0, 1);
    glVertex3f(0, 0, 1024);

    glTexCoord2f(1, 1);
    glVertex3f(1024, 0, 1024);
    
    glTexCoord2f(1, 0);
    glVertex3f(1024, 0, 0);

    glEnd();

    glDepthMask(true);
}

float WorldFade(void)
{
    return fade_current;
}

int WorldSceneBegin()
{
    return scene_begin;
}

// How long since this current iteration of the city went
// on display
int WorldSceneElapsed()
{
    int elapsed;

    if(!EntityReady() || !WorldSceneBegin()) {
        elapsed = 1;
    }
    else {
        elapsed = SDL_GetTicks() - WorldSceneBegin();
    }

    elapsed = MAX(elapsed, 1);

    return elapsed;
}

void WorldUpdate(void)
{
    unsigned fade_delta;
    int now;

    now = SDL_GetTicks();
    if(reset_needed) {
        // Now we've faded out the scene, rebuild it
        do_reset();
    }
    
    if(fade_state != FADE_IDLE) {
        if((fade_state == FADE_WAIT) && TextureReady() && EntityReady()) {
            fade_state = FADE_IN;
            fade_start = now;
            fade_current = 1.0f;
        }
        
        fade_delta = now - fade_start;
        
        // See if we're done fading in or out
        if((fade_delta > FADE_TIME) && (fade_state != FADE_WAIT)) {
            if(fade_state == FADE_OUT) {
                reset_needed = true;
                fade_state = FADE_WAIT;
                fade_current = 1.0f;
            }
            else {
                fade_start = FADE_IDLE;
                fade_current = 0.0f;
                start_time = time(NULL);
                scene_begin = SDL_GetTicks();
            }
        }
        else {
            fade_current = (float)fade_delta / FADE_TIME;
            if(fade_state == FADE_IN) {
                fade_current = 1.0f - fade_current;
            }

            if(fade_start == FADE_WAIT) {
                fade_current = 1.0f;
            }
        }

        if(!TextureReady()) {
            fade_current = 1.0f;
        }
    }

    if((fade_state == FADE_IDLE) && !TextureReady()) {
        fade_state = FADE_IN;
        fade_start = now;
    }

    if((fade_state == FADE_IDLE) && (WorldSceneElapsed() > RESET_INTERVAL)) {
        WorldReset();
    }
}

void WorldInit(void)
{
    last_update = SDL_GetTicks();
    for(int i = 0; i < CARS; ++i) {
        new Car();
    }

    sky = new Sky();
    WorldReset();
    fade_state = FADE_OUT;
    fade_start = 0;
}
