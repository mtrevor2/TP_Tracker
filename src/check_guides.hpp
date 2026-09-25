#pragma once
#include "model.hpp"
#include <stdexcept>

namespace tracker {
// Presentation-only data, deliberately separate from inventory/seed/solver state.
class CheckGuides {
    Json data_ = Json::object();
public:
    void load(const Json& data) {
        if (!data.is_object() || data.value("version",0)!=1 ||
            !data.at("checks").is_object() || !data.at("sources").is_object())
            throw std::runtime_error("Unsupported collection-guide resource");
        const auto& sources=data.at("sources");
        for (const auto& [id,source] : sources.items()) {
            if (!source.at("label").is_string() || !source.at("url").is_string() ||
                source.at("label").get_ref<const std::string&>().empty() ||
                source.at("label").get_ref<const std::string&>().size()>256 ||
                !source.at("url").get_ref<const std::string&>().starts_with("https://"))
                throw std::runtime_error("Invalid collection-guide source");
        }
        for (const auto& [name,guide] : data.at("checks").items()) {
            if (!guide.at("steps").is_array() || guide.at("steps").empty() || guide.at("steps").size()>8 ||
                !guide.at("sources").is_array() || guide.at("sources").empty())
                throw std::runtime_error("Invalid collection guide: "+name);
            for (const auto& step : guide.at("steps"))
                if (!step.is_string() || trim(step.get<std::string>()).empty() || step.get_ref<const std::string&>().size()>1600)
                    throw std::runtime_error("Invalid collection-guide step");
            for (const auto& id : guide.at("sources"))
                if (!id.is_string() || !sources.contains(id.get<std::string>()))
                    throw std::runtime_error("Missing collection-guide source");
            const auto kind=guide.value("kind",std::string{});
            if (!kind.empty() && kind!="hint_sign" && kind!="twilit_insect")
                throw std::runtime_error("Invalid collection-guide kind");
        }
        data_=data; // Commit only after validation; bad reloads preserve the previous guidebook.
    }
    std::vector<std::string> blocks(const std::string& name) const {
        std::vector<std::string> out={"<div class='tp-group'>How to obtain</div>"};
        if (!data_.contains("checks") || !data_.at("checks").contains(name)) {
            out.push_back("<div>Collection directions are unavailable for this check.</div>");
            return out;
        }
        const auto& guide=data_.at("checks").at(name);
        for (const auto& step : guide.at("steps"))
            out.push_back("<div>"+escape(step.get<std::string>())+"</div>");
        const auto kind=guide.value("kind",std::string{});
        if(kind=="hint_sign") out.push_back("<div>Read every page of the sign. TPTracker saves the displayed hint in Notes and marks the sign done.</div>");
        if(kind=="twilit_insect") out.push_back("<div>Use Wolf Link&#39;s senses to reveal the insect, defeat it, and collect its drop. The game marks these insects on its own map.</div>");
        out.push_back("<div class='tp-note'>Collection guide only. The requirements on the left use your seed settings and inventory; randomized rewards may differ.</div>");
        std::string credits;
        for (const auto& id : guide.at("sources")) {
            if(!credits.empty()) credits+="; ";
            credits+=data_.at("sources").at(id.get<std::string>()).at("label").get<std::string>();
        }
        out.push_back("<div class='tp-note'>Sources: "+escape(credits)+"</div>");
        return out;
    }
};
}
