// clay.hpp — C++17 single-header port of clay.h v0.14
// Define CLAY_IMPLEMENTATION in exactly one translation unit before including.

#pragma once
#ifndef CLAY_HPP
#define CLAY_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <vector>
#include <functional>
#include <stdexcept>
#include <algorithm>
#include <climits>
#include <cfloat>

#if !defined(CLAY_DISABLE_SIMD) && (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64))
#include <emmintrin.h>
#elif !defined(CLAY_DISABLE_SIMD) && defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace clay {

// ─── Forward declarations ────────────────────────────────────────────────────
class Context;

// ─── Exceptions ──────────────────────────────────────────────────────────────
struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct TextMeasurementNotSet     : Error { using Error::Error; };
struct ElementsCapacityExceeded  : Error { using Error::Error; };
struct TextCacheCapacityExceeded : Error { using Error::Error; };
struct DuplicateId               : Error { using Error::Error; };
struct FloatingParentNotFound    : Error { using Error::Error; };
struct PercentageOver1           : Error { using Error::Error; };
struct InternalError             : Error { using Error::Error; };

// ─── Basic types ─────────────────────────────────────────────────────────────
struct Dimensions   { float width{}, height{}; };
struct Vector2      { float x{}, y{}; };
struct Color        { float r{}, g{}, b{}, a{}; };
struct BoundingBox  { float x{}, y{}, width{}, height{}; };
struct CornerRadius { float topLeft{}, topRight{}, bottomLeft{}, bottomRight{}; };

struct StringSlice {
    int32_t     length{};
    const char* chars{};
    const char* baseChars{};
};

// ─── Element ID ──────────────────────────────────────────────────────────────
struct ElementId {
    uint32_t    id{};
    uint32_t    offset{};
    uint32_t    baseId{};
    std::string_view stringId{};
};

// ─── Sizing ──────────────────────────────────────────────────────────────────
enum class SizingType : uint8_t { Fit, Grow, Percent, Fixed };

struct SizingMinMax { float min{}, max{}; };

struct SizingAxis {
    union {
        SizingMinMax minMax{};
        float        percent;
    } size;
    SizingType type{ SizingType::Fit };
};

struct Sizing {
    SizingAxis width{};
    SizingAxis height{};
};

namespace sizing {
    inline constexpr SizingAxis fit(float mn = 0, float mx = FLT_MAX) noexcept {
        SizingAxis a{}; a.size.minMax = {mn, mx}; a.type = SizingType::Fit; return a;
    }
    inline constexpr SizingAxis grow(float mn = 0, float mx = FLT_MAX) noexcept {
        SizingAxis a{}; a.size.minMax = {mn, mx}; a.type = SizingType::Grow; return a;
    }
    inline constexpr SizingAxis fixed(float v) noexcept {
        SizingAxis a{}; a.size.minMax = {v, v}; a.type = SizingType::Fixed; return a;
    }
    inline constexpr SizingAxis percent(float p) noexcept {
        SizingAxis a{}; a.size.percent = p; a.type = SizingType::Percent; return a;
    }
}

// ─── Padding / alignment ─────────────────────────────────────────────────────
struct Padding { uint16_t left{}, right{}, top{}, bottom{}; };

inline constexpr Padding paddingAll(uint16_t v) noexcept { return {v,v,v,v}; }

enum class LayoutDirection : uint8_t { LeftToRight, TopToBottom };
enum class AlignX : uint8_t { Left, Right, Center };
enum class AlignY : uint8_t { Top, Bottom, Center };

struct ChildAlignment { AlignX x{ AlignX::Left }; AlignY y{ AlignY::Top }; };

struct LayoutConfig {
    Sizing          sizing{};
    Padding         padding{};
    uint16_t        childGap{};
    ChildAlignment  childAlignment{};
    LayoutDirection layoutDirection{ LayoutDirection::LeftToRight };
};

// ─── Text config ─────────────────────────────────────────────────────────────
enum class TextWrapMode : uint8_t { Words, Newlines, None };
enum class TextAlignment : uint8_t { Left, Center, Right };

struct TextElementConfig {
    void*         userData{};
    Color         textColor{};
    uint16_t      fontId{};
    uint16_t      fontSize{};
    uint16_t      letterSpacing{};
    uint16_t      lineHeight{};
    TextWrapMode  wrapMode{ TextWrapMode::Words };
    TextAlignment textAlignment{ TextAlignment::Left };
};

// ─── Element configs ─────────────────────────────────────────────────────────
struct AspectRatioElementConfig { float aspectRatio{}; };
struct ImageElementConfig       { void* imageData{}; };

enum class FloatingAttachPoint : uint8_t {
    LeftTop, LeftCenter, LeftBottom,
    CenterTop, CenterCenter, CenterBottom,
    RightTop, RightCenter, RightBottom
};
enum class PointerCaptureMode  : uint8_t { Capture, Passthrough };
enum class FloatingAttachTo    : uint8_t { None, Parent, ElementWithId, Root };
enum class FloatingClipTo      : uint8_t { None, AttachedParent };

struct FloatingAttachPoints {
    FloatingAttachPoint element{ FloatingAttachPoint::LeftTop };
    FloatingAttachPoint parent { FloatingAttachPoint::LeftTop };
};

struct FloatingElementConfig {
    Vector2             offset{};
    Dimensions          expand{};
    uint32_t            parentId{};
    int16_t             zIndex{};
    FloatingAttachPoints attachPoints{};
    PointerCaptureMode  pointerCaptureMode{ PointerCaptureMode::Capture };
    FloatingAttachTo    attachTo{ FloatingAttachTo::None };
    FloatingClipTo      clipTo{ FloatingClipTo::None };
};

struct CustomElementConfig { void* customData{}; };

struct ClipElementConfig {
    bool    horizontal{};
    bool    vertical{};
    Vector2 childOffset{};
};

struct BorderWidth {
    uint16_t left{}, right{}, top{}, bottom{}, betweenChildren{};
};

inline constexpr BorderWidth borderOutside(uint16_t w) noexcept { return {w,w,w,w,0}; }
inline constexpr BorderWidth borderAll    (uint16_t w) noexcept { return {w,w,w,w,w}; }
inline constexpr CornerRadius cornerRadius(float r)    noexcept { return {r,r,r,r}; }

struct BorderElementConfig { Color color{}; BorderWidth width{}; };

// ─── Element declaration ─────────────────────────────────────────────────────
struct ElementDeclaration {
    ElementId             id{};
    LayoutConfig          layout{};
    Color                 backgroundColor{};
    CornerRadius          cornerRadius{};
    AspectRatioElementConfig aspectRatio{};
    ImageElementConfig    image{};
    FloatingElementConfig floating{};
    CustomElementConfig   custom{};
    ClipElementConfig     clip{};
    BorderElementConfig   border{};
    void*                 userData{};
};

// ─── Compile-time ID hashing ─────────────────────────────────────────────────
namespace detail {
    inline constexpr uint32_t hashStringCT(std::string_view s, uint32_t offset = 0, uint32_t seed = 0) noexcept {
        uint32_t base = seed;
        for (char c : s) {
            base += static_cast<uint8_t>(c);
            base += (base << 10);
            base ^= (base >> 6);
        }
        uint32_t hash = base;
        hash += offset;
        hash += (hash << 10);
        hash ^= (hash >> 6);
        hash += (hash << 3);
        base += (base << 3);
        hash ^= (hash >> 11);
        base ^= (base >> 11);
        hash += (hash << 15);
        base += (base << 15);
        return hash + 1;
    }
} // namespace detail

inline ElementId makeId(std::string_view label, uint32_t index = 0, uint32_t seed = 0) noexcept {
    ElementId eid;
    eid.stringId = label;
    eid.offset   = index;
    eid.baseId   = seed;
    eid.id       = detail::hashStringCT(label, index, seed);
    return eid;
}

inline namespace literals {
    inline ElementId operator""_id(const char* s, std::size_t n) noexcept {
        return makeId(std::string_view(s, n));
    }
}

// ─── Pointer data ─────────────────────────────────────────────────────────────
enum class PointerState : uint8_t {
    PressedThisFrame, Pressed, ReleasedThisFrame, Released
};
struct PointerData { Vector2 position{}; PointerState state{ PointerState::Released }; };

// ─── Render commands ─────────────────────────────────────────────────────────
struct TextRenderData {
    StringSlice stringContents{};
    Color       textColor{};
    uint16_t    fontId{};
    uint16_t    fontSize{};
    uint16_t    letterSpacing{};
    uint16_t    lineHeight{};
};
struct RectangleRenderData { Color backgroundColor{}; CornerRadius cornerRadius{}; };
struct ImageRenderData     { Color backgroundColor{}; CornerRadius cornerRadius{}; void* imageData{}; };
struct CustomRenderData    { Color backgroundColor{}; CornerRadius cornerRadius{}; void* customData{}; };
struct ClipRenderData      { bool horizontal{}; bool vertical{}; };
struct BorderRenderData    { Color color{}; CornerRadius cornerRadius{}; BorderWidth width{}; };

union RenderData {
    RectangleRenderData rectangle{};
    TextRenderData      text;
    ImageRenderData     image;
    CustomRenderData    custom;
    BorderRenderData    border;
    ClipRenderData      clip;
};

enum class RenderCommandType : uint8_t {
    None, Rectangle, Border, Text, Image,
    ScissorStart, ScissorEnd, Custom
};

struct RenderCommand {
    BoundingBox       boundingBox{};
    RenderData        renderData{};
    void*             userData{};
    uint32_t          id{};
    int16_t           zIndex{};
    RenderCommandType commandType{ RenderCommandType::None };
};

// ─── Scroll / element data ────────────────────────────────────────────────────
struct ScrollContainerData {
    Vector2*          scrollPosition{};
    Dimensions        scrollContainerDimensions{};
    Dimensions        contentDimensions{};
    ClipElementConfig config{};
    bool              found{};
};

struct ElementData {
    BoundingBox boundingBox{};
    bool        found{};
};

// ─── Measure-text callback type ───────────────────────────────────────────────
using MeasureTextFn = std::function<Dimensions(StringSlice, const TextElementConfig*, void*)>;
using QueryScrollFn = std::function<Vector2(uint32_t, void*)>;
using OnHoverFn     = std::function<void(ElementId, PointerData)>;

// ═══════════════════════════════════════════════════════════════════════════════
//  Internal implementation types (kept in clay:: namespace, prefixed with I_)
// ═══════════════════════════════════════════════════════════════════════════════
namespace internal {

static constexpr float MAXFLOAT_VAL = FLT_MAX;
static constexpr float EPSILON      = 0.01f;

template<class T>
inline T clay_max(T a, T b) { return a > b ? a : b; }
template<class T>
inline T clay_min(T a, T b) { return a < b ? a : b; }

// ── Small typed array backed by std::vector ──────────────────────────────────
// We keep raw std::vector<T> everywhere — this just aliases usage.

// ── Hashing (runtime) ────────────────────────────────────────────────────────
inline ElementId hashNumber(uint32_t offset, uint32_t seed) noexcept {
    uint32_t h = seed;
    h += (offset + 48);
    h += (h << 10); h ^= (h >> 6);
    h += (h << 3);  h ^= (h >> 11); h += (h << 15);
    return ElementId{ h + 1, offset, seed, {} };
}

inline ElementId hashString(std::string_view key, uint32_t offset, uint32_t seed) noexcept {
    uint32_t base = seed;
    for (unsigned char c : key) {
        base += c; base += (base<<10); base ^= (base>>6);
    }
    uint32_t hash = base;
    hash += offset; hash += (hash<<10); hash ^= (hash>>6);
    hash += (hash<<3); base += (base<<3);
    hash ^= (hash>>11); base ^= (base>>11);
    hash += (hash<<15); base += (base<<15);
    return ElementId{ hash+1, offset, base+1, key };
}

#if !defined(CLAY_DISABLE_SIMD) && (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64))
inline bool memEq(const char* a, const char* b, int32_t len) {
    while (len >= 16) {
        __m128i va = _mm_loadu_si128((const __m128i*)a);
        __m128i vb = _mm_loadu_si128((const __m128i*)b);
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(va, vb)) != 0xFFFF) return false;
        a += 16; b += 16; len -= 16;
    }
    while (len--) { if (*a++ != *b++) return false; }
    return true;
}
#else
inline bool memEq(const char* a, const char* b, int32_t len) {
    return std::memcmp(a, b, (size_t)len) == 0;
}
#endif

// ── Internal config type tag ──────────────────────────────────────────────────
enum class ConfigType : uint8_t {
    None, Border, Floating, Clip, Aspect, Image, Text, Custom, Shared
};

struct SharedElementConfig { Color backgroundColor{}; CornerRadius cornerRadius{}; void* userData{}; };

union ConfigUnion {
    TextElementConfig*        textElementConfig{};
    AspectRatioElementConfig* aspectRatioElementConfig;
    ImageElementConfig*       imageElementConfig;
    FloatingElementConfig*    floatingElementConfig;
    CustomElementConfig*      customElementConfig;
    ClipElementConfig*        clipElementConfig;
    BorderElementConfig*      borderElementConfig;
    SharedElementConfig*      sharedElementConfig;
};

struct ElementConfig { ConfigType type{ ConfigType::None }; ConfigUnion config{}; };

// ── Text measurement cache ────────────────────────────────────────────────────
struct MeasuredWord {
    int32_t startOffset{};
    int32_t length{};
    float   width{};
    int32_t next{ -1 };
};

struct MeasureTextCacheItem {
    Dimensions unwrappedDimensions{};
    int32_t    measuredWordsStartIndex{ -1 };
    float      minWidth{};
    bool       containsNewlines{};
    uint32_t   id{};
    int32_t    nextIndex{};
    uint32_t   generation{};
};

struct WrappedTextLine {
    Dimensions       dimensions{};
    std::string_view line{};
};

struct TextElementData {
    std::string_view  text{};
    Dimensions        preferredDimensions{};
    int32_t           elementIndex{};
    int32_t           wrappedLinesStart{};
    int32_t           wrappedLinesLength{};
};

// ── Layout element ────────────────────────────────────────────────────────────
struct LayoutElementChildren { int32_t* elements{}; uint16_t length{}; };

struct LayoutElement {
    union {
        LayoutElementChildren children{};
        TextElementData*      textElementData;
    } childrenOrTextContent;

    Dimensions   dimensions{};
    Dimensions   minDimensions{};
    LayoutConfig* layoutConfig{};

    int32_t elementConfigsStart{};
    int32_t elementConfigsLength{};

    uint32_t id{};
};

// ── Hash map item ─────────────────────────────────────────────────────────────
struct DebugElementData { bool collision{}; bool collapsed{}; };

struct LayoutElementHashMapItem {
    BoundingBox  boundingBox{};
    ElementId    elementId{};
    LayoutElement* layoutElement{};
    OnHoverFn    onHoverFn{};
    int32_t      nextIndex{ -1 };
    uint32_t     generation{};
    uint32_t     idAlias{};
    DebugElementData* debugData{};
};

// ── Scroll container internal ─────────────────────────────────────────────────
struct ScrollContainerDataInternal {
    LayoutElement* layoutElement{};
    BoundingBox    boundingBox{};
    Dimensions     contentSize{};
    Vector2        scrollOrigin{ -1,-1 };
    Vector2        pointerOrigin{};
    Vector2        scrollMomentum{};
    Vector2        scrollPosition{};
    Vector2        previousDelta{};
    float          momentumTime{};
    uint32_t       elementId{};
    bool           openThisFrame{};
    bool           pointerScrollActive{};
};

// ── Tree roots ────────────────────────────────────────────────────────────────
struct LayoutElementTreeRoot {
    int32_t  layoutElementIndex{};
    uint32_t parentId{};
    uint32_t clipElementId{};
    int16_t  zIndex{};
    Vector2  pointerOffset{};
};

struct LayoutElementTreeNode {
    LayoutElement* layoutElement{};
    Vector2        position{};
    Vector2        nextChildOffset{};
};

} // namespace internal

// ═══════════════════════════════════════════════════════════════════════════════
//  Context — holds all state, replaces global Clay__currentContext
// ═══════════════════════════════════════════════════════════════════════════════
class Context {
public:
    // Construction ─────────────────────────────────────────────────────────────
    explicit Context(Dimensions layoutDimensions,
                     int32_t    maxElements    = 8192,
                     int32_t    maxTextCache   = 16384)
        : layoutDimensions_(layoutDimensions)
        , maxElementCount_(maxElements)
        , maxMeasureTextCacheWordCount_(maxTextCache)
    {
        initPersistentMemory();
    }

    // ── Public configuration ───────────────────────────────────────────────────
    void setMeasureTextFunction(MeasureTextFn fn, void* userData = nullptr) {
        measureTextFn_      = std::move(fn);
        measureTextUserData_ = userData;
    }
    void setQueryScrollOffsetFunction(QueryScrollFn fn, void* userData = nullptr) {
        queryScrollFn_           = std::move(fn);
        queryScrollUserData_     = userData;
        externalScrollHandling_  = static_cast<bool>(queryScrollFn_);
    }
    void setLayoutDimensions(Dimensions d) { layoutDimensions_ = d; }
    void setDebugModeEnabled(bool v)       { debugModeEnabled_ = v; }
    bool isDebugModeEnabled() const        { return debugModeEnabled_; }
    void setCullingEnabled(bool v)         { disableCulling_ = !v; }

    // ── Frame API ──────────────────────────────────────────────────────────────
    void setPointerState(Vector2 position, bool pointerDown);
    void updateScrollContainers(bool enableDrag, Vector2 scrollDelta, float deltaTime);
    Vector2 getScrollOffset();

    void beginLayout();
    std::vector<RenderCommand>& endLayout();

    // ── Element building (lambda style) ───────────────────────────────────────
    void element(const ElementDeclaration& decl, std::function<void()> children = {}) {
        openElement();
        configureOpenElement(decl);
        if (children) children();
        closeElement();
    }

    void text(std::string_view txt, const TextElementConfig* cfg) {
        openTextElement(txt, cfg);
    }
    void text(std::string_view txt, const TextElementConfig& cfg) {
        openTextElement(txt, &cfg);
    }

    // ── Low-level imperative element building (matches old C API style) ───────
    void openElement();
    void configureOpenElement(const ElementDeclaration& decl);
    void closeElement();
    void openTextElement(std::string_view text, const TextElementConfig* cfg);

    // ── Convenience: store a text config and return pointer ───────────────────
    TextElementConfig* storeTextConfig(TextElementConfig cfg) {
        if (boolWarn_.maxElementsExceeded) return &textConfigDefault_;
        textElementConfigs_.push_back(cfg);
        return &textElementConfigs_.back();
    }

    // ── Interaction queries ───────────────────────────────────────────────────
    bool hovered();
    void onHover(OnHoverFn fn);
    bool pointerOver(ElementId id) const;
    const std::vector<ElementId>& getPointerOverIds() const { return pointerOverIds_; }

    ElementData           getElementData(ElementId id) const;
    ScrollContainerData   getScrollContainerData(ElementId id);

    // ── ID helpers ────────────────────────────────────────────────────────────
    ElementId getElementId(std::string_view s) const {
        return internal::hashString(s, 0, 0);
    }
    ElementId getElementIdWithIndex(std::string_view s, uint32_t idx) const {
        return internal::hashString(s, idx, 0);
    }
    uint32_t getParentElementId() const;

    // ── Debug ─────────────────────────────────────────────────────────────────
    void resetMeasureTextCache();

private:
    // ── Private internal helpers ──────────────────────────────────────────────
    void initPersistentMemory();
    void initEphemeralMemory();

    internal::LayoutElement* getOpenLayoutElement();
    internal::LayoutElement* getLayoutElement(int32_t idx);

    ElementId attachId(ElementId eid);
    ElementId generateIdForAnonymousElement(internal::LayoutElement* el);

    void attachElementConfig(internal::ConfigUnion cfg, internal::ConfigType type);
    internal::ConfigUnion findElementConfigWithType(internal::LayoutElement* el,
                                                     internal::ConfigType type) const;
    bool elementHasConfig(internal::LayoutElement* el, internal::ConfigType type) const;

    internal::LayoutElementHashMapItem* addHashMapItem(ElementId eid,
                                                        internal::LayoutElement* el,
                                                        uint32_t alias);
    internal::LayoutElementHashMapItem* getHashMapItem(uint32_t id);

    void addRenderCommand(RenderCommand cmd);
    bool elementIsOffscreen(const BoundingBox& bb) const;

    internal::MeasureTextCacheItem* measureTextCached(std::string_view text,
                                                       const TextElementConfig* cfg);
    internal::MeasuredWord* addMeasuredWord(internal::MeasuredWord word,
                                             internal::MeasuredWord* prev);
    uint32_t hashStringContentsWithConfig(std::string_view text,
                                           const TextElementConfig* cfg) const;

    void sizeContainersAlongAxis(bool xAxis);
    void calculateFinalLayout();
    void updateAspectRatioBox(internal::LayoutElement* el);

    std::string_view intToString(int32_t v);

    bool floatEqual(float a, float b) const { return (a-b) < EPSILON && (a-b) > -EPSILON; }

    // Debug view
    void renderDebugView();

    // ── State ─────────────────────────────────────────────────────────────────
    static constexpr float EPSILON = internal::EPSILON;

    int32_t    maxElementCount_;
    int32_t    maxMeasureTextCacheWordCount_;
    bool       debugModeEnabled_{};
    bool       disableCulling_{};
    bool       externalScrollHandling_{};
    uint32_t   debugSelectedElementId_{};
    uint32_t   generation_{};

    Dimensions  layoutDimensions_{};
    PointerData pointerInfo_{};

    MeasureTextFn measureTextFn_{};
    void*         measureTextUserData_{};
    QueryScrollFn queryScrollFn_{};
    void*         queryScrollUserData_{};

    struct BoolWarn {
        bool maxElementsExceeded{};
        bool maxRenderCommandsExceeded{};
        bool maxTextMeasureCacheExceeded{};
        bool textMeasurementFunctionNotSet{};
    } boolWarn_{};

    // Per-frame (ephemeral) storage
    std::vector<internal::LayoutElement>               layoutElements_;
    std::vector<RenderCommand>                         renderCommands_;
    std::vector<int32_t>                               openLayoutElementStack_;
    std::vector<int32_t>                               layoutElementChildren_;
    std::vector<int32_t>                               layoutElementChildrenBuffer_;
    std::vector<internal::TextElementData>             textElementData_;
    std::vector<int32_t>                               aspectRatioElementIndexes_;
    std::vector<int32_t>                               layoutElementClipElementIds_;
    std::vector<int32_t>                               openClipElementStack_;
    std::vector<int32_t>                               reusableElementIndexBuffer_;

    // Config pools (ephemeral)
    std::vector<LayoutConfig>                          layoutConfigs_;
    std::vector<internal::ElementConfig>               elementConfigs_;
    std::vector<TextElementConfig>                     textElementConfigs_;
    std::vector<AspectRatioElementConfig>              aspectRatioElementConfigs_;
    std::vector<ImageElementConfig>                    imageElementConfigs_;
    std::vector<FloatingElementConfig>                 floatingElementConfigs_;
    std::vector<ClipElementConfig>                     clipElementConfigs_;
    std::vector<CustomElementConfig>                   customElementConfigs_;
    std::vector<BorderElementConfig>                   borderElementConfigs_;
    std::vector<internal::SharedElementConfig>         sharedElementConfigs_;
    std::vector<std::string_view>                      layoutElementIdStrings_;
    std::vector<internal::WrappedTextLine>             wrappedTextLines_;
    std::vector<internal::LayoutElementTreeNode>       layoutElementTreeNodeArray_;
    std::vector<internal::LayoutElementTreeRoot>       layoutElementTreeRoots_;
    std::vector<bool>                                  treeNodeVisited_;
    std::vector<char>                                  dynamicStringData_;

    // Persistent storage (survives frames)
    std::vector<internal::ScrollContainerDataInternal> scrollContainerDatas_;
    std::vector<internal::LayoutElementHashMapItem>    layoutElementsHashMapInternal_;
    std::vector<int32_t>                               layoutElementsHashMap_;
    std::vector<internal::MeasureTextCacheItem>        measureTextHashMapInternal_;
    std::vector<int32_t>                               measureTextHashMapInternalFreeList_;
    std::vector<int32_t>                               measureTextHashMap_;
    std::vector<internal::MeasuredWord>                measuredWords_;
    std::vector<int32_t>                               measuredWordsFreeList_;
    std::vector<ElementId>                             pointerOverIds_;
    std::vector<internal::DebugElementData>            debugElementData_;

    // Defaults (used as fallback returns)
    TextElementConfig           textConfigDefault_{};
    LayoutConfig                layoutConfigDefault_{};
    internal::SharedElementConfig sharedConfigDefault_{};
    internal::LayoutElementHashMapItem hashMapItemDefault_{};

    // Debug
    static constexpr uint32_t DEBUG_VIEW_WIDTH = 400;
    Color debugHighlightColor_{ 168, 66, 28, 100 };
    bool  warningsEnabled_{ true };
};

} // namespace clay

// ═══════════════════════════════════════════════════════════════════════════════
//  IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════════
#ifdef CLAY_IMPLEMENTATION

namespace clay {

using namespace internal;

// ── initPersistentMemory ──────────────────────────────────────────────────────
void Context::initPersistentMemory() {
    int32_t cap = maxElementCount_;
    int32_t tcap = maxMeasureTextCacheWordCount_;

    layoutElementsHashMapInternal_.reserve(cap);
    layoutElementsHashMap_.assign(cap, -1);
    measureTextHashMapInternal_.reserve(cap);
    measureTextHashMapInternal_.resize(1); // index 0 = "null"
    measureTextHashMapInternalFreeList_.reserve(cap);
    measureTextHashMap_.assign(cap / 32 + 1, 0);
    measuredWords_.reserve(tcap);
    measuredWordsFreeList_.reserve(tcap);
    pointerOverIds_.reserve(cap);
    debugElementData_.reserve(cap);
    scrollContainerDatas_.reserve(10);
}

// ── initEphemeralMemory ───────────────────────────────────────────────────────
void Context::initEphemeralMemory() {
    int32_t cap = maxElementCount_;

    layoutElements_.clear();          layoutElements_.reserve(cap);
    renderCommands_.clear();          renderCommands_.reserve(cap);
    openLayoutElementStack_.clear();  openLayoutElementStack_.reserve(cap);
    layoutElementChildren_.clear();   layoutElementChildren_.reserve(cap);
    layoutElementChildrenBuffer_.clear(); layoutElementChildrenBuffer_.reserve(cap);
    textElementData_.clear();         textElementData_.reserve(cap);
    aspectRatioElementIndexes_.clear();
    layoutElementClipElementIds_.clear(); layoutElementClipElementIds_.resize(cap, 0);
    openClipElementStack_.clear();
    reusableElementIndexBuffer_.clear(); reusableElementIndexBuffer_.reserve(cap);

    layoutConfigs_.clear();           layoutConfigs_.reserve(cap);
    elementConfigs_.clear();          elementConfigs_.reserve(cap);
    textElementConfigs_.clear();      textElementConfigs_.reserve(cap);
    aspectRatioElementConfigs_.clear(); aspectRatioElementConfigs_.reserve(cap);
    imageElementConfigs_.clear();       imageElementConfigs_.reserve(cap);
    floatingElementConfigs_.clear();    floatingElementConfigs_.reserve(cap);
    clipElementConfigs_.clear();        clipElementConfigs_.reserve(cap);
    customElementConfigs_.clear();      customElementConfigs_.reserve(cap);
    borderElementConfigs_.clear();      borderElementConfigs_.reserve(cap);
    sharedElementConfigs_.clear();      sharedElementConfigs_.reserve(cap);

    layoutElementIdStrings_.clear();  layoutElementIdStrings_.resize(cap);
    wrappedTextLines_.clear();        wrappedTextLines_.reserve(cap);
    layoutElementTreeNodeArray_.clear(); layoutElementTreeNodeArray_.reserve(cap);
    layoutElementTreeRoots_.clear();
    treeNodeVisited_.clear();         treeNodeVisited_.resize(cap, false);
    dynamicStringData_.clear();       dynamicStringData_.reserve(cap);
}

// ── helpers ───────────────────────────────────────────────────────────────────
LayoutElement* Context::getLayoutElement(int32_t idx) {
    if (idx < 0 || idx >= (int32_t)layoutElements_.size()) {
        throw InternalError("Clay: out of bounds layout element access");
    }
    return &layoutElements_[idx];
}

LayoutElement* Context::getOpenLayoutElement() {
    int32_t idx = openLayoutElementStack_.back();
    return getLayoutElement(idx);
}

uint32_t Context::getParentElementId() const {
    int32_t n = (int32_t)openLayoutElementStack_.size();
    if (n < 2) return 0;
    int32_t parentIdx = openLayoutElementStack_[n-2];
    return layoutElements_[parentIdx].id;
}

// ── Hash map ──────────────────────────────────────────────────────────────────
LayoutElementHashMapItem* Context::addHashMapItem(ElementId eid,
                                                   LayoutElement* el,
                                                   uint32_t alias)
{
    int32_t cap = (int32_t)layoutElementsHashMap_.size();
    uint32_t bucket = eid.id % (uint32_t)cap;
    int32_t prev = -1;
    int32_t cur  = layoutElementsHashMap_[bucket];

    while (cur != -1) {
        auto& item = layoutElementsHashMapInternal_[cur];
        if (item.elementId.id == eid.id) {
            if (item.generation <= generation_) {
                item.elementId   = eid;
                item.idAlias     = alias;
                item.generation  = generation_ + 1;
                item.layoutElement = el;
                if (item.debugData) item.debugData->collision = false;
                item.onHoverFn   = {};
            } else {
                throw DuplicateId("Clay: duplicate element ID in layout");
            }
            return &item;
        }
        prev = cur;
        cur  = item.nextIndex;
    }

    debugElementData_.push_back(DebugElementData{});
    LayoutElementHashMapItem newItem{};
    newItem.elementId    = eid;
    newItem.layoutElement = el;
    newItem.nextIndex    = -1;
    newItem.generation   = generation_ + 1;
    newItem.idAlias      = alias;
    newItem.debugData    = &debugElementData_.back();
    layoutElementsHashMapInternal_.push_back(newItem);
    int32_t newIdx = (int32_t)layoutElementsHashMapInternal_.size() - 1;
    if (prev != -1) layoutElementsHashMapInternal_[prev].nextIndex = newIdx;
    else            layoutElementsHashMap_[bucket] = newIdx;
    return &layoutElementsHashMapInternal_[newIdx];
}

LayoutElementHashMapItem* Context::getHashMapItem(uint32_t id) {
    if (layoutElementsHashMap_.empty()) return &hashMapItemDefault_;
    uint32_t bucket = id % (uint32_t)layoutElementsHashMap_.size();
    int32_t cur = layoutElementsHashMap_[bucket];
    while (cur != -1) {
        auto& item = layoutElementsHashMapInternal_[cur];
        if (item.elementId.id == id) return &item;
        cur = item.nextIndex;
    }
    return &hashMapItemDefault_;
}

// ── Config helpers ────────────────────────────────────────────────────────────
void Context::attachElementConfig(ConfigUnion cfg, ConfigType type) {
    if (boolWarn_.maxElementsExceeded) return;
    auto* el = getOpenLayoutElement();
    el->elementConfigsLength++;
    elementConfigs_.push_back(ElementConfig{ type, cfg });
}

ConfigUnion Context::findElementConfigWithType(LayoutElement* el, ConfigType type) const {
    for (int32_t i = 0; i < el->elementConfigsLength; ++i) {
        auto& ec = elementConfigs_[el->elementConfigsStart + i];
        if (ec.type == type) return ec.config;
    }
    return ConfigUnion{};
}

bool Context::elementHasConfig(LayoutElement* el, ConfigType type) const {
    for (int32_t i = 0; i < el->elementConfigsLength; ++i) {
        if (elementConfigs_[el->elementConfigsStart + i].type == type) return true;
    }
    return false;
}

void Context::updateAspectRatioBox(LayoutElement* el) {
    for (int32_t j = 0; j < el->elementConfigsLength; ++j) {
        auto& ec = elementConfigs_[el->elementConfigsStart + j];
        if (ec.type == ConfigType::Aspect) {
            float ar = ec.config.aspectRatioElementConfig->aspectRatio;
            if (ar == 0) break;
            if (el->dimensions.width == 0 && el->dimensions.height != 0)
                el->dimensions.width = el->dimensions.height * ar;
            else if (el->dimensions.width != 0 && el->dimensions.height == 0)
                el->dimensions.height = el->dimensions.width * (1.f / ar);
            break;
        }
    }
}

// ── openElement / closeElement ────────────────────────────────────────────────
void Context::openElement() {
    if ((int32_t)layoutElements_.size() >= maxElementCount_ - 1 ||
        boolWarn_.maxElementsExceeded) {
        boolWarn_.maxElementsExceeded = true;
        return;
    }
    layoutElements_.push_back(LayoutElement{});
    int32_t idx = (int32_t)layoutElements_.size() - 1;
    openLayoutElementStack_.push_back(idx);

    int32_t clipId = openClipElementStack_.empty() ? 0 : openClipElementStack_.back();
    if (idx < (int32_t)layoutElementClipElementIds_.size())
        layoutElementClipElementIds_[idx] = clipId;
}

void Context::closeElement() {
    if (boolWarn_.maxElementsExceeded) return;

    LayoutElement* el = getOpenLayoutElement();
    LayoutConfig*  lc = el->layoutConfig;

    bool clipH = false, clipV = false;
    for (int32_t i = 0; i < el->elementConfigsLength; ++i) {
        auto& ec = elementConfigs_[el->elementConfigsStart + i];
        if (ec.type == ConfigType::Clip) {
            clipH = ec.config.clipElementConfig->horizontal;
            clipV = ec.config.clipElementConfig->vertical;
            openClipElementStack_.pop_back();
            break;
        } else if (ec.type == ConfigType::Floating) {
            openClipElementStack_.pop_back();
        }
    }

    float lpw = (float)(lc->padding.left + lc->padding.right);
    float lph = (float)(lc->padding.top  + lc->padding.bottom);

    el->childrenOrTextContent.children.elements =
        layoutElementChildren_.data() + layoutElementChildren_.size();

    if (lc->layoutDirection == LayoutDirection::LeftToRight) {
        el->dimensions.width = lpw;
        el->minDimensions.width = lpw;
        for (int32_t i = 0; i < el->childrenOrTextContent.children.length; ++i) {
            int32_t off = (int32_t)layoutElementChildrenBuffer_.size()
                        - el->childrenOrTextContent.children.length + i;
            int32_t ci  = layoutElementChildrenBuffer_[off];
            LayoutElement* child = getLayoutElement(ci);
            el->dimensions.width += child->dimensions.width;
            el->dimensions.height = clay_max(el->dimensions.height,
                                             child->dimensions.height + lph);
            if (!clipH) el->minDimensions.width  += child->minDimensions.width;
            if (!clipV) el->minDimensions.height  = clay_max(el->minDimensions.height,
                                                    child->minDimensions.height + lph);
            layoutElementChildren_.push_back(ci);
        }
        float gap = (float)(clay_max(el->childrenOrTextContent.children.length - 1, 0)
                            * lc->childGap);
        el->dimensions.width += gap;
        if (!clipH) el->minDimensions.width += gap;
    } else {
        el->dimensions.height = lph;
        el->minDimensions.height = lph;
        for (int32_t i = 0; i < el->childrenOrTextContent.children.length; ++i) {
            int32_t off = (int32_t)layoutElementChildrenBuffer_.size()
                        - el->childrenOrTextContent.children.length + i;
            int32_t ci  = layoutElementChildrenBuffer_[off];
            LayoutElement* child = getLayoutElement(ci);
            el->dimensions.height += child->dimensions.height;
            el->dimensions.width   = clay_max(el->dimensions.width,
                                              child->dimensions.width + lpw);
            if (!clipV) el->minDimensions.height += child->minDimensions.height;
            if (!clipH) el->minDimensions.width   = clay_max(el->minDimensions.width,
                                                    child->minDimensions.width + lpw);
            layoutElementChildren_.push_back(ci);
        }
        float gap = (float)(clay_max(el->childrenOrTextContent.children.length - 1, 0)
                            * lc->childGap);
        el->dimensions.height += gap;
        if (!clipV) el->minDimensions.height += gap;
    }

    int32_t nch = el->childrenOrTextContent.children.length;
    for (int32_t i = 0; i < nch; ++i) layoutElementChildrenBuffer_.pop_back();

    // clamp width
    if (lc->sizing.width.type != SizingType::Percent) {
        if (lc->sizing.width.size.minMax.max <= 0)
            lc->sizing.width.size.minMax.max = FLT_MAX;
        el->dimensions.width = clay_min(clay_max(el->dimensions.width,
                                        lc->sizing.width.size.minMax.min),
                                        lc->sizing.width.size.minMax.max);
        el->minDimensions.width = clay_min(clay_max(el->minDimensions.width,
                                           lc->sizing.width.size.minMax.min),
                                           lc->sizing.width.size.minMax.max);
    } else {
        el->dimensions.width = 0;
    }
    // clamp height
    if (lc->sizing.height.type != SizingType::Percent) {
        if (lc->sizing.height.size.minMax.max <= 0)
            lc->sizing.height.size.minMax.max = FLT_MAX;
        el->dimensions.height = clay_min(clay_max(el->dimensions.height,
                                         lc->sizing.height.size.minMax.min),
                                         lc->sizing.height.size.minMax.max);
        el->minDimensions.height = clay_min(clay_max(el->minDimensions.height,
                                            lc->sizing.height.size.minMax.min),
                                            lc->sizing.height.size.minMax.max);
    } else {
        el->dimensions.height = 0;
    }

    updateAspectRatioBox(el);

    bool isFloating = elementHasConfig(el, ConfigType::Floating);
    int32_t closingIdx = openLayoutElementStack_.back();
    openLayoutElementStack_.pop_back();

    if (!isFloating && openLayoutElementStack_.size() > 1) {
        LayoutElement* parent = getOpenLayoutElement();
        parent->childrenOrTextContent.children.length++;
        layoutElementChildrenBuffer_.push_back(closingIdx);
    }
}

// ── configureOpenElement ──────────────────────────────────────────────────────
void Context::configureOpenElement(const ElementDeclaration& d) {
    LayoutElement* el = getOpenLayoutElement();

    layoutConfigs_.push_back(d.layout);
    el->layoutConfig = &layoutConfigs_.back();

    if ((d.layout.sizing.width.type  == SizingType::Percent && d.layout.sizing.width.size.percent  > 1) ||
        (d.layout.sizing.height.type == SizingType::Percent && d.layout.sizing.height.size.percent > 1))
        throw PercentageOver1("Clay: sizing percent > 1.0");

    el->elementConfigsStart  = (int32_t)elementConfigs_.size();
    el->elementConfigsLength = 0;

    ElementId openId = d.id;

    SharedElementConfig* sharedCfg = nullptr;
    if (d.backgroundColor.a > 0) {
        sharedElementConfigs_.push_back(SharedElementConfig{ d.backgroundColor });
        sharedCfg = &sharedElementConfigs_.back();
        ConfigUnion u{}; u.sharedElementConfig = sharedCfg;
        attachElementConfig(u, ConfigType::Shared);
    }

    static const CornerRadius zeroR{};
    if (std::memcmp(&d.cornerRadius, &zeroR, sizeof(CornerRadius)) != 0) {
        if (sharedCfg) {
            sharedCfg->cornerRadius = d.cornerRadius;
        } else {
            sharedElementConfigs_.push_back(SharedElementConfig{ {}, d.cornerRadius });
            sharedCfg = &sharedElementConfigs_.back();
            ConfigUnion u{}; u.sharedElementConfig = sharedCfg;
            attachElementConfig(u, ConfigType::Shared);
        }
    }
    if (d.userData) {
        if (sharedCfg) { sharedCfg->userData = d.userData; }
        else {
            sharedElementConfigs_.push_back(SharedElementConfig{ {}, {}, d.userData });
            sharedCfg = &sharedElementConfigs_.back();
            ConfigUnion u{}; u.sharedElementConfig = sharedCfg;
            attachElementConfig(u, ConfigType::Shared);
        }
    }

    if (d.image.imageData) {
        imageElementConfigs_.push_back(d.image);
        ConfigUnion u{}; u.imageElementConfig = &imageElementConfigs_.back();
        attachElementConfig(u, ConfigType::Image);
    }
    if (d.aspectRatio.aspectRatio > 0) {
        aspectRatioElementConfigs_.push_back(d.aspectRatio);
        ConfigUnion u{}; u.aspectRatioElementConfig = &aspectRatioElementConfigs_.back();
        attachElementConfig(u, ConfigType::Aspect);
        aspectRatioElementIndexes_.push_back((int32_t)layoutElements_.size() - 1);
    }
    if (d.floating.attachTo != FloatingAttachTo::None) {
        FloatingElementConfig fc = d.floating;
        int32_t n = (int32_t)openLayoutElementStack_.size();
        if (n >= 2) {
            LayoutElement* hierParent = getLayoutElement(openLayoutElementStack_[n-2]);
            uint32_t clipId = 0;
            if (d.floating.attachTo == FloatingAttachTo::Parent) {
                fc.parentId = hierParent->id;
                if (!openClipElementStack_.empty())
                    clipId = (uint32_t)openClipElementStack_.back();
            } else if (d.floating.attachTo == FloatingAttachTo::ElementWithId) {
                auto* pi = getHashMapItem(fc.parentId);
                if (pi == &hashMapItemDefault_)
                    throw FloatingParentNotFound("Clay: floating parent not found");
                clipId = (uint32_t)layoutElementClipElementIds_[
                    (int32_t)(pi->layoutElement - layoutElements_.data())];
            } else if (d.floating.attachTo == FloatingAttachTo::Root) {
                fc.parentId = internal::hashString("Clay__RootContainer", 0, 0).id;
            }
            if (!openId.id)
                openId = internal::hashString("Clay__FloatingContainer",
                                               (uint32_t)layoutElementTreeRoots_.size(), 0);
            if (d.floating.clipTo == FloatingClipTo::None) clipId = 0;
            int32_t curIdx = openLayoutElementStack_.back();
            layoutElementClipElementIds_[curIdx] = (int32_t)clipId;
            openClipElementStack_.push_back((int32_t)clipId);
            layoutElementTreeRoots_.push_back(LayoutElementTreeRoot{
                curIdx, fc.parentId, clipId, fc.zIndex, {}
            });
        }
        floatingElementConfigs_.push_back(fc);
        ConfigUnion u{}; u.floatingElementConfig = &floatingElementConfigs_.back();
        attachElementConfig(u, ConfigType::Floating);
    }
    if (d.custom.customData) {
        customElementConfigs_.push_back(d.custom);
        ConfigUnion u{}; u.customElementConfig = &customElementConfigs_.back();
        attachElementConfig(u, ConfigType::Custom);
    }

    if (openId.id != 0) {
        attachId(openId);
    } else if (el->id == 0) {
        generateIdForAnonymousElement(el);
    }

    if (d.clip.horizontal || d.clip.vertical) {
        clipElementConfigs_.push_back(d.clip);
        ConfigUnion u{}; u.clipElementConfig = &clipElementConfigs_.back();
        attachElementConfig(u, ConfigType::Clip);
        openClipElementStack_.push_back((int32_t)el->id);

        ScrollContainerDataInternal* found = nullptr;
        for (auto& sd : scrollContainerDatas_) {
            if (sd.elementId == el->id) {
                sd.layoutElement = el;
                sd.openThisFrame = true;
                found = &sd;
                break;
            }
        }
        if (!found) {
            scrollContainerDatas_.push_back(ScrollContainerDataInternal{
                el, {}, {}, {-1,-1}, {}, {}, {}, {}, 0, el->id, true, false
            });
            found = &scrollContainerDatas_.back();
        }
        if (externalScrollHandling_ && queryScrollFn_) {
            found->scrollPosition = queryScrollFn_(found->elementId, queryScrollUserData_);
        }
    }

    static const BorderWidth zeroBW{};
    if (std::memcmp(&d.border.width, &zeroBW, sizeof(BorderWidth)) != 0) {
        borderElementConfigs_.push_back(d.border);
        ConfigUnion u{}; u.borderElementConfig = &borderElementConfigs_.back();
        attachElementConfig(u, ConfigType::Border);
    }
}

// ── attachId / generateAnonymous ──────────────────────────────────────────────
ElementId Context::attachId(ElementId eid) {
    if (boolWarn_.maxElementsExceeded) return ElementId{};
    LayoutElement* el = getOpenLayoutElement();
    uint32_t alias = el->id;
    el->id = eid.id;
    addHashMapItem(eid, el, alias);
    int32_t elIdx = (int32_t)(el - layoutElements_.data());
    layoutElementIdStrings_[elIdx] = eid.stringId;
    return eid;
}

ElementId Context::generateIdForAnonymousElement(LayoutElement* el) {
    int32_t n = (int32_t)openLayoutElementStack_.size();
    LayoutElement* parent = (n >= 2) ? getLayoutElement(openLayoutElementStack_[n-2])
                                     : getLayoutElement(0);
    ElementId eid = internal::hashNumber(
        parent->childrenOrTextContent.children.length, parent->id);
    el->id = eid.id;
    addHashMapItem(eid, el, 0);
    int32_t elIdx = (int32_t)(el - layoutElements_.data());
    layoutElementIdStrings_[elIdx] = eid.stringId;
    return eid;
}

// ── openTextElement ───────────────────────────────────────────────────────────
void Context::openTextElement(std::string_view text, const TextElementConfig* cfg) {
    if ((int32_t)layoutElements_.size() >= maxElementCount_ - 1 ||
        boolWarn_.maxElementsExceeded) {
        boolWarn_.maxElementsExceeded = true;
        return;
    }
    LayoutElement* parent = getOpenLayoutElement();

    layoutElements_.push_back(LayoutElement{});
    int32_t idx = (int32_t)layoutElements_.size() - 1;
    LayoutElement* textEl = &layoutElements_[idx];

    int32_t clipId = openClipElementStack_.empty() ? 0 : openClipElementStack_.back();
    if (idx < (int32_t)layoutElementClipElementIds_.size())
        layoutElementClipElementIds_[idx] = clipId;

    layoutElementChildrenBuffer_.push_back(idx);

    MeasureTextCacheItem* measured = measureTextCached(text, cfg);

    ElementId eid = internal::hashNumber(parent->childrenOrTextContent.children.length,
                                          parent->id);
    textEl->id = eid.id;
    addHashMapItem(eid, textEl, 0);
    layoutElementIdStrings_[idx] = eid.stringId;

    float h = cfg->lineHeight > 0 ? (float)cfg->lineHeight
                                   : measured->unwrappedDimensions.height;
    textEl->dimensions = { measured->unwrappedDimensions.width, h };
    textEl->minDimensions = { measured->minWidth, h };

    textElementData_.push_back(TextElementData{
        text, measured->unwrappedDimensions, idx, 0, 0
    });
    textEl->childrenOrTextContent.textElementData = &textElementData_.back();

    // store text config in pool
    textElementConfigs_.push_back(*cfg);
    TextElementConfig* cfgPtr = &textElementConfigs_.back();

    elementConfigs_.push_back(ElementConfig{ ConfigType::Text,
        [&]{ ConfigUnion u{}; u.textElementConfig = cfgPtr; return u; }() });
    textEl->elementConfigsStart  = (int32_t)elementConfigs_.size() - 1;
    textEl->elementConfigsLength = 1;

    layoutConfigs_.push_back(layoutConfigDefault_);
    textEl->layoutConfig = &layoutConfigs_.back();

    parent->childrenOrTextContent.children.length++;
}

// ── Text measurement cache ────────────────────────────────────────────────────
uint32_t Context::hashStringContentsWithConfig(std::string_view text,
                                                const TextElementConfig* cfg) const {
    uint32_t hash = 0;
    // always content-hash since we can't know if pointer is stable
    for (unsigned char c : text) {
        hash += c; hash += (hash<<10); hash ^= (hash>>6);
    }
    hash += cfg->fontId;   hash += (hash<<10); hash ^= (hash>>6);
    hash += cfg->fontSize; hash += (hash<<10); hash ^= (hash>>6);
    hash += cfg->letterSpacing; hash += (hash<<10); hash ^= (hash>>6);
    hash += (hash<<3); hash ^= (hash>>11); hash += (hash<<15);
    return hash + 1;
}

MeasuredWord* Context::addMeasuredWord(MeasuredWord word, MeasuredWord* prev) {
    if (!measuredWordsFreeList_.empty()) {
        int32_t ni = measuredWordsFreeList_.back();
        measuredWordsFreeList_.pop_back();
        measuredWords_[ni] = word;
        prev->next = ni;
        return &measuredWords_[ni];
    }
    prev->next = (int32_t)measuredWords_.size();
    measuredWords_.push_back(word);
    return &measuredWords_.back();
}

MeasureTextCacheItem* Context::measureTextCached(std::string_view text,
                                                   const TextElementConfig* cfg) {
    if (!measureTextFn_) {
        if (!boolWarn_.textMeasurementFunctionNotSet) {
            boolWarn_.textMeasurementFunctionNotSet = true;
            throw TextMeasurementNotSet(
                "Clay: no measure-text function set. Call setMeasureTextFunction().");
        }
        return &measureTextHashMapInternal_[0];
    }

    uint32_t id     = hashStringContentsWithConfig(text, cfg);
    uint32_t bucket = id % (uint32_t)measureTextHashMap_.size();
    int32_t prevIdx = 0;
    int32_t curIdx  = measureTextHashMap_[bucket];

    while (curIdx != 0) {
        MeasureTextCacheItem& entry = measureTextHashMapInternal_[curIdx];
        if (entry.id == id) {
            entry.generation = generation_;
            return &entry;
        }
        // evict stale entry
        if (generation_ - entry.generation > 2) {
            int32_t wi = entry.measuredWordsStartIndex;
            while (wi != -1) {
                measuredWordsFreeList_.push_back(wi);
                wi = measuredWords_[wi].next;
            }
            int32_t nextI = entry.nextIndex;
            measureTextHashMapInternal_[curIdx] = MeasureTextCacheItem{};
            measureTextHashMapInternal_[curIdx].measuredWordsStartIndex = -1;
            measureTextHashMapInternalFreeList_.push_back(curIdx);
            if (prevIdx == 0) measureTextHashMap_[bucket] = nextI;
            else              measureTextHashMapInternal_[prevIdx].nextIndex = nextI;
            curIdx = nextI;
        } else {
            prevIdx = curIdx;
            curIdx  = entry.nextIndex;
        }
    }

    int32_t newIdx = 0;
    MeasureTextCacheItem newItem{};
    newItem.measuredWordsStartIndex = -1;
    newItem.id         = id;
    newItem.generation = generation_;

    if (!measureTextHashMapInternalFreeList_.empty()) {
        newIdx = measureTextHashMapInternalFreeList_.back();
        measureTextHashMapInternalFreeList_.pop_back();
        measureTextHashMapInternal_[newIdx] = newItem;
    } else {
        if ((int32_t)measureTextHashMapInternal_.size() >= maxElementCount_ - 1) {
            boolWarn_.maxTextMeasureCacheExceeded = true;
            throw TextCacheCapacityExceeded(
                "Clay: text measure cache full. Increase maxMeasureTextCacheWordCount.");
        }
        measureTextHashMapInternal_.push_back(newItem);
        newIdx = (int32_t)measureTextHashMapInternal_.size() - 1;
    }
    MeasureTextCacheItem* measured = &measureTextHashMapInternal_[newIdx];

    static const char spaceChar = ' ';
    StringSlice spaceSlice{ 1, &spaceChar, &spaceChar };
    float spaceW = measureTextFn_(spaceSlice, cfg, measureTextUserData_).width;

    MeasuredWord tempWord{}; tempWord.next = -1;
    MeasuredWord* prevWord = &tempWord;

    int32_t start = 0, end = 0;
    float lineWidth = 0, measuredWidth = 0, measuredHeight = 0;

    while (end < (int32_t)text.size()) {
        if ((int32_t)measuredWords_.size() >= maxMeasureTextCacheWordCount_ - 1) {
            boolWarn_.maxTextMeasureCacheExceeded = true;
            throw TextCacheCapacityExceeded("Clay: measured words cache full.");
        }
        char cur = text[end];
        if (cur == ' ' || cur == '\n') {
            int32_t len = end - start;
            StringSlice sl{ len, text.data() + start, text.data() };
            Dimensions dim = measureTextFn_(sl, cfg, measureTextUserData_);
            measured->minWidth = clay_max(dim.width, measured->minWidth);
            measuredHeight = clay_max(measuredHeight, dim.height);
            if (cur == ' ') {
                dim.width += spaceW;
                prevWord = addMeasuredWord({ start, len+1, dim.width, -1 }, prevWord);
                lineWidth += dim.width;
            } else {
                if (len > 0)
                    prevWord = addMeasuredWord({ start, len, dim.width, -1 }, prevWord);
                prevWord = addMeasuredWord({ end+1, 0, 0, -1 }, prevWord);
                lineWidth += dim.width;
                measuredWidth = clay_max(lineWidth, measuredWidth);
                measured->containsNewlines = true;
                lineWidth = 0;
            }
            start = end + 1;
        }
        end++;
    }
    if (end - start > 0) {
        StringSlice sl{ end-start, text.data()+start, text.data() };
        Dimensions dim = measureTextFn_(sl, cfg, measureTextUserData_);
        addMeasuredWord({ start, end-start, dim.width, -1 }, prevWord);
        lineWidth += dim.width;
        measuredHeight = clay_max(measuredHeight, dim.height);
        measured->minWidth = clay_max(dim.width, measured->minWidth);
    }
    measuredWidth = clay_max(lineWidth, measuredWidth) - cfg->letterSpacing;

    measured->measuredWordsStartIndex = tempWord.next;
    measured->unwrappedDimensions = { measuredWidth, measuredHeight };

    if (prevIdx != 0) measureTextHashMapInternal_[prevIdx].nextIndex = newIdx;
    else              measureTextHashMap_[bucket] = newIdx;

    return measured;
}

// ── beginLayout / endLayout ───────────────────────────────────────────────────
void Context::beginLayout() {
    initEphemeralMemory();
    generation_++;
    boolWarn_ = BoolWarn{};

    float rw = layoutDimensions_.width;
    if (debugModeEnabled_) rw -= (float)DEBUG_VIEW_WIDTH;

    openElement();
    ElementDeclaration rootDecl{};
    rootDecl.id     = internal::hashString("Clay__RootContainer", 0, 0);
    rootDecl.layout.sizing.width  = sizing::fixed(rw);
    rootDecl.layout.sizing.height = sizing::fixed(layoutDimensions_.height);
    configureOpenElement(rootDecl);

    openLayoutElementStack_.push_back(0);
    layoutElementTreeRoots_.push_back(LayoutElementTreeRoot{ 0, 0, 0, 0, {} });
}

std::vector<RenderCommand>& Context::endLayout() {
    closeElement();
    if (debugModeEnabled_ && !boolWarn_.maxElementsExceeded) {
        warningsEnabled_ = false;
        renderDebugView();
        warningsEnabled_ = true;
    }
    if (!boolWarn_.maxElementsExceeded)
        calculateFinalLayout();
    return renderCommands_;
}

// ── addRenderCommand ──────────────────────────────────────────────────────────
void Context::addRenderCommand(RenderCommand cmd) {
    if ((int32_t)renderCommands_.size() < maxElementCount_ - 1) {
        renderCommands_.push_back(cmd);
    } else if (!boolWarn_.maxRenderCommandsExceeded) {
        boolWarn_.maxRenderCommandsExceeded = true;
        throw ElementsCapacityExceeded("Clay: render commands capacity exceeded.");
    }
}

bool Context::elementIsOffscreen(const BoundingBox& bb) const {
    if (disableCulling_) return false;
    return bb.x  > layoutDimensions_.width  ||
           bb.y  > layoutDimensions_.height ||
           bb.x + bb.width  < 0 ||
           bb.y + bb.height < 0;
}

// ── sizeContainersAlongAxis ───────────────────────────────────────────────────
void Context::sizeContainersAlongAxis(bool xAxis) {
    std::vector<int32_t> bfsBuffer;
    std::vector<int32_t> resizable;
    bfsBuffer.reserve(maxElementCount_);
    resizable.reserve(maxElementCount_);

    for (int32_t rootIndex = 0; rootIndex < (int32_t)layoutElementTreeRoots_.size(); ++rootIndex) {
        bfsBuffer.clear();
        auto* root = &layoutElementTreeRoots_[rootIndex];
        LayoutElement* rootEl = getLayoutElement(root->layoutElementIndex);
        bfsBuffer.push_back(root->layoutElementIndex);

        if (elementHasConfig(rootEl, ConfigType::Floating)) {
            auto* fc = findElementConfigWithType(rootEl, ConfigType::Floating).floatingElementConfig;
            auto* pi = getHashMapItem(fc->parentId);
            if (pi && pi != &hashMapItemDefault_) {
                if (rootEl->layoutConfig->sizing.width.type  == SizingType::Grow)
                    rootEl->dimensions.width  = pi->layoutElement->dimensions.width;
                if (rootEl->layoutConfig->sizing.height.type == SizingType::Grow)
                    rootEl->dimensions.height = pi->layoutElement->dimensions.height;
            }
        }
        rootEl->dimensions.width  = clay_min(clay_max(rootEl->dimensions.width,
            rootEl->layoutConfig->sizing.width.size.minMax.min),
            rootEl->layoutConfig->sizing.width.size.minMax.max);
        rootEl->dimensions.height = clay_min(clay_max(rootEl->dimensions.height,
            rootEl->layoutConfig->sizing.height.size.minMax.min),
            rootEl->layoutConfig->sizing.height.size.minMax.max);

        for (int32_t bi = 0; bi < (int32_t)bfsBuffer.size(); ++bi) {
            int32_t parentIdx = bfsBuffer[bi];
            LayoutElement* parent = getLayoutElement(parentIdx);
            LayoutConfig*  psc    = parent->layoutConfig;
            float parentSize = xAxis ? parent->dimensions.width : parent->dimensions.height;
            float parentPad  = (float)(xAxis
                ? psc->padding.left + psc->padding.right
                : psc->padding.top  + psc->padding.bottom);
            float innerContent = 0, totalPadGap = parentPad;
            bool sizingAlongAxis = (xAxis && psc->layoutDirection == LayoutDirection::LeftToRight) ||
                                   (!xAxis && psc->layoutDirection == LayoutDirection::TopToBottom);
            resizable.clear();
            int32_t growCount = 0;
            float childGap = (float)psc->childGap;

            for (int32_t ci = 0; ci < parent->childrenOrTextContent.children.length; ++ci) {
                int32_t childIdx = parent->childrenOrTextContent.children.elements[ci];
                LayoutElement* child = getLayoutElement(childIdx);
                SizingAxis cs = xAxis ? child->layoutConfig->sizing.width
                                      : child->layoutConfig->sizing.height;
                float csize = xAxis ? child->dimensions.width : child->dimensions.height;

                if (!elementHasConfig(child, ConfigType::Text) &&
                    child->childrenOrTextContent.children.length > 0)
                    bfsBuffer.push_back(childIdx);

                bool textWithWordWrap =
                    elementHasConfig(child, ConfigType::Text) &&
                    findElementConfigWithType(child, ConfigType::Text).textElementConfig->wrapMode
                        == TextWrapMode::Words;
                if (cs.type != SizingType::Percent && cs.type != SizingType::Fixed &&
                    (!elementHasConfig(child, ConfigType::Text) || textWithWordWrap))
                    resizable.push_back(childIdx);

                if (sizingAlongAxis) {
                    innerContent += (cs.type == SizingType::Percent ? 0.f : csize);
                    if (cs.type == SizingType::Grow) growCount++;
                    if (ci > 0) { innerContent += childGap; totalPadGap += childGap; }
                } else {
                    innerContent = clay_max(csize, innerContent);
                }
            }

            // expand percent
            for (int32_t ci = 0; ci < parent->childrenOrTextContent.children.length; ++ci) {
                int32_t childIdx = parent->childrenOrTextContent.children.elements[ci];
                LayoutElement* child = getLayoutElement(childIdx);
                SizingAxis cs = xAxis ? child->layoutConfig->sizing.width
                                      : child->layoutConfig->sizing.height;
                float* csize = xAxis ? &child->dimensions.width : &child->dimensions.height;
                if (cs.type == SizingType::Percent) {
                    *csize = (parentSize - totalPadGap) * cs.size.percent;
                    if (sizingAlongAxis) innerContent += *csize;
                    updateAspectRatioBox(child);
                }
            }

            if (sizingAlongAxis) {
                float toDistribute = parentSize - parentPad - innerContent;
                if (toDistribute < 0) {
                    auto* clipCfg = findElementConfigWithType(parent, ConfigType::Clip).clipElementConfig;
                    if (clipCfg && ((xAxis && clipCfg->horizontal) || (!xAxis && clipCfg->vertical)))
                        continue;
                    while (toDistribute < -EPSILON && !resizable.empty()) {
                        float largest = 0, secondLargest = 0, widthToAdd = toDistribute;
                        for (int32_t ri : resizable) {
                            LayoutElement* c = getLayoutElement(ri);
                            float cs = xAxis ? c->dimensions.width : c->dimensions.height;
                            if (floatEqual(cs, largest)) continue;
                            if (cs > largest) { secondLargest = largest; largest = cs; }
                            if (cs < largest) { secondLargest = clay_max(secondLargest, cs);
                                                widthToAdd = secondLargest - largest; }
                        }
                        widthToAdd = clay_max(widthToAdd, toDistribute / (float)resizable.size());
                        for (int32_t ri = 0; ri < (int32_t)resizable.size(); ) {
                            LayoutElement* c = getLayoutElement(resizable[ri]);
                            float* cs   = xAxis ? &c->dimensions.width : &c->dimensions.height;
                            float minSz = xAxis ? c->minDimensions.width : c->minDimensions.height;
                            float prev  = *cs;
                            if (floatEqual(*cs, largest)) {
                                *cs += widthToAdd;
                                if (*cs <= minSz) { *cs = minSz; resizable.erase(resizable.begin()+ri); continue; }
                                toDistribute -= (*cs - prev);
                            }
                            ++ri;
                        }
                    }
                } else if (toDistribute > 0 && growCount > 0) {
                    resizable.erase(std::remove_if(resizable.begin(), resizable.end(),
                        [&](int32_t ri){
                            LayoutElement* c = getLayoutElement(ri);
                            SizingType st = xAxis ? c->layoutConfig->sizing.width.type
                                                  : c->layoutConfig->sizing.height.type;
                            return st != SizingType::Grow;
                        }), resizable.end());
                    while (toDistribute > EPSILON && !resizable.empty()) {
                        float smallest = FLT_MAX, secondSmallest = FLT_MAX, widthToAdd = toDistribute;
                        for (int32_t ri : resizable) {
                            LayoutElement* c = getLayoutElement(ri);
                            float cs = xAxis ? c->dimensions.width : c->dimensions.height;
                            if (floatEqual(cs, smallest)) continue;
                            if (cs < smallest) { secondSmallest = smallest; smallest = cs; }
                            if (cs > smallest) { secondSmallest = clay_min(secondSmallest, cs);
                                                 widthToAdd = secondSmallest - smallest; }
                        }
                        widthToAdd = clay_min(widthToAdd, toDistribute / (float)resizable.size());
                        for (int32_t ri = 0; ri < (int32_t)resizable.size(); ) {
                            LayoutElement* c = getLayoutElement(resizable[ri]);
                            float* cs   = xAxis ? &c->dimensions.width : &c->dimensions.height;
                            float maxSz = xAxis ? c->layoutConfig->sizing.width.size.minMax.max
                                                : c->layoutConfig->sizing.height.size.minMax.max;
                            float prev  = *cs;
                            if (floatEqual(*cs, smallest)) {
                                *cs += widthToAdd;
                                if (*cs >= maxSz) { *cs = maxSz; resizable.erase(resizable.begin()+ri); continue; }
                                toDistribute -= (*cs - prev);
                            }
                            ++ri;
                        }
                    }
                }
            } else {
                for (int32_t ri : resizable) {
                    LayoutElement* c = getLayoutElement(ri);
                    SizingAxis cs = xAxis ? c->layoutConfig->sizing.width
                                          : c->layoutConfig->sizing.height;
                    float minSz = xAxis ? c->minDimensions.width : c->minDimensions.height;
                    float* csize = xAxis ? &c->dimensions.width : &c->dimensions.height;
                    float maxSz  = parentSize - parentPad;
                    if (elementHasConfig(parent, ConfigType::Clip)) {
                        auto* cc = findElementConfigWithType(parent, ConfigType::Clip).clipElementConfig;
                        if ((xAxis && cc->horizontal) || (!xAxis && cc->vertical))
                            maxSz = clay_max(maxSz, innerContent);
                    }
                    if (cs.type == SizingType::Grow)
                        *csize = clay_min(maxSz, cs.size.minMax.max);
                    *csize = clay_max(minSz, clay_min(*csize, maxSz));
                }
            }
        }
    }
}

// ── intToString ───────────────────────────────────────────────────────────────
std::string_view Context::intToString(int32_t v) {
    if (v == 0) { dynamicStringData_.push_back('0'); return { &dynamicStringData_.back(), 1 }; }
    size_t start = dynamicStringData_.size();
    bool neg = v < 0;
    if (neg) v = -v;
    while (v > 0) {
        dynamicStringData_.push_back((char)('0' + v % 10));
        v /= 10;
    }
    if (neg) dynamicStringData_.push_back('-');
    size_t end = dynamicStringData_.size();
    std::reverse(dynamicStringData_.begin() + (ptrdiff_t)start, dynamicStringData_.end());
    return { dynamicStringData_.data() + start, end - start };
}

// ── calculateFinalLayout ──────────────────────────────────────────────────────
void Context::calculateFinalLayout() {
    sizeContainersAlongAxis(true);

    // wrap text
    for (auto& ted : textElementData_) {
        ted.wrappedLinesStart  = (int32_t)wrappedTextLines_.size();
        ted.wrappedLinesLength = 0;

        LayoutElement* el = getLayoutElement(ted.elementIndex);
        auto* tcfg = findElementConfigWithType(el, ConfigType::Text).textElementConfig;
        MeasureTextCacheItem* mci = measureTextCached(ted.text, tcfg);

        float lineHeight = tcfg->lineHeight > 0 ? (float)tcfg->lineHeight
                                                 : ted.preferredDimensions.height;

        if (!mci->containsNewlines && ted.preferredDimensions.width <= el->dimensions.width) {
            wrappedTextLines_.push_back({ el->dimensions, ted.text });
            ted.wrappedLinesLength++;
            continue;
        }
        static const char sp = ' ';
        StringSlice spSl{ 1, &sp, &sp };
        float spaceW = measureTextFn_(spSl, tcfg, measureTextUserData_).width;

        float lineWidth = 0;
        int32_t lineLenChars = 0, lineStartOff = 0;
        int32_t wi = mci->measuredWordsStartIndex;
        while (wi != -1) {
            MeasuredWord* mw = &measuredWords_[wi];
            if (lineLenChars == 0 && lineWidth + mw->width > el->dimensions.width) {
                wrappedTextLines_.push_back({
                    { mw->width, lineHeight },
                    std::string_view(ted.text.data() + mw->startOffset, (size_t)mw->length)
                });
                ted.wrappedLinesLength++;
                wi = mw->next;
                lineStartOff = mw->startOffset + mw->length;
            } else if (mw->length == 0 || lineWidth + mw->width > el->dimensions.width) {
                bool fs = (lineLenChars > 0 && ted.text[clay_max(lineStartOff + lineLenChars - 1, 0)] == ' ');
                wrappedTextLines_.push_back({
                    { lineWidth + (fs ? -spaceW : 0), lineHeight },
                    std::string_view(ted.text.data() + lineStartOff,
                                     (size_t)(lineLenChars + (fs ? -1 : 0)))
                });
                ted.wrappedLinesLength++;
                if (lineLenChars == 0 || mw->length == 0) wi = mw->next;
                lineWidth = 0; lineLenChars = 0; lineStartOff = mw->startOffset;
            } else {
                lineWidth   += mw->width + tcfg->letterSpacing;
                lineLenChars += mw->length;
                wi = mw->next;
            }
        }
        if (lineLenChars > 0) {
            wrappedTextLines_.push_back({
                { lineWidth - tcfg->letterSpacing, lineHeight },
                std::string_view(ted.text.data() + lineStartOff, (size_t)lineLenChars)
            });
            ted.wrappedLinesLength++;
        }
        el->dimensions.height = lineHeight * (float)ted.wrappedLinesLength;
    }

    // aspect ratio vertical
    for (int32_t i : aspectRatioElementIndexes_) {
        LayoutElement* ae = getLayoutElement(i);
        auto* ac = findElementConfigWithType(ae, ConfigType::Aspect).aspectRatioElementConfig;
        ae->dimensions.height = (1.f / ac->aspectRatio) * ae->dimensions.width;
        ae->layoutConfig->sizing.height.size.minMax.max = ae->dimensions.height;
    }

    // propagate heights via DFS
    std::vector<LayoutElementTreeNode> dfs;
    dfs.reserve(maxElementCount_);
    for (auto& root : layoutElementTreeRoots_) {
        treeNodeVisited_[dfs.size()] = false;
        dfs.push_back({ getLayoutElement(root.layoutElementIndex), {}, {} });
    }
    while (!dfs.empty()) {
        LayoutElementTreeNode& node = dfs.back();
        LayoutElement* el = node.layoutElement;
        if (!treeNodeVisited_[dfs.size()-1]) {
            treeNodeVisited_[dfs.size()-1] = true;
            if (elementHasConfig(el, ConfigType::Text) || el->childrenOrTextContent.children.length == 0) {
                dfs.pop_back(); continue;
            }
            for (int32_t i = 0; i < el->childrenOrTextContent.children.length; ++i) {
                treeNodeVisited_[dfs.size()] = false;
                dfs.push_back({ getLayoutElement(el->childrenOrTextContent.children.elements[i]), {}, {} });
            }
            continue;
        }
        dfs.pop_back();
        LayoutConfig* lc = el->layoutConfig;
        if (lc->layoutDirection == LayoutDirection::LeftToRight) {
            for (int32_t j = 0; j < el->childrenOrTextContent.children.length; ++j) {
                LayoutElement* ch = getLayoutElement(el->childrenOrTextContent.children.elements[j]);
                float chH = clay_max(ch->dimensions.height + lc->padding.top + lc->padding.bottom,
                                     el->dimensions.height);
                el->dimensions.height = clay_min(clay_max(chH, lc->sizing.height.size.minMax.min),
                                                          lc->sizing.height.size.minMax.max);
            }
        } else {
            float ch = (float)(lc->padding.top + lc->padding.bottom);
            for (int32_t j = 0; j < el->childrenOrTextContent.children.length; ++j)
                ch += getLayoutElement(el->childrenOrTextContent.children.elements[j])->dimensions.height;
            ch += (float)(clay_max(el->childrenOrTextContent.children.length-1,0) * lc->childGap);
            el->dimensions.height = clay_min(clay_max(ch, lc->sizing.height.size.minMax.min),
                                                       lc->sizing.height.size.minMax.max);
        }
    }

    sizeContainersAlongAxis(false);

    // aspect ratio horizontal
    for (int32_t i : aspectRatioElementIndexes_) {
        LayoutElement* ae = getLayoutElement(i);
        auto* ac = findElementConfigWithType(ae, ConfigType::Aspect).aspectRatioElementConfig;
        ae->dimensions.width = ac->aspectRatio * ae->dimensions.height;
    }

    // sort roots by z-index
    std::stable_sort(layoutElementTreeRoots_.begin(), layoutElementTreeRoots_.end(),
        [](const LayoutElementTreeRoot& a, const LayoutElementTreeRoot& b){
            return a.zIndex < b.zIndex;
        });

    // generate render commands
    renderCommands_.clear();
    for (int32_t rootIndex = 0; rootIndex < (int32_t)layoutElementTreeRoots_.size(); ++rootIndex) {
        dfs.clear();
        auto* root = &layoutElementTreeRoots_[rootIndex];
        LayoutElement* rootEl = getLayoutElement(root->layoutElementIndex);
        Vector2 rootPos{};

        // position floating roots
        if (elementHasConfig(rootEl, ConfigType::Floating)) {
            auto* fc = findElementConfigWithType(rootEl, ConfigType::Floating).floatingElementConfig;
            auto* pi = getHashMapItem(fc->parentId);
            if (pi && pi != &hashMapItemDefault_) {
                BoundingBox pb = pi->boundingBox;
                Dimensions  rd = rootEl->dimensions;
                Vector2 tap{};
                switch (fc->attachPoints.parent) {
                case FloatingAttachPoint::LeftTop: case FloatingAttachPoint::LeftCenter: case FloatingAttachPoint::LeftBottom:
                    tap.x = pb.x; break;
                case FloatingAttachPoint::CenterTop: case FloatingAttachPoint::CenterCenter: case FloatingAttachPoint::CenterBottom:
                    tap.x = pb.x + pb.width/2; break;
                default: tap.x = pb.x + pb.width; break;
                }
                switch (fc->attachPoints.element) {
                case FloatingAttachPoint::LeftTop: case FloatingAttachPoint::LeftCenter: case FloatingAttachPoint::LeftBottom:
                    break;
                case FloatingAttachPoint::CenterTop: case FloatingAttachPoint::CenterCenter: case FloatingAttachPoint::CenterBottom:
                    tap.x -= rd.width/2; break;
                default: tap.x -= rd.width; break;
                }
                switch (fc->attachPoints.parent) {
                case FloatingAttachPoint::LeftTop: case FloatingAttachPoint::RightTop: case FloatingAttachPoint::CenterTop:
                    tap.y = pb.y; break;
                case FloatingAttachPoint::LeftCenter: case FloatingAttachPoint::CenterCenter: case FloatingAttachPoint::RightCenter:
                    tap.y = pb.y + pb.height/2; break;
                default: tap.y = pb.y + pb.height; break;
                }
                switch (fc->attachPoints.element) {
                case FloatingAttachPoint::LeftTop: case FloatingAttachPoint::RightTop: case FloatingAttachPoint::CenterTop:
                    break;
                case FloatingAttachPoint::LeftCenter: case FloatingAttachPoint::CenterCenter: case FloatingAttachPoint::RightCenter:
                    tap.y -= rd.height/2; break;
                default: tap.y -= rd.height; break;
                }
                tap.x += fc->offset.x; tap.y += fc->offset.y;
                rootPos = tap;
            }
        }
        if (root->clipElementId) {
            auto* ci = getHashMapItem(root->clipElementId);
            if (ci && ci != &hashMapItemDefault_) {
                if (externalScrollHandling_) {
                    auto* cc = findElementConfigWithType(ci->layoutElement, ConfigType::Clip).clipElementConfig;
                    if (cc && cc->horizontal) rootPos.x += cc->childOffset.x;
                    if (cc && cc->vertical)   rootPos.y += cc->childOffset.y;
                }
                RenderCommand sc{};
                sc.boundingBox = ci->boundingBox;
                sc.id          = internal::hashNumber(rootEl->id,
                    rootEl->childrenOrTextContent.children.length + 10).id;
                sc.zIndex      = root->zIndex;
                sc.commandType = RenderCommandType::ScissorStart;
                addRenderCommand(sc);
            }
        }

        LayoutElementTreeNode rootNode{};
        rootNode.layoutElement  = rootEl;
        rootNode.position       = rootPos;
        rootNode.nextChildOffset = { (float)rootEl->layoutConfig->padding.left,
                                     (float)rootEl->layoutConfig->padding.top };
        dfs.push_back(rootNode);
        treeNodeVisited_[0] = false;

        while (!dfs.empty()) {
            LayoutElementTreeNode& cur = dfs.back();
            LayoutElement* el   = cur.layoutElement;
            LayoutConfig*  lc   = el->layoutConfig;
            Vector2 scrollOffset{};

            if (!treeNodeVisited_[dfs.size()-1]) {
                treeNodeVisited_[dfs.size()-1] = true;

                BoundingBox ebb{ cur.position.x, cur.position.y,
                                 el->dimensions.width, el->dimensions.height };
                if (elementHasConfig(el, ConfigType::Floating)) {
                    auto* fc = findElementConfigWithType(el, ConfigType::Floating).floatingElementConfig;
                    ebb.x -= fc->expand.width; ebb.width  += fc->expand.width  * 2;
                    ebb.y -= fc->expand.height; ebb.height += fc->expand.height * 2;
                }

                ScrollContainerDataInternal* scd = nullptr;
                if (elementHasConfig(el, ConfigType::Clip)) {
                    auto* cc = findElementConfigWithType(el, ConfigType::Clip).clipElementConfig;
                    for (auto& sd : scrollContainerDatas_) {
                        if (sd.layoutElement == el) {
                            scd = &sd;
                            sd.boundingBox = ebb;
                            scrollOffset = cc->childOffset;
                            if (externalScrollHandling_) scrollOffset = {};
                            break;
                        }
                    }
                }

                auto* hmi = getHashMapItem(el->id);
                if (hmi && hmi != &hashMapItemDefault_) {
                    hmi->boundingBox = ebb;
                    if (hmi->idAlias) {
                        auto* alias = getHashMapItem(hmi->idAlias);
                        if (alias && alias != &hashMapItemDefault_)
                            alias->boundingBox = ebb;
                    }
                }

                // sort configs: clip last, border before clip
                int32_t sorted[20]; int32_t nc = el->elementConfigsLength;
                for (int32_t i=0;i<nc;++i) sorted[i]=i;
                for (int32_t pass=nc-1;pass>0;--pass)
                    for (int32_t i=0;i<pass;++i) {
                        auto ta = elementConfigs_[el->elementConfigsStart+sorted[i]].type;
                        auto tb = elementConfigs_[el->elementConfigsStart+sorted[i+1]].type;
                        if (tb == ConfigType::Clip || ta == ConfigType::Border)
                            std::swap(sorted[i], sorted[i+1]);
                    }

                bool emitRect = false;
                SharedElementConfig* sc = nullptr;
                {
                    auto cu = findElementConfigWithType(el, ConfigType::Shared);
                    sc = cu.sharedElementConfig;
                }
                if (sc && sc->backgroundColor.a > 0) emitRect = true;
                if (!sc) sc = &sharedConfigDefault_;

                for (int32_t ei=0; ei<nc; ++ei) {
                    auto& ecfg = elementConfigs_[el->elementConfigsStart+sorted[ei]];
                    RenderCommand rc{};
                    rc.boundingBox = ebb;
                    rc.userData    = sc->userData;
                    rc.id          = el->id;
                    bool offscreen  = elementIsOffscreen(ebb);
                    bool shouldRender = !offscreen;

                    switch (ecfg.type) {
                    case ConfigType::Aspect:
                    case ConfigType::Floating:
                    case ConfigType::Shared:
                    case ConfigType::Border:
                        shouldRender = false; break;
                    case ConfigType::Clip:
                        rc.commandType = RenderCommandType::ScissorStart;
                        rc.renderData.clip = { ecfg.config.clipElementConfig->horizontal,
                                               ecfg.config.clipElementConfig->vertical };
                        break;
                    case ConfigType::Image:
                        rc.commandType = RenderCommandType::Image;
                        rc.renderData.image = { sc->backgroundColor, sc->cornerRadius,
                                                ecfg.config.imageElementConfig->imageData };
                        emitRect = false; break;
                    case ConfigType::Text: {
                        if (!shouldRender) break;
                        shouldRender = false;
                        auto* tc = ecfg.config.textElementConfig;
                        TextElementData* ted = el->childrenOrTextContent.textElementData;
                        float natH = ted->preferredDimensions.height;
                        float finH = tc->lineHeight > 0 ? (float)tc->lineHeight : natH;
                        float lhOff = (finH - natH) / 2;
                        float ypos  = lhOff;
                        for (int32_t li = 0; li < ted->wrappedLinesLength; ++li) {
                            auto& wl = wrappedTextLines_[ted->wrappedLinesStart + li];
                            if (wl.line.empty()) { ypos += finH; continue; }
                            float off = ebb.width - wl.dimensions.width;
                            if (tc->textAlignment == TextAlignment::Left)   off = 0;
                            if (tc->textAlignment == TextAlignment::Center) off /= 2;
                            RenderCommand tc2{};
                            tc2.boundingBox = { ebb.x+off, ebb.y+ypos,
                                                wl.dimensions.width, wl.dimensions.height };
                            tc2.renderData.text = {
                                { (int32_t)wl.line.size(), wl.line.data(), ted->text.data() },
                                tc->textColor, tc->fontId, tc->fontSize,
                                tc->letterSpacing, tc->lineHeight
                            };
                            tc2.userData    = tc->userData;
                            tc2.id          = internal::hashNumber((uint32_t)li, el->id).id;
                            tc2.zIndex      = root->zIndex;
                            tc2.commandType = RenderCommandType::Text;
                            addRenderCommand(tc2);
                            ypos += finH;
                            if (!disableCulling_ && ebb.y + ypos > layoutDimensions_.height) break;
                        }
                        break;
                    }
                    case ConfigType::Custom:
                        rc.commandType = RenderCommandType::Custom;
                        rc.renderData.custom = { sc->backgroundColor, sc->cornerRadius,
                                                 ecfg.config.customElementConfig->customData };
                        emitRect = false; break;
                    default: break;
                    }
                    if (shouldRender) addRenderCommand(rc);
                }

                if (emitRect) {
                    RenderCommand rc{};
                    rc.boundingBox = ebb;
                    rc.renderData.rectangle = { sc->backgroundColor, sc->cornerRadius };
                    rc.userData    = sc->userData;
                    rc.id          = el->id;
                    rc.zIndex      = root->zIndex;
                    rc.commandType = RenderCommandType::Rectangle;
                    addRenderCommand(rc);
                }

                // axis alignment
                if (!elementHasConfig(el, ConfigType::Text)) {
                    Dimensions cs{};
                    if (lc->layoutDirection == LayoutDirection::LeftToRight) {
                        for (int32_t i=0;i<el->childrenOrTextContent.children.length;++i) {
                            LayoutElement* ch = getLayoutElement(el->childrenOrTextContent.children.elements[i]);
                            cs.width += ch->dimensions.width;
                            cs.height = clay_max(cs.height, ch->dimensions.height);
                        }
                        cs.width += (float)(clay_max(el->childrenOrTextContent.children.length-1,0)
                                            * lc->childGap);
                        float ex = el->dimensions.width - (float)(lc->padding.left+lc->padding.right) - cs.width;
                        if (lc->childAlignment.x == AlignX::Left)   ex = 0;
                        if (lc->childAlignment.x == AlignX::Center) ex /= 2;
                        cur.nextChildOffset.x += ex;
                    } else {
                        for (int32_t i=0;i<el->childrenOrTextContent.children.length;++i) {
                            LayoutElement* ch = getLayoutElement(el->childrenOrTextContent.children.elements[i]);
                            cs.width  = clay_max(cs.width, ch->dimensions.width);
                            cs.height += ch->dimensions.height;
                        }
                        cs.height += (float)(clay_max(el->childrenOrTextContent.children.length-1,0)
                                             * lc->childGap);
                        float ex = el->dimensions.height - (float)(lc->padding.top+lc->padding.bottom) - cs.height;
                        if (lc->childAlignment.y == AlignY::Top)    ex = 0;
                        if (lc->childAlignment.y == AlignY::Center) ex /= 2;
                        cur.nextChildOffset.y += ex;
                    }
                    if (scd) scd->contentSize = {
                        cs.width  + (float)(lc->padding.left + lc->padding.right),
                        cs.height + (float)(lc->padding.top  + lc->padding.bottom)
                    };
                }
            } else {
                // upward pass — emit borders and close scissors
                bool closeClip = false;
                auto* cc = findElementConfigWithType(el, ConfigType::Clip).clipElementConfig;
                if (cc) {
                    closeClip = true;
                    for (auto& sd : scrollContainerDatas_) {
                        if (sd.layoutElement == el) {
                            scrollOffset = cc->childOffset;
                            if (externalScrollHandling_) scrollOffset = {};
                            break;
                        }
                    }
                }
                if (elementHasConfig(el, ConfigType::Border)) {
                    auto* hmi = getHashMapItem(el->id);
                    BoundingBox ebb = hmi->boundingBox;
                    if (!elementIsOffscreen(ebb)) {
                        SharedElementConfig* sc = nullptr;
                        { auto cu = findElementConfigWithType(el, ConfigType::Shared);
                          sc = cu.sharedElementConfig ? cu.sharedElementConfig : &sharedConfigDefault_; }
                        auto* bc = findElementConfigWithType(el, ConfigType::Border).borderElementConfig;
                        RenderCommand rc{};
                        rc.boundingBox = ebb;
                        rc.renderData.border = { bc->color, sc->cornerRadius, bc->width };
                        rc.userData    = sc->userData;
                        rc.id          = internal::hashNumber(el->id,
                            el->childrenOrTextContent.children.length).id;
                        rc.commandType = RenderCommandType::Border;
                        addRenderCommand(rc);

                        if (bc->width.betweenChildren > 0 && bc->color.a > 0) {
                            float halfGap = lc->childGap / 2.f;
                            Vector2 boff{ (float)lc->padding.left - halfGap,
                                          (float)lc->padding.top  - halfGap };
                            if (lc->layoutDirection == LayoutDirection::LeftToRight) {
                                for (int32_t i=0;i<el->childrenOrTextContent.children.length;++i) {
                                    LayoutElement* ch = getLayoutElement(el->childrenOrTextContent.children.elements[i]);
                                    if (i > 0) {
                                        RenderCommand br{};
                                        br.boundingBox = { ebb.x+boff.x+scrollOffset.x, ebb.y+scrollOffset.y,
                                                           (float)bc->width.betweenChildren, el->dimensions.height };
                                        br.renderData.rectangle.backgroundColor = bc->color;
                                        br.id = internal::hashNumber(el->id,
                                            el->childrenOrTextContent.children.length+1+i).id;
                                        br.commandType = RenderCommandType::Rectangle;
                                        addRenderCommand(br);
                                    }
                                    boff.x += ch->dimensions.width + (float)lc->childGap;
                                }
                            } else {
                                for (int32_t i=0;i<el->childrenOrTextContent.children.length;++i) {
                                    LayoutElement* ch = getLayoutElement(el->childrenOrTextContent.children.elements[i]);
                                    if (i > 0) {
                                        RenderCommand br{};
                                        br.boundingBox = { ebb.x+scrollOffset.x, ebb.y+boff.y+scrollOffset.y,
                                                           el->dimensions.width, (float)bc->width.betweenChildren };
                                        br.renderData.rectangle.backgroundColor = bc->color;
                                        br.id = internal::hashNumber(el->id,
                                            el->childrenOrTextContent.children.length+1+i).id;
                                        br.commandType = RenderCommandType::Rectangle;
                                        addRenderCommand(br);
                                    }
                                    boff.y += ch->dimensions.height + (float)lc->childGap;
                                }
                            }
                        }
                    }
                }
                if (closeClip) {
                    RenderCommand rc{};
                    rc.id = internal::hashNumber(el->id,
                        rootEl->childrenOrTextContent.children.length + 11).id;
                    rc.commandType = RenderCommandType::ScissorEnd;
                    addRenderCommand(rc);
                }
                dfs.pop_back(); continue;
            }

            // push children
            if (!elementHasConfig(el, ConfigType::Text)) {
                int32_t nch = el->childrenOrTextContent.children.length;
                size_t base = dfs.size();
                dfs.resize(base + nch);
                for (int32_t i=0; i<nch; ++i) {
                    LayoutElement* ch = getLayoutElement(el->childrenOrTextContent.children.elements[i]);
                    uint32_t ni = (uint32_t)(base + nch - 1 - i);
                    LayoutElementTreeNode& nn = dfs[ni];
                    if (lc->layoutDirection == LayoutDirection::LeftToRight) {
                        cur.nextChildOffset.y = (float)lc->padding.top;
                        float ws = el->dimensions.height - (float)(lc->padding.top+lc->padding.bottom) - ch->dimensions.height;
                        if (lc->childAlignment.y == AlignY::Center)  cur.nextChildOffset.y += ws/2;
                        if (lc->childAlignment.y == AlignY::Bottom)  cur.nextChildOffset.y += ws;
                    } else {
                        cur.nextChildOffset.x = (float)lc->padding.left;
                        float ws = el->dimensions.width - (float)(lc->padding.left+lc->padding.right) - ch->dimensions.width;
                        if (lc->childAlignment.x == AlignX::Center) cur.nextChildOffset.x += ws/2;
                        if (lc->childAlignment.x == AlignX::Right)  cur.nextChildOffset.x += ws;
                    }
                    nn.layoutElement = ch;
                    nn.position = {
                        cur.position.x + cur.nextChildOffset.x + scrollOffset.x,
                        cur.position.y + cur.nextChildOffset.y + scrollOffset.y
                    };
                    nn.nextChildOffset = { (float)ch->layoutConfig->padding.left,
                                           (float)ch->layoutConfig->padding.top };
                    if (ni < treeNodeVisited_.size()) treeNodeVisited_[ni] = false;

                    if (lc->layoutDirection == LayoutDirection::LeftToRight)
                        cur.nextChildOffset.x += ch->dimensions.width + (float)lc->childGap;
                    else
                        cur.nextChildOffset.y += ch->dimensions.height + (float)lc->childGap;
                }
            }
        }

        if (root->clipElementId) {
            RenderCommand rc{};
            rc.id = internal::hashNumber(rootEl->id,
                rootEl->childrenOrTextContent.children.length + 11).id;
            rc.commandType = RenderCommandType::ScissorEnd;
            addRenderCommand(rc);
        }
    }
}

// ── setPointerState ───────────────────────────────────────────────────────────
void Context::setPointerState(Vector2 position, bool pointerDown) {
    if (boolWarn_.maxElementsExceeded) return;
    pointerInfo_.position = position;
    pointerOverIds_.clear();

    std::vector<int32_t> dfs;
    dfs.reserve(maxElementCount_);

    for (int32_t ri = (int32_t)layoutElementTreeRoots_.size()-1; ri >= 0; --ri) {
        dfs.clear();
        auto* root = &layoutElementTreeRoots_[ri];
        dfs.push_back(root->layoutElementIndex);
        treeNodeVisited_[0] = false;
        bool found = false;
        while (!dfs.empty()) {
            if (treeNodeVisited_[dfs.size()-1]) { dfs.pop_back(); continue; }
            treeNodeVisited_[dfs.size()-1] = true;
            LayoutElement* el = getLayoutElement(dfs.back());
            auto* hmi = getHashMapItem(el->id);
            int32_t clipId = layoutElementClipElementIds_[(int32_t)(el - layoutElements_.data())];
            auto* clipItem = getHashMapItem((uint32_t)clipId);
            if (hmi && hmi != &hashMapItemDefault_) {
                BoundingBox ebb = hmi->boundingBox;
                ebb.x -= root->pointerOffset.x; ebb.y -= root->pointerOffset.y;
                bool inBox   = position.x >= ebb.x && position.x <= ebb.x+ebb.width &&
                               position.y >= ebb.y && position.y <= ebb.y+ebb.height;
                bool clipOk  = (clipId == 0 || (clipItem && clipItem != &hashMapItemDefault_ &&
                    position.x >= clipItem->boundingBox.x &&
                    position.x <= clipItem->boundingBox.x + clipItem->boundingBox.width &&
                    position.y >= clipItem->boundingBox.y &&
                    position.y <= clipItem->boundingBox.y + clipItem->boundingBox.height));
                if (inBox && clipOk) {
                    if (hmi->onHoverFn) hmi->onHoverFn(hmi->elementId, pointerInfo_);
                    pointerOverIds_.push_back(hmi->elementId);
                    found = true;
                    if (hmi->idAlias) pointerOverIds_.push_back(ElementId{ hmi->idAlias });
                }
                if (elementHasConfig(el, ConfigType::Text)) { dfs.pop_back(); continue; }
                for (int32_t i = el->childrenOrTextContent.children.length-1; i >= 0; --i) {
                    int32_t ci = el->childrenOrTextContent.children.elements[i];
                    dfs.push_back(ci);
                    if (dfs.size()-1 < treeNodeVisited_.size()) treeNodeVisited_[dfs.size()-1] = false;
                }
            } else { dfs.pop_back(); }
        }
        LayoutElement* rootEl = getLayoutElement(root->layoutElementIndex);
        if (found && elementHasConfig(rootEl, ConfigType::Floating)) {
            auto* fc = findElementConfigWithType(rootEl, ConfigType::Floating).floatingElementConfig;
            if (fc->pointerCaptureMode == PointerCaptureMode::Capture) break;
        }
    }

    if (pointerDown) {
        if (pointerInfo_.state == PointerState::PressedThisFrame)
            pointerInfo_.state = PointerState::Pressed;
        else if (pointerInfo_.state != PointerState::Pressed)
            pointerInfo_.state = PointerState::PressedThisFrame;
    } else {
        if (pointerInfo_.state == PointerState::ReleasedThisFrame)
            pointerInfo_.state = PointerState::Released;
        else if (pointerInfo_.state != PointerState::Released)
            pointerInfo_.state = PointerState::ReleasedThisFrame;
    }
}

// ── updateScrollContainers ────────────────────────────────────────────────────
void Context::updateScrollContainers(bool enableDrag, Vector2 scrollDelta, float deltaTime) {
    bool isPointerActive = enableDrag && (pointerInfo_.state == PointerState::Pressed ||
                                          pointerInfo_.state == PointerState::PressedThisFrame);
    int32_t highestPriIdx = -1;
    ScrollContainerDataInternal* highestPriData = nullptr;

    for (int32_t i = 0; i < (int32_t)scrollContainerDatas_.size(); ) {
        auto& sd = scrollContainerDatas_[i];
        if (!sd.openThisFrame) {
            scrollContainerDatas_.erase(scrollContainerDatas_.begin() + i);
            continue;
        }
        sd.openThisFrame = false;
        auto* hmi = getHashMapItem(sd.elementId);
        if (!hmi || hmi == &hashMapItemDefault_) {
            scrollContainerDatas_.erase(scrollContainerDatas_.begin() + i);
            continue;
        }

        if (!isPointerActive && sd.pointerScrollActive) {
            float xd = sd.scrollPosition.x - sd.scrollOrigin.x;
            if (xd < -10 || xd > 10) sd.scrollMomentum.x = xd / (sd.momentumTime * 25);
            float yd = sd.scrollPosition.y - sd.scrollOrigin.y;
            if (yd < -10 || yd > 10) sd.scrollMomentum.y = yd / (sd.momentumTime * 25);
            sd.pointerScrollActive = false;
            sd.pointerOrigin = {}; sd.scrollOrigin = {}; sd.momentumTime = 0;
        }

        bool scrollOccurred = scrollDelta.x != 0 || scrollDelta.y != 0;
        sd.scrollPosition.x += sd.scrollMomentum.x;
        sd.scrollMomentum.x *= 0.95f;
        if ((sd.scrollMomentum.x > -0.1f && sd.scrollMomentum.x < 0.1f) || scrollOccurred)
            sd.scrollMomentum.x = 0;
        float maxX = clay_max(sd.contentSize.width  - sd.layoutElement->dimensions.width,  0.f);
        sd.scrollPosition.x = clay_min(clay_max(sd.scrollPosition.x, -maxX), 0.f);

        sd.scrollPosition.y += sd.scrollMomentum.y;
        sd.scrollMomentum.y *= 0.95f;
        if ((sd.scrollMomentum.y > -0.1f && sd.scrollMomentum.y < 0.1f) || scrollOccurred)
            sd.scrollMomentum.y = 0;
        float maxY = clay_max(sd.contentSize.height - sd.layoutElement->dimensions.height, 0.f);
        sd.scrollPosition.y = clay_min(clay_max(sd.scrollPosition.y, -maxY), 0.f);

        for (int32_t j = 0; j < (int32_t)pointerOverIds_.size(); ++j) {
            if (sd.layoutElement->id == pointerOverIds_[j].id) {
                highestPriIdx  = j;
                highestPriData = &sd;
            }
        }
        ++i;
    }

    if (highestPriIdx > -1 && highestPriData) {
        LayoutElement* sel = highestPriData->layoutElement;
        auto* cc = findElementConfigWithType(sel, ConfigType::Clip).clipElementConfig;
        bool canV = cc->vertical   && highestPriData->contentSize.height > sel->dimensions.height;
        bool canH = cc->horizontal && highestPriData->contentSize.width  > sel->dimensions.width;
        if (canV) highestPriData->scrollPosition.y += scrollDelta.y * 10;
        if (canH) highestPriData->scrollPosition.x += scrollDelta.x * 10;
        if (isPointerActive) {
            highestPriData->scrollMomentum = {};
            if (!highestPriData->pointerScrollActive) {
                highestPriData->pointerOrigin      = pointerInfo_.position;
                highestPriData->scrollOrigin        = highestPriData->scrollPosition;
                highestPriData->pointerScrollActive = true;
            } else {
                float dx = 0, dy = 0;
                if (canH) {
                    float old = highestPriData->scrollPosition.x;
                    highestPriData->scrollPosition.x = highestPriData->scrollOrigin.x +
                        (pointerInfo_.position.x - highestPriData->pointerOrigin.x);
                    highestPriData->scrollPosition.x = clay_max(clay_min(highestPriData->scrollPosition.x, 0.f),
                        -(highestPriData->contentSize.width - highestPriData->boundingBox.width));
                    dx = highestPriData->scrollPosition.x - old;
                }
                if (canV) {
                    float old = highestPriData->scrollPosition.y;
                    highestPriData->scrollPosition.y = highestPriData->scrollOrigin.y +
                        (pointerInfo_.position.y - highestPriData->pointerOrigin.y);
                    highestPriData->scrollPosition.y = clay_max(clay_min(highestPriData->scrollPosition.y, 0.f),
                        -(highestPriData->contentSize.height - highestPriData->boundingBox.height));
                    dy = highestPriData->scrollPosition.y - old;
                }
                if (dx > -0.1f && dx < 0.1f && dy > -0.1f && dy < 0.1f && highestPriData->momentumTime > 0.15f) {
                    highestPriData->momentumTime = 0;
                    highestPriData->pointerOrigin = pointerInfo_.position;
                    highestPriData->scrollOrigin  = highestPriData->scrollPosition;
                } else {
                    highestPriData->momentumTime += deltaTime;
                }
            }
        }
        float mxV = clay_max(highestPriData->contentSize.height - sel->dimensions.height, 0.f);
        float mxH = clay_max(highestPriData->contentSize.width  - sel->dimensions.width,  0.f);
        if (canV) highestPriData->scrollPosition.y = clay_max(clay_min(highestPriData->scrollPosition.y, 0.f), -mxV);
        if (canH) highestPriData->scrollPosition.x = clay_max(clay_min(highestPriData->scrollPosition.x, 0.f), -mxH);
    }
}

// ── getScrollOffset ───────────────────────────────────────────────────────────
Vector2 Context::getScrollOffset() {
    if (boolWarn_.maxElementsExceeded) return {};
    LayoutElement* el = getOpenLayoutElement();
    if (el->id == 0) generateIdForAnonymousElement(el);
    for (auto& sd : scrollContainerDatas_)
        if (sd.layoutElement == el) return sd.scrollPosition;
    return {};
}

// ── hovered / onHover / pointerOver ──────────────────────────────────────────
bool Context::hovered() {
    if (boolWarn_.maxElementsExceeded) return false;
    LayoutElement* el = getOpenLayoutElement();
    if (el->id == 0) generateIdForAnonymousElement(el);
    for (auto& pid : pointerOverIds_)
        if (pid.id == el->id) return true;
    return false;
}

void Context::onHover(OnHoverFn fn) {
    if (boolWarn_.maxElementsExceeded) return;
    LayoutElement* el = getOpenLayoutElement();
    if (el->id == 0) generateIdForAnonymousElement(el);
    auto* hmi = getHashMapItem(el->id);
    if (hmi && hmi != &hashMapItemDefault_) hmi->onHoverFn = std::move(fn);
}

bool Context::pointerOver(ElementId id) const {
    for (auto& pid : pointerOverIds_)
        if (pid.id == id.id) return true;
    return false;
}

// ── getElementData / getScrollContainerData ───────────────────────────────────
ElementData Context::getElementData(ElementId id) const {
    auto* item = const_cast<Context*>(this)->getHashMapItem(id.id);
    if (!item || item == &const_cast<Context*>(this)->hashMapItemDefault_) return {};
    return { item->boundingBox, true };
}

ScrollContainerData Context::getScrollContainerData(ElementId id) {
    for (auto& sd : scrollContainerDatas_) {
        if (sd.elementId == id.id) {
            auto* cc = findElementConfigWithType(sd.layoutElement, ConfigType::Clip).clipElementConfig;
            if (!cc) return {};
            return {
                &sd.scrollPosition,
                { sd.boundingBox.width, sd.boundingBox.height },
                sd.contentSize,
                *cc,
                true
            };
        }
    }
    return {};
}

// ── resetMeasureTextCache ─────────────────────────────────────────────────────
void Context::resetMeasureTextCache() {
    measureTextHashMapInternal_.clear();
    measureTextHashMapInternal_.resize(1);
    measureTextHashMapInternalFreeList_.clear();
    measureTextHashMap_.assign(measureTextHashMap_.size(), 0);
    measuredWords_.clear();
    measuredWordsFreeList_.clear();
}

// ── renderDebugView (stub) ────────────────────────────────────────────────────
void Context::renderDebugView() {
    // The debug view reuses the same public element/text API.
    // A full port would replicate Clay__RenderDebugView() using context.element()/text().
    // Omitted here for brevity — the debug overlay is not part of the core layout engine.
}

} // namespace clay

#endif // CLAY_IMPLEMENTATION
#endif // CLAY_HPP
