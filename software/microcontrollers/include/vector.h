#ifndef VECTOR_H
#define VECTOR_H

struct Point {
    float x;
    float y;

    bool operator==(const Point &other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Point &other) const { return !(*this == other); }
};

class Vector {
  public:
    Vector(float angle = NAN, float distance = NAN);
    static Vector fromPoint(Point point);

    const float angle;
    const float distance;

    Vector operator=(const Vector &other) const;
    Vector operator+(const Vector &other) const;
    Vector operator-(const Vector &other) const;
    Vector operator*(const float other) const;
    Vector operator/(const float other) const;
};

#endif
