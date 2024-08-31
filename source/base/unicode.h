#ifndef UNICODE_H
#define UNICODE_H

namespace bite::unicode {
    // TODO: refactor
    // TODO: deobfuscate (https://stackoverflow.com/a/70760522 maybe?)
    /**
     *
     * @tparam T container with .push_back(uint8_t or equivalent) method
     * @param buffer location where to write utf8 encoded bytes
     * @param codepoint unicode scalar value
     * @return true on success
     */
    template <typename T>
    bool codepoint_to_utf8(T& buffer, char32_t codepoint) {
        if (codepoint < 0x80) {
            buffer.push_back(codepoint);
        } else if (codepoint < 0x800) {
            buffer.push_back(192 + codepoint / 64);
            buffer.push_back(128 + codepoint % 64);
        } else if (codepoint - 0xd800U < 0x800) {
            return false;
        } else if (codepoint < 0x10000) {
            buffer.push_back(224 + codepoint / 4096);
            buffer.push_back(128 + codepoint / 64 % 64), buffer.push_back(128 + codepoint % 64);
        } else if (codepoint < 0x110000) {
            buffer.push_back(240 + codepoint / 262144);
            buffer.push_back(128 + codepoint / 4096 % 64);
            buffer.push_back(128 + codepoint / 64 % 64);
            buffer.push_back(128 + codepoint % 64);
        } else {
            return false;
        }

        return true;
    }
}


#endif //UNICODE_H
