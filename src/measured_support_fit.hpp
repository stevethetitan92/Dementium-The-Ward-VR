#pragma once
namespace measuredSupport {
struct Fit {int weapon;XrPosef wrist;XrVector3f contact;};
inline constexpr Fit fits[]={
    {2,{{-0.1428198069f,-0.0928111523f,0.9846513271f,0.0380859710f},{-0.0412597656f,-0.0635737181f,-0.2296781540f}},{-0.0059034170f,-0.0359914612f,-0.2833872458f}},
    {3,{{-0.1428198069f,-0.0928111523f,0.9846513271f,0.0380859710f},{-0.0412597656f,-0.0635737181f,-0.2296781540f}},{-0.0062519416f,-0.0359941202f,-0.2833704030f}},
    {4,{{-0.1672562063f,-0.1741647422f,0.9704082012f,0.0001164856f},{-0.0397949219f,-0.0811874866f,-0.2318314314f}},{-0.0096354291f,-0.0429777892f,-0.2967464486f}},
    {5,{{0,0,0,1},{0,0,0}},{-.10489406f,-.00401486f,-.23116244f}}, // saved support surface centroid
};
inline const Fit* find(int weapon){for(const auto& f:fits)if(f.weapon==weapon)return &f;return nullptr;}
}
