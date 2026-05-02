#include <lazytmux/frecency_store.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <system_error>
#include <utility>

namespace lazytmux::frecency {
namespace {

constexpr std::string_view kSessions = "sessions";
constexpr std::string_view kName = "name";
constexpr std::string_view kVisits = "visits";
constexpr std::string_view kLastUsed = "last_used";

Result<std::string> read_all(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return std::unexpected(Error(std::format("failed to open {}", path.string())));
    }
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        return std::unexpected(Error(std::format("failed to read {}", path.string())));
    }
    return body;
}

Result<std::map<tmux::SessionName, Entry>> parse_entries(const nlohmann::json& doc) {
    if (!doc.is_object() || !doc.contains(kSessions) || !doc.at(kSessions).is_array()) {
        return std::unexpected(Error("frecency file must contain a sessions array"));
    }

    std::map<tmux::SessionName, Entry> entries;
    for (const auto& row : doc.at(kSessions)) {
        if (!row.is_object() || !row.contains(kName) || !row.contains(kVisits) ||
            !row.contains(kLastUsed)) {
            return std::unexpected(Error("frecency row is missing required fields"));
        }
        if (!row.at(kName).is_string() || !row.at(kVisits).is_number_unsigned() ||
            !row.at(kLastUsed).is_number_integer()) {
            return std::unexpected(Error("frecency row has the wrong field types"));
        }

        auto name = tmux::SessionName::make(row.at(kName).get<std::string>());
        if (!name) {
            return std::unexpected(std::move(name).error().with_context("parse frecency name"));
        }
        auto visits = row.at(kVisits).get<std::uint32_t>();
        auto last_used = row.at(kLastUsed).get<std::int64_t>();
        entries.emplace(std::move(*name), Entry{.visits = visits, .last_used = last_used});
    }
    return entries;
}

nlohmann::json to_json(const Index& index) {
    nlohmann::json rows = nlohmann::json::array();
    for (const auto& [name, entry] : index.entries()) {
        rows.push_back({
            {std::string{kName}, std::string{name.as_str()}},
            {std::string{kVisits}, entry.visits},
            {std::string{kLastUsed}, entry.last_used},
        });
    }
    return nlohmann::json{{std::string{kSessions}, std::move(rows)}};
}

}  // namespace

Result<Index> load_index(const std::filesystem::path& path) {
    std::error_code exists_ec;
    if (!std::filesystem::exists(path, exists_ec)) {
        if (exists_ec) {
            return std::unexpected(
                Error(std::format("failed to inspect {}: {}", path.string(), exists_ec.message())));
        }
        return Index{};
    }

    auto body = read_all(path);
    if (!body) {
        return std::unexpected(std::move(body).error());
    }
    try {
        auto doc = nlohmann::json::parse(*body);
        auto entries = parse_entries(doc);
        if (!entries) {
            return std::unexpected(std::move(entries).error());
        }
        return Index{std::move(*entries)};
    } catch (const nlohmann::json::exception& e) {
        return std::unexpected(
            Error(std::format("failed to parse {}: {}", path.string(), e.what())));
    }
}

Result<void> save_index(const Index& index, const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code mkdir_ec;
        std::filesystem::create_directories(parent, mkdir_ec);
        if (mkdir_ec) {
            return std::unexpected(
                Error(std::format("failed to create {}: {}", parent.string(), mkdir_ec.message())));
        }
    }

    auto tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            return std::unexpected(Error(std::format("failed to open {}", tmp.string())));
        }
        out << to_json(index).dump(2) << '\n';
        if (!out) {
            return std::unexpected(Error(std::format("failed to write {}", tmp.string())));
        }
    }

    std::error_code rename_ec;
    std::filesystem::rename(tmp, path, rename_ec);
    if (rename_ec) {
        std::error_code remove_ec;
        std::filesystem::remove(tmp, remove_ec);
        return std::unexpected(Error(std::format(
            "failed to rename {} to {}: {}", tmp.string(), path.string(), rename_ec.message())));
    }
    return {};
}

}  // namespace lazytmux::frecency
