/*
 * Vector2.cpp
 *
 * 2006 Shamus Young
 * 
 * Functions for dealing with 2d (usually texture mapping) values
 *
 */

#include <cfloat>
#include <cmath>

#include <GL/gl.h>

#include "glTypes.hpp"
#include "Macro.hpp"
#include "Math.hpp"

GLvector2 glVectorNormalize(GLvector2 v)
{
    float length;

    length = glVectorLength(v);
    if(length < 0x000001f) {
        return v;
    }

    return (v * (1.0f / length));
}

void glVectorLength(GLvector2 v)
{
    return (float)sqrt((v.x * v.x) + (v.y * v.y));
}

GLvector2 glVectorSinCos(float a)
{
    GLvector2 val;

    a *= DEGREES_TO_RADIANS;
    val.x = sinf(a);
    val.y = cosf(a);

    return val;
}

GLvector2 glVector(float x, float y)
{
    GLvector2 val;

    val.x = x;
    val.y = y;

    return val;
}

GLvector2 glVectorAdd(GLvector2 val1, GLvector2 val2)
{
    GLvector2 result;

    result.x = val1.x + val2.x;
    result.y = val1.y + val2.y;

    return result;
}

GLvector2 glVectorInterpolate(GLvector2 v1, GLvector2 v2, float scalar)
{
    GLvector2 result;

    result.x = MathInterpolate(v1.x, v2.x, scalar);
    result.y = MathInterpolate(v1.y, v2.y, scalar);

    return result;
}

GLvector2 glVectorSubtract(GLvector2 val1, GLvector2 val2)
{
    GLvector2 result;

    result.x = val1.x - val2.x;
    result.y = val1.y - val2.y;

    return result;
}

GLvector2 GLvector2::operator+(GLvector2 const &c)
{
    return glVector(x + c.x, y + c.y);
}

GLvector2 GLvector2::operator+(float const &c)
{
    return glVector(x + c, y + c);
}

void GLvector2::operator+=(GLvector2 const &c)
{
    x += c.x;
    y += c.y;
}

void GLvector2::operator+=(float const &c)
{
    x += c;
    y += c;
}

GLvector2 GLvector2::operator-(GLvector2 const &c)
{
    return glVector(x - c.x, y - c.y);
}

GLvector2 GLvector2::operator-(float const &c)
{
    return glVector(x - c, y - c);
}

void GLvector2::operator-=(GLvector2 const &c)
{
    x -= c.x;
    y -= c.y;
}

void GLvector2::operator-=(float const &c)
{
    x -= c;
    y -= c;
}

GLvector2 GLvector2::operator*(GLvector2 const &c)
{
    return glVector(x * c.x, y * c.y);
}

GLvector2 GLvector2::operator*(float const &c)
{
    return glVector(x * c, y * c);
}

void GLvector2::operator*=(GLvector2 const &c)
{
    x *= c.x;
    y *= c.y;
}

void GLvector2::operator*=(float const &c)
{
    x *= c;
    y *= c;
}

GLvector2 GLvector2::operator/(GLvector2 const &c)
{
    return glVector(x / c.x, y / c.y);
}

GLvector2 GLvector2::operator/(float const &c)
{
    return glVector(x / c, y / c);
}

void GLvector2::operator/=(GLvector2 const &c)
{
    x /= c.x;
    y /= c.y;
}

void GLvector2::operator/=(float const &c)
{
    x /= c;
    y /= c;
}

bool GLvector2::operator==(GLvector2 const &c)
{
    return ((x == c.x) && (y == c.y));
}