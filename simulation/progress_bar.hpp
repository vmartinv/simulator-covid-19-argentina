#ifndef PROGRESSBAR_PROGRESSBAR_HPP
#define PROGRESSBAR_PROGRESSBAR_HPP

#include <chrono>
#include <iostream>

class ProgressBar {
private:
    unsigned int ticks = 0;

    const unsigned int total_ticks;
    const unsigned int bar_width = 70;
    const char complete_char = '=';
    const char incomplete_char = ' ';
    const std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    const unsigned int step;
    const bool override_show = false;

    inline void display() {
        if(!override_show) return;
        float progress = (float) ticks / total_ticks;
        unsigned pos = (int) (bar_width * progress);
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now-start_time).count();

        std::cout << "[";

        for (unsigned int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << complete_char;
            else if (i == pos) std::cout << ">";
            else std::cout << incomplete_char;
        }
        std::cout << "] " << int(progress * 100.0) << "% "
                << float(time_elapsed) / 1000.0 << "s\r";
        std::cout.flush();
    }
public:
    static bool show;
    enum ShowMode {
        DEFAULT,
        HIDE,
        FORCE_SHOW
    };
    ProgressBar(unsigned int total, ShowMode show_mode=DEFAULT, unsigned int width=70) : total_ticks {total}, bar_width {width}, step {std::max(total/width, 1U)}, override_show {show_mode==FORCE_SHOW || (show_mode==DEFAULT && show) }  {}


    inline void operator++() {
        if(++ticks%step==0){
            display();
        }
    }

    inline void done()
    {
        ticks = total_ticks;
        display();
        if(override_show)
            std::cout << std::endl;
    }
};

#endif //PROGRESSBAR_PROGRESSBAR_HPP
