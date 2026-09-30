#pragma once
#include <windows.h>
#include <cstdio>

template<typename T>
class SharedMemory {
public:
    ~SharedMemory() { Close(); }

    bool Create(const wchar_t* name) {
        m_map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
            PAGE_READWRITE, 0, sizeof(T), name);
        if (!m_map) return false;
        m_data = (T*)MapViewOfFile(m_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(T));
        if (!m_data) { CloseHandle(m_map); m_map = nullptr; return false; }
        memset(m_data, 0, sizeof(T));
        return true;
    }

    bool Open(const wchar_t* name) {
        m_map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
        if (!m_map) return false;
        m_data = (T*)MapViewOfFile(m_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(T));
        if (!m_data) { CloseHandle(m_map); m_map = nullptr; return false; }
        return true;
    }

    void Close() {
        if (m_data) { UnmapViewOfFile(m_data); m_data = nullptr; }
        if (m_map) { CloseHandle(m_map); m_map = nullptr; }
    }

    T* Get() { return m_data; }
    explicit operator bool() const { return m_data != nullptr; }

private:
    HANDLE m_map = nullptr;
    T* m_data = nullptr;
};
