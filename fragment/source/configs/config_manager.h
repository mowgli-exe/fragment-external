#pragma once

#include <string>
#include <vector>

namespace config_manager
{
    void initialize();
    void refresh();

    std::string sanitize(const std::string& name);

    bool save(const std::string& name);
    bool export_to_string(const std::string& name, std::string& out);
    bool create(const std::string& name);
    bool load(const std::string& name);
    bool remove(const std::string& name);
    void open_directory();

    const std::vector<std::string>& get_configs();
    const std::string& get_status();
    bool status_is_error();
}
