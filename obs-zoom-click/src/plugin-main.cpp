#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QWidget>
#include "zoom-filter.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-zoom-click", "en-US")

static ZoomEventFilter *g_zoomFilter = nullptr;

static void frontend_event_callback(enum obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        QMainWindow *mainWin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
        if (!mainWin) return;

        // OBS Basic Preview ウィジェットを取得
        QWidget *preview = mainWin->findChild<QWidget*>("preview");
        if (preview) {
            g_zoomFilter = new ZoomEventFilter(preview, mainWin);
            preview->installEventFilter(g_zoomFilter);
        }
    }
}

bool obs_module_load(void)
{
    obs_frontend_add_event_callback(frontend_event_callback, nullptr);
    return true;
}

void obs_module_unload(void)
{
    if (g_zoomFilter) {
        delete g_zoomFilter;
        g_zoomFilter = nullptr;
    }
}