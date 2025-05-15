#include "header.h"

uintptr_t found_offset;
HANDLE process_handle;
size_t skip_table[256];

std::vector<BYTE> hexStringToByteVector(const std::string& hex) {
    std::vector<BYTE> bytes;
    std::istringstream hex_stream(hex);
    std::string byte;
    //std::cout << "Here 1" << std::endl;
    while (hex_stream >> byte) {
        if (byte == "?") {
            bytes.push_back(0xFF);
        }else {
            bytes.push_back(static_cast<BYTE>(std::stoi(byte, nullptr, 16)));
        }
    }
    return bytes;
}

bool find_pattern_in_memory(const std::vector<BYTE>& buffer, const std::vector<BYTE>& pattern, size_t& found_offset, uintptr_t address) {
    bool status = false;
    const size_t buffer_size = buffer.size();
    const size_t pattern_size = pattern.size();
    const size_t limit = buffer_size - pattern_size;

    std::fill(std::begin(skip_table), std::end(skip_table), pattern_size);
    for (size_t i = 0; i < pattern_size - 1; ++i) {
        if (pattern[i] != 0xFF) {
            skip_table[pattern[i]] = pattern_size - 1 - i;
        }
    }

    size_t last = pattern_size - 1;
    size_t i = 0;
    while (i <= limit)
    {
        size_t j = last;

        // Compare pattern from end to beginning
        while (j != SIZE_MAX && (pattern[j] == 0xFF || buffer[i + j] == pattern[j])) {
            if (j == 0) {
                found_offset = i;
                status = true;
                std::cout << std::hex << "Found match at address => " << address + found_offset << std::endl;
            }
            --j;
        }

        BYTE skip_byte = buffer[i + last];
        if (pattern[last] == 0xFF) {
            i += 1;
        } else {
            i += skip_table[skip_byte];
        }
    }
    if (status)
    {
        return true;
    }
    
    return false;
}

void read_all_memory_once(DWORD processId, const std::vector<BYTE>& byte_pattern, uintptr_t s_point) {
    process_handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId);
    
    if (process_handle == NULL) {
        //std::cerr << "Failed to open process handle. Error code: " << GetLastError() << std::endl;
        return;
    }
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t address = s_point;
    while (VirtualQueryEx(process_handle, (LPCVOID)address, &mbi, sizeof(mbi))) {
        bool is_committed = mbi.State == MEM_COMMIT;
        bool is_readable = mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE);
        bool is_not_guard = !(mbi.Protect & PAGE_GUARD);
        bool is_not_noaccess = !(mbi.Protect & PAGE_NOACCESS);
        bool is_valid_type = mbi.Type == MEM_PRIVATE || mbi.Type == MEM_IMAGE;

        if (is_committed && is_readable && is_not_guard && is_not_noaccess && is_valid_type) {
            static std::vector<BYTE> buffer;
            if (buffer.size() < mbi.RegionSize) buffer.resize(mbi.RegionSize);
            SIZE_T bytesRead;

            if (ReadProcessMemory(process_handle, (LPCVOID)address, buffer.data(), mbi.RegionSize, &bytesRead)) {
                buffer.resize(bytesRead);
                //std::cout << "Read " << bytesRead << " bytes from address: " << std::hex << address << std::endl;

                size_t found_offset = 0;

                if (find_pattern_in_memory(buffer, byte_pattern, found_offset, address)) {
                    //std::cout << std::hex << "Match Found at Region >> " << address << std::endl;
                }
            } else {
                std::cerr << "Failed to read memory at address: " << std::hex << address << " Error code: " << GetLastError() << std::endl;
            }
        }
        address += mbi.RegionSize;
    }
    CloseHandle(process_handle);
}

int main()
{
    std::string pattern;
    DWORD processID;

    std::cout << "Enter process ID: ";
    std::cin >> processID;
    std::cin.ignore(); // Clear leftover newline from input buffer

    std::cout << "Enter hex pattern (use '?' for wildcards, e.g. '8B 45 ? 89 45 FC'): ";
    std::getline(std::cin, pattern);

    std::vector<BYTE> byte_pattern = hexStringToByteVector(pattern);

    // Starting address, change this depending on your target process memory layout
    uintptr_t scan_start = 0x00000000;

    read_all_memory_once(processID, byte_pattern, scan_start);
    return 0;
}