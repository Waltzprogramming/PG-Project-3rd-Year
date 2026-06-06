#include "AudioPlayer.h"

#include <Windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <utility>

namespace {
std::atomic_uint nextAudioChannel{1};

std::wstring widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        return std::wstring(value.begin(), value.end());
    }

    std::wstring result(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), required);
    result.pop_back();
    return result;
}

bool sendMci(const std::wstring& command) {
    wchar_t errorText[256]{};
    const MCIERROR result = mciSendStringW(command.c_str(), nullptr, 0, nullptr);
    if (result == 0) {
        return true;
    }

    mciGetErrorStringW(result, errorText, static_cast<UINT>(std::size(errorText)));
    std::wcerr << L"MCI audio command failed: " << command << L" -> " << errorText << std::endl;
    return false;
}

std::wstring normalizeMciPath(const std::string& filePath) {
    std::filesystem::path path(filePath);
    if (!path.is_absolute()) {
        path = std::filesystem::absolute(path);
    }
    path = path.lexically_normal();
    std::wstring native = path.native();
    std::replace(native.begin(), native.end(), L'/', L'\\');
    return native;
}
}

AudioPlayer::AudioPlayer()
    : m_alias(L"audio_channel_" + std::to_wstring(nextAudioChannel.fetch_add(1))) {
}

AudioPlayer::AudioPlayer(std::wstring alias)
    : m_alias(std::move(alias)) {
}

AudioPlayer::~AudioPlayer() {
    close();
}

bool AudioPlayer::open(const std::string& filePath) {
    close();

    std::error_code errorCode;
    std::filesystem::path resolved(filePath);
    if (!resolved.is_absolute()) {
        resolved = std::filesystem::absolute(resolved, errorCode);
    }
    resolved = resolved.lexically_normal();
    if (!std::filesystem::exists(resolved, errorCode)) {
        std::cerr << "Audio file does not exist: " << resolved.string() << std::endl;
        return false;
    }

    const std::wstring path = normalizeMciPath(resolved.string());
    std::wstringstream command;
    command << L"open \"" << path << L"\" type mpegvideo alias " << m_alias;
    m_open = sendMci(command.str());
    if (!m_open) {
        std::wstringstream fallbackCommand;
        fallbackCommand << L"open \"" << path << L"\" alias " << m_alias;
        m_open = sendMci(fallbackCommand.str());
    }
    if (m_open) {
        sendMci(L"set " + m_alias + L" time format milliseconds");
        setVolume(900);
    }
    return m_open;
}

bool AudioPlayer::playLoop() {
    if (!m_open) {
        return false;
    }
    return sendMci(L"play " + m_alias + L" repeat");
}

bool AudioPlayer::playOnce() {
    if (!m_open) {
        return false;
    }

    sendMci(L"stop " + m_alias);
    if (!sendMci(L"seek " + m_alias + L" to start")) {
        return false;
    }
    return sendMci(L"play " + m_alias);
}

bool AudioPlayer::setVolume(int volume) {
    if (!m_open) {
        return false;
    }

    const int clampedVolume = std::clamp(volume, 0, 1000);
    return sendMci(L"setaudio " + m_alias + L" volume to " + std::to_wstring(clampedVolume));
}

void AudioPlayer::stop() {
    if (m_open) {
        sendMci(L"stop " + m_alias);
    }
}

void AudioPlayer::close() {
    if (!m_open) {
        return;
    }
    stop();
    sendMci(L"close " + m_alias);
    m_open = false;
}
