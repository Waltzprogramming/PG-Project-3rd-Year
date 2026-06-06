#pragma once

#include <string>

class AudioPlayer {
public:
    AudioPlayer();
    explicit AudioPlayer(std::wstring alias);
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;
    ~AudioPlayer();

    bool open(const std::string& filePath);
    bool playLoop();
    bool playOnce();
    bool setVolume(int volume);
    void stop();
    void close();

private:
    std::wstring m_alias;
    bool m_open{false};
};
