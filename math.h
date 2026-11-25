#ifndef _math_h_
#define _math_h_

struct Vec3 {
    double_t x = 0.0, y = 0.0, z = 0.0;
  Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
  Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
  Vec3 operator*(double s) const { return { x * s, y * s, z * s }; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  Vec3& operator-=(const Vec3& o) { x -= o.x, y -= o.y, z -= o.z; return *this; }
  double_t magSq() const { return x * x + y * y + z * z; }
  double_t mag() const { return std::sqrt(magSq()); }
  double_t dot(const Vec3& o) const {return x*o.x + y*o.y + z*o.z;}
  Vec3 cross(const Vec3& o) const { return {y*o.z - z*o.y, z*o.x-x*o.z, x*o.y - y*o.x};}


};


#endif
