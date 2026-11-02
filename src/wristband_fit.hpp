#pragma once
namespace wristband {
// Enclosing fit of the local 9mm left-hand wrist triangles, bone 11.
// Positive X=-native Y, Y=native Z, Z=-native X. Metres.
inline constexpr float fittedRadiusX=0.038412485f,fittedRadiusY=0.023203571f;
inline constexpr XrVector3f fittedCenter{0.001800000f,-0.008200000f,0.003000000f};
}
