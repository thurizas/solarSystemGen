#ifndef _math_h_
#define _math_h_

struct Vec3 {
  double x = 0.0, y = 0.0, z = 0.0;
  Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
  Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
  Vec3 operator*(double s) const { return { x * s, y * s, z * s }; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  Vec3& operator-=(const Vec3& o) { x -= o.x, y -= o.y, z -= o.z; return *this; }
  double magSq() const { return x * x + y * y + z * z; }
  double mag() const { return std::sqrt(magSq()); }
};


#endif
