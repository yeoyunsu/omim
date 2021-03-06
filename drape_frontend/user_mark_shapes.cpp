#include "drape_frontend/user_mark_shapes.hpp"

#include "drape_frontend/line_shape.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/poi_symbol_shape.hpp"
#include "drape_frontend/shader_def.hpp"
#include "drape_frontend/shape_view_params.hpp"
#include "drape_frontend/text_shape.hpp"
#include "drape_frontend/tile_utils.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/utils/vertex_decl.hpp"
#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"

#include "indexer/feature_decl.hpp"

#include "geometry/clipping.hpp"

namespace df
{

namespace
{

template <typename TCreateVector>
void AlignFormingNormals(TCreateVector const & fn, dp::Anchor anchor,
                         dp::Anchor first, dp::Anchor second,
                         glsl::vec2 & firstNormal, glsl::vec2 & secondNormal)
{
  firstNormal = fn();
  secondNormal = -firstNormal;
  if ((anchor & second) != 0)
  {
    firstNormal *= 2;
    secondNormal = glsl::vec2(0.0, 0.0);
  }
  else if ((anchor & first) != 0)
  {
    firstNormal = glsl::vec2(0.0, 0.0);
    secondNormal *= 2;
  }
}

void AlignHorizontal(float halfWidth, dp::Anchor anchor,
                     glsl::vec2 & left, glsl::vec2 & right)
{
  AlignFormingNormals([&halfWidth]{ return glsl::vec2(-halfWidth, 0.0f); }, anchor, dp::Left, dp::Right, left, right);
}

void AlignVertical(float halfHeight, dp::Anchor anchor,
                   glsl::vec2 & up, glsl::vec2 & down)
{
  AlignFormingNormals([&halfHeight]{ return glsl::vec2(0.0f, -halfHeight); }, anchor, dp::Top, dp::Bottom, up, down);
}

struct UserPointVertex : gpu::BaseVertex
{
  UserPointVertex() = default;
  UserPointVertex(TPosition const & pos, TNormal const & normal, TTexCoord const & texCoord, bool isAnim)
    : m_position(pos)
    , m_normal(normal)
    , m_texCoord(texCoord)
    , m_isAnim(isAnim ? 1.0f : -1.0f)
  {}

  static dp::BindingInfo GetBinding()
  {
    dp::BindingInfo info(4);
    uint8_t offset = 0;
    offset += dp::FillDecl<TPosition, UserPointVertex>(0, "a_position", info, offset);
    offset += dp::FillDecl<TNormal, UserPointVertex>(1, "a_normal", info, offset);
    offset += dp::FillDecl<TTexCoord, UserPointVertex>(2, "a_colorTexCoords", info, offset);
    offset += dp::FillDecl<bool, UserPointVertex>(3, "a_animate", info, offset);

    return info;
  }

  TPosition m_position;
  TNormal m_normal;
  TTexCoord m_texCoord;
  float m_isAnim;
};

} // namespace

void CacheUserMarks(TileKey const & tileKey, ref_ptr<dp::TextureManager> textures,
                    IDCollection const & marksId, UserMarksRenderCollection & renderParams,
                    dp::Batcher & batcher)
{
  float const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());
  using UPV = UserPointVertex;
  size_t const vertexCount = marksId.size() * dp::Batcher::VertexPerQuad;
  buffer_vector<UPV, 128> buffer;
  bool isAnimated = false;

  dp::TextureManager::SymbolRegion region;
  dp::GLState::DepthLayer depthLayer = dp::GLState::UserMarkLayer;
  for (auto const id : marksId)
  {
    auto const it = renderParams.find(id);
    ASSERT(it != renderParams.end(), ());

    UserMarkRenderParams & renderInfo = *it->second.get();
    if (!renderInfo.m_isVisible)
      continue;

    m2::PointD const tileCenter = tileKey.GetGlobalRect().Center();
    depthLayer = renderInfo.m_depthLayer;

    if (renderInfo.m_hasSymbolPriority)
    {
      PoiSymbolViewParams params(renderInfo.m_featureId);
      params.m_tileCenter = tileCenter;
      params.m_depth = renderInfo.m_depth;
      params.m_depthLayer = renderInfo.m_depthLayer;
      params.m_minVisibleScale = renderInfo.m_minZoom;
      params.m_specialDisplacement = SpecialDisplacement::UserMark;
      params.m_specialPriority = renderInfo.m_priority;
      params.m_symbolName = renderInfo.m_symbolName;
      PoiSymbolShape(renderInfo.m_pivot, params, tileKey,
                     0 /* textIndex */).Draw(&batcher, textures);
    }
    else
    {
      buffer.reserve(vertexCount);

      textures->GetSymbolRegion(renderInfo.m_symbolName, region);
      m2::RectF const & texRect = region.GetTexRect();
      m2::PointF const pxSize = region.GetPixelSize();
      dp::Anchor const anchor = renderInfo.m_anchor;
      m2::PointD const pt = MapShape::ConvertToLocal(renderInfo.m_pivot, tileCenter,
                                                     kShapeCoordScalar);
      glsl::vec3 const pos = glsl::vec3(glsl::ToVec2(pt), renderInfo.m_depth);
      bool const runAnim = renderInfo.m_hasCreationAnimation && renderInfo.m_justCreated;
      isAnimated |= runAnim;

      glsl::vec2 left, right, up, down;
      AlignHorizontal(pxSize.x * 0.5f, anchor, left, right);
      AlignVertical(pxSize.y * 0.5f, anchor, up, down);

      m2::PointD const pixelOffset = renderInfo.m_pixelOffset;
      glsl::vec2 const offset(pixelOffset.x, pixelOffset.y);

      buffer.emplace_back(pos, left + down + offset, glsl::ToVec2(texRect.LeftTop()), runAnim);
      buffer.emplace_back(pos, left + up + offset, glsl::ToVec2(texRect.LeftBottom()), runAnim);
      buffer.emplace_back(pos, right + down + offset, glsl::ToVec2(texRect.RightTop()), runAnim);
      buffer.emplace_back(pos, right + up + offset, glsl::ToVec2(texRect.RightBottom()), runAnim);
    }

    if (renderInfo.m_titleDecl != nullptr && !renderInfo.m_titleDecl->m_primaryText.empty())
    {
      TextViewParams params;
      params.m_featureID = renderInfo.m_featureId;
      params.m_tileCenter = tileCenter;
      params.m_titleDecl = *renderInfo.m_titleDecl;

      // Here we use visual scale to adapt texts sizes and offsets
      // to different screen resolutions and DPI.
      params.m_titleDecl.m_primaryTextFont.m_size *= vs;
      params.m_titleDecl.m_secondaryTextFont.m_size *= vs;
      params.m_titleDecl.m_primaryOffset *= vs;
      params.m_titleDecl.m_secondaryOffset *= vs;

      params.m_depth = renderInfo.m_depth;
      params.m_depthLayer = renderInfo.m_depthLayer;
      params.m_minVisibleScale = renderInfo.m_minZoom;
      if (renderInfo.m_hasTitlePriority)
      {
        params.m_specialDisplacement = SpecialDisplacement::UserMark;
        params.m_specialPriority = renderInfo.m_priority;
      }

      dp::TextureManager::SymbolRegion region;
      textures->GetSymbolRegion(renderInfo.m_symbolName, region);
      m2::PointF const symbolSize = region.GetPixelSize();

      TextShape(renderInfo.m_pivot, params, tileKey, renderInfo.m_hasSymbolPriority /* hasPOI */,
                symbolSize, renderInfo.m_anchor, 0 /* textIndex */).Draw(&batcher, textures);
    }

    renderInfo.m_justCreated = false;
  }

  if (!buffer.empty())
  {
    dp::GLState state(isAnimated ? gpu::BOOKMARK_ANIM_PROGRAM : gpu::BOOKMARK_PROGRAM, depthLayer);
    state.SetProgram3dIndex(isAnimated ? gpu::BOOKMARK_ANIM_BILLBOARD_PROGRAM
                                       : gpu::BOOKMARK_BILLBOARD_PROGRAM);
    state.SetColorTexture(region.GetTexture());
    state.SetTextureFilter(gl_const::GLNearest);

    dp::AttributeProvider attribProvider(1, static_cast<uint32_t>(buffer.size()));
    attribProvider.InitStream(0, UPV::GetBinding(), make_ref(buffer.data()));

    batcher.InsertListOfStrip(state, make_ref(&attribProvider), dp::Batcher::VertexPerQuad);
  }
}

void CacheUserLines(TileKey const & tileKey, ref_ptr<dp::TextureManager> textures,
                    IDCollection const & linesId, UserLinesRenderCollection & renderParams,
                    dp::Batcher & batcher)
{
  float const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());
  int const kLineSimplifyLevelEnd = 15;
  bool const simplify = tileKey.m_zoomLevel <= kLineSimplifyLevelEnd;

  double sqrScale;
  if (simplify)
  {
    double const currentScaleGtoP = 1.0 / GetScale(tileKey.m_zoomLevel);
    sqrScale = math::sqr(currentScaleGtoP);
  }

  for (auto id : linesId)
  {
    auto const it = renderParams.find(id);
    ASSERT(it != renderParams.end(), ());
    UserLineRenderParams const & renderInfo = *it->second.get();

    m2::SharedSpline spline = renderInfo.m_spline;
    if (simplify)
    {
      spline.Reset(new m2::Spline(renderInfo.m_spline->GetSize()));

      static double const kMinSegmentLength = math::sqr(4.0 * vs);
      m2::PointD lastAddedPoint;
      for (auto const & point : renderInfo.m_spline->GetPath())
      {
        if (spline->GetSize() > 1 && point.SquareLength(lastAddedPoint) * sqrScale < kMinSegmentLength)
        {
          spline->ReplacePoint(point);
        }
        else
        {
          spline->AddPoint(point);
          lastAddedPoint = point;
        }
      }
    }

    auto const clippedSplines = m2::ClipSplineByRect(tileKey.GetGlobalRect(), spline);
    for (auto const & clippedSpline : clippedSplines)
    {
      for (auto const & layer : renderInfo.m_layers)
      {
        LineViewParams params;
        params.m_tileCenter = tileKey.GetGlobalRect().Center();
        params.m_baseGtoPScale = 1.0f;
        params.m_cap = dp::RoundCap;
        params.m_join = dp::RoundJoin;
        params.m_color = layer.m_color;
        params.m_depth = layer.m_depth;
        params.m_depthLayer = renderInfo.m_depthLayer;
        params.m_width = layer.m_width * vs;
        params.m_minVisibleScale = 1;
        params.m_rank = 0;

        LineShape(clippedSpline, params).Draw(make_ref(&batcher), textures);
      }
    }
  }
}
} // namespace df
