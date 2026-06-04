#include "Config.h"

// ============================================================================
// Mutable globals — default values match k-prefixed constants
// ============================================================================
float gEnterExitDurationSec = 0.25f;   // uDWM binary: 0.25
float gRotateListDurationSec = 0.175f;  // uDWM binary: 0.175
float gRotateToHomeMaxDurationSec = 0.75f; // uDWM binary: 0.75
float gNearPlaneEdgeSize = kNearPlaneEdgeSize;
float gFinalMinRectWidthPercentage = kFinalMinRectWidthPercentage;
float gFinalMinRectLeftPercentage = kFinalMinRectLeftPercentage;
int gMaxVisibleCards = kMaxVisibleCards;
std::array<float, 3> gNormalizationBezier = kNormalizationBezier;
std::array<XMFLOAT3, 3> gBezierControls = {
    kBezierControls[0],
    kBezierControls[1],
    kBezierControls[2],
};
XMFLOAT3 gCameraFinalTranslate = kCameraFinalTranslate;
XMFLOAT3 gCameraFinalRotate = kCameraFinalRotate;
