#pragma once
#include "model.hpp"
#include <cmath>
#include <cctype>

namespace tracker {
inline bool addPersonalNote(std::vector<std::string>& notes,std::string text) {
    text=trim(text);
    if (text.empty() || text.size()>1000 || notes.size()>=64) return false;
    size_t total=text.size(); for (const auto& note : notes) total+=note.size();
    if (total>16000) return false;
    notes.push_back(std::move(text)); return true;
}
inline bool deletePersonalNote(std::vector<std::string>& notes,size_t index) {
    if (index>=notes.size()) return false;
    notes.erase(notes.begin()+index); return true;
}
// Screen strings contain J2D formatting commands, not the original BMG tags.
inline std::string hintPlainText(std::string_view text) {
    std::string result;
    for (size_t i=0;i<text.size() && text[i];) {
        const auto c=static_cast<unsigned char>(text[i]);
        if (c==0x1b) {
            ++i;
            while (i<text.size() && std::isalpha(static_cast<unsigned char>(text[i]))) ++i;
            if (i<text.size() && text[i]=='[') {
                auto end=text.find(']',i);
                if (end==std::string_view::npos) break;
                i=end+1;
            }
        } else {
            if (c>=32 || c=='\n' || c=='\t') result+=text[i];
            ++i;
        }
    }
    return trim(result);
}
inline std::string hintSignAt(const Json& signs, const std::string& stage, int room,
                              float x, float y, float z) {
    for (const auto& sign : signs) {
        if (sign.at("stage")!=stage || sign.at("room")!=room) continue;
        const auto& p=sign.at("pos");
        if (std::abs(x-p[0].get<float>())<5 && std::abs(y-p[1].get<float>())<5 &&
            std::abs(z-p[2].get<float>())<5) return sign.at("name");
    }
    return {};
}
}
