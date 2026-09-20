#pragma once

#include <QObject>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QWidget>
#include <obs.h>

class ZoomEventFilter : public QObject {
    Q_OBJECT

public:
    explicit ZoomEventFilter(QWidget *previewWidget, QObject *parent = nullptr);
    ~ZoomEventFilter();

    void setEnabled(bool enable) { enabled = enable; }
    bool isEnabled() const { return enabled; }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void updateAnimation();

private:
    void handleLeftClick(const QPointF &pos);
    void handleWheel(int delta);
    void resetZoom();
    
    obs_sceneitem_t* getSelectedOrFirstSceneItem();
    void calculateCropBounds(float zoomFactor, float centerXRatio, float centerYRatio,
                             int &cropL, int &cropR, int &cropT, int &cropB,
                             uint32_t srcWidth, uint32_t srcHeight);

    QWidget *previewWidget;
    QTimer *animTimer;
    bool enabled = true;

    // ズーム状態パラメータ
    float currentZoom = 1.0f;
    float targetZoom = 1.0f;
    float centerX = 0.5f; // 0.0 - 1.0 (相対座標)
    float centerY = 0.5f;

    // 現在の補間用クロップ値
    float curCropL = 0, curCropR = 0, curCropT = 0, curCropB = 0;
    float targetCropL = 0, targetCropR = 0, targetCropT = 0, targetCropB = 0;

    const float MAX_ZOOM = 5.0f;
    const float MIN_ZOOM = 1.0f;
};