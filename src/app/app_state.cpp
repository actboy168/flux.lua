#include "app/context.h"

namespace flux::app {

app_state::app_state()
    : services(flux::runtime::services()), ui(services.ui), draw(services.draw) {}

app_state& current() {
    static app_state data;
    return data;
}

void request_frame() {
    current().frame_requested = true;
}

}
