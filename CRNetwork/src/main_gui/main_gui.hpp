#pragma once

#include <render_pipeline/rppanda/showbase/direct_object.hpp>

class MainApp;

class MainGUI : public rppanda::DirectObject
{
public:
    MainGUI(MainApp& app);

    virtual ~MainGUI();

    void show();
    void hide();

private:
    void on_imgui_new_frame();

    void ui_openvr();

    bool is_open_ = true;
    MainApp& app_;

    std::string today_date_;
};

inline void MainGUI::show()
{
    is_open_ = true;
}

inline void MainGUI::hide()
{
    is_open_ = false;
}
