#pragma once
#include "core/project_model.h"
#include "core/generation_model.h"

struct AppState {
    ProjectState project;
    SceneInfo scene;
    std::vector<TemplateSession> templates;
    int current_template_index = 0;
};