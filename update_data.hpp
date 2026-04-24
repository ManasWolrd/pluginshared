#pragma once
#include <juce_core/juce_core.h>

namespace pluginshared {
class UpdateData {
public:
    struct GithubInfo {
        std::string_view owner;
        std::string_view repo_name;
    };

    UpdateData(GithubInfo info)
        : github_info_(info) {}

    juce::String GetPluginReleaseUrl() {
        juce::String s{"https://github.com/"};
        s << github_info_.owner.data() << "/" << github_info_.repo_name.data() << "/releases/latest";
        return s;
    }
private:
    GithubInfo github_info_;
};
} // namespace pluginshared
