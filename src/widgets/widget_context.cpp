#include "widgets/widget_context.h"

#include "runtime/services.h"

namespace flux::widgets {

flux::ui::ui_context& widget_ui() {
    return flux::runtime::services().ui;
}

}
