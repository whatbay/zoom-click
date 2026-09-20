#include "zoom-filter.hpp"
#include <obs-frontend-api.h>
#include <algorithm>
#include <cmath>

ZoomEventFilter::ZoomEventFilter(QWidget *preview, QObject *parent)
    : QObject(parent), previewWidget(preview)
{
    animTimer = new QTimer(this);
    connect(animTimer, &QTimer::timeout, this, &ZoomEventFilter::updateAnimation);
    animTimer->start(16); // 約60fpsでスムーズなアニメーション補間
}

ZoomEventFilter::~ZoomEventFilter()
{
    if (animTimer) animTimer->stop();
}

bool ZoomEventFilter::eventFilter(QObject *obj, QEvent *event)
{
    if (!enabled) return false;

    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            handleLeftClick(me->position());
            return true; // OBS標準の選択・移動イベントをキャプチャして抑止
        }
    }
    else if (event->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent*>(event);
        handleWheel(we->angleDelta().y());
        return true;
    }
    else if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            resetZoom();
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

void ZoomEventFilter::handleLeftClick(const QPointF &pos)
{
    if (!previewWidget) return;

    // プレビュー表示サイズ内での正規化座標(0.0〜1.0)を取得
    float normX = std::clamp(static_cast<float>(pos.x() / previewWidget->width()), 0.0f, 1.0f);
    float normY = std::clamp(static_cast<float>(pos.y() / previewWidget->height()), 0.0f, 1.0f);

    centerX = normX;
    centerY = normY;

    // 1.0倍状態ならデフォルトで2.0倍へズーム、既にズーム中なら位置のみ中心変更
    if (targetZoom <= 1.05f) {
        targetZoom = 2.0f;
    }

    obs_sceneitem_t *item = getSelectedOrFirstSceneItem();
    if (!item) return;

    obs_source_t *source = obs_sceneitem_get_source(item);
    uint32_t width = obs_source_get_width(source);
    uint32_t height = obs_source_get_height(source);
    if (width == 0 || height == 0) return;

    int cL, cR, cT, cB;
    calculateCropBounds(targetZoom, centerX, centerY, cL, cR, cT, cB, width, height);

    targetCropL = static_cast<float>(cL);
    targetCropR = static_cast<float>(cR);
    targetCropT = static_cast<float>(cT);
    targetCropB = static_cast<float>(cB);
}

void ZoomEventFilter::handleWheel(int delta)
{
    if (delta > 0) {
        targetZoom = std::min(MAX_ZOOM, targetZoom + 0.3f);
    } else {
        targetZoom = std::max(MIN_ZOOM, targetZoom - 0.3f);
    }

    if (targetZoom <= 1.0f) {
        resetZoom();
        return;
    }

    obs_sceneitem_t *item = getSelectedOrFirstSceneItem();
    if (!item) return;

    obs_source_t *source = obs_sceneitem_get_source(item);
    uint32_t width = obs_source_get_width(source);
    uint32_t height = obs_source_get_height(source);

    int cL, cR, cT, cB;
    calculateCropBounds(targetZoom, centerX, centerY, cL, cR, cT, cB, width, height);

    targetCropL = static_cast<float>(cL);
    targetCropR = static_cast<float>(cR);
    targetCropT = static_cast<float>(cT);
    targetCropB = static_cast<float>(cB);
}

void ZoomEventFilter::resetZoom()
{
    targetZoom = 1.0f;
    targetCropL = targetCropR = targetCropT = targetCropB = 0.0f;
}

void ZoomEventFilter::calculateCropBounds(float zoom, float cX, float cY,
                                          int &cropL, int &cropR, int &cropT, int &cropB,
                                          uint32_t srcWidth, uint32_t srcHeight)
{
    float visibleWidth = static_cast<float>(srcWidth) / zoom;
    float visibleHeight = static_cast<float>(srcHeight) / zoom;

    float left = (cX * srcWidth) - (visibleWidth / 2.0f);
    float top = (cY * srcHeight) - (visibleHeight / 2.0f);

    left = std::clamp(left, 0.0f, static_cast<float>(srcWidth) - visibleWidth);
    top = std::clamp(top, 0.0f, static_cast<float>(srcHeight) - visibleHeight);

    cropL = static_cast<int>(left);
    cropT = static_cast<int>(top);
    cropR = static_cast<int>(srcWidth - (left + visibleWidth));
    cropB = static_cast<int>(srcHeight - (top + visibleHeight));
}

void ZoomEventFilter::updateAnimation()
{
    // イージング補間 (Lerp)
    float lerpFactor = 0.15f;
    curCropL += (targetCropL - curCropL) * lerpFactor;
    curCropR += (targetCropR - curCropR) * lerpFactor;
    curCropT += (targetCropT - curCropT) * lerpFactor;
    curCropB += (targetCropB - curCropB) * lerpFactor;

    obs_sceneitem_t *item = getSelectedOrFirstSceneItem();
    if (!item) return;

    struct obs_sceneitem_crop crop;
    crop.left = static_cast<int>(curCropL);
    crop.right = static_cast<int>(curCropR);
    crop.top = static_cast<int>(curCropT);
    crop.bottom = static_cast<int>(curCropB);

    obs_sceneitem_defer_update_begin(item);
    obs_sceneitem_set_crop(item, &crop);
    obs_sceneitem_defer_update_end(item);
}

obs_sceneitem_t* ZoomEventFilter::getSelectedOrFirstSceneItem()
{
    obs_source_t *current_scene_source = obs_frontend_get_current_scene();
    if (!current_scene_source) return nullptr;

    obs_scene_t *scene = obs_scene_from_source(current_scene_source);
    obs_source_release(current_scene_source);

    if (!scene) return nullptr;

    // 最初に選択されているシーンアイテムを取得
    struct search_data {
        obs_sceneitem_t *item = nullptr;
    } data;

    obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *param) {
        auto *d = static_cast<search_data*>(param);
        if (!d->item && obs_sceneitem_visible(item)) {
            d->item = item;
        }
        return true;
    }, &data);

    return data.item;
}