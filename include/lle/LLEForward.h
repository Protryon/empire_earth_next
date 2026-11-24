// The Low-Level Engine's classes by name: one list for every header that only names them (LLE and UI floor lane,
// round 1), and the two forms a C++ class over a Rust object takes: URust, the owned object (its class is not copied),
// and URustView, a class that is the Rust object itself (never made or destroyed in C++). Requires nothing.
#ifndef LLE_LLEFORWARD_H
#define LLE_LLEFORWARD_H

class ESChecksum;
class FSFile;
class FSFileSpec;
class FSPath;
class GE2DPoint;
class GE3DLine;
class GE3DPlane;
class GE3DPoint;
class GEBitmap;
class GEBoundingBox;
class GEColor;
class GEFont;
class GELight;
class GELine;
class GEMaterial;
class GEModel;
class GEModelManager;
class GEOrientation;
class GEPhysicalModel;
class GEPoint;
class GEPolygonArray;
class GEQuaternion;
class GERasterizer;
class GERectangle;
class GERhombus;
class GEScreen;
class GESurface;
struct GESurfaceDescription;
class GETerrainMesh;
class GEText;
class GETexture;
class GETextureCoordinate;
struct GETextureDescription;
class GETextureManager;
class GETextureSet;
class GETransformation;
class GEVector;
class GEVertexArray;
class GEViewport;
class GEWindow;
class ISEntity;
class NEMessage;
class SEScore;
class SESoundManager;
class TSCriticalSection;
class TSEvent;
class TSReadWriteLock;
class TSThread;
template <class T> class U2DPoint;
class UIBackground;
class UIBackgroundTable;
class UIButton;
class UIComboBox;
class UIControl;
class UIControlsTable;
class UIEditBox;
class UIEntity;
class UIEntityGroup;
class UIEvent;
class UIForm;
class UIFormManager;
class UIHotKey;
class UILayoutUtility;
template <class T> struct UIListBoxRust;
class UIManager;
class UIRadioButton;
class UIScrollBar;
class UITextLabel;
class UResourceFile;
class UString;

template <class T> struct URust {
    URust(T* inObject = nullptr) : mObject(inObject) {}
    URust(const URust&) = delete;
    URust& operator=(const URust&) = delete;
    URust& operator=(T* inObject) { this->mObject = inObject; return *this; }
    operator T*() const { return this->mObject; }
    T* operator->() const { return this->mObject; }
    T* mObject;
};

struct URustView {
    URustView() = delete;
    ~URustView() = delete;
};

#endif
