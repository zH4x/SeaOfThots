#pragma once

struct Vec2f
{
    double x;
    double y;

    Vec2f(double x, double y) : x(x), y(y) { };

    Vec2f operator +(Vec2f other)
    {
        return Vec2f{ 
            x = this->x + other.x, 
            y = this->y + other.y,
        };
    }

    Vec2f operator *(Vec2f other)
    {
        return Vec2f{ 
            x = this->x * other.x, 
            y = this->y * other.y,
        };
    }

    Vec2f operator /(Vec2f other)
    {
        return Vec2f{ 
            x = this->x / other.x, 
            y = this->y / other.y,
        };
    }
};

struct Vec3f
{
    double x;
    double y;
    double z;

    Vec3f(double x, double y, double z) : x(x), y(y), z(z) { };

    Vec3f operator +(Vec3f other)
    {
        return Vec3f{ 
            x = this->x + other.x, 
            y = this->y + other.y, 
            z = this->z + other.z
        };
    }

    Vec3f operator *(Vec3f other)
    {
        return Vec3f{ 
            x = this->x * other.x, 
            y = this->y * other.y, 
            z = this->z * other.z
        };
    }

    Vec3f operator /(Vec3f other)
    {
        return Vec3f{ 
            x = this->x / other.x, 
            y = this->y / other.y, 
            z = this->z / other.z
        };
    }
};