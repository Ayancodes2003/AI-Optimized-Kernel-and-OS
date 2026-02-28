#ifndef __AIE_OS_COMMON_CONFIG_H
#define __AIE_OS_COMMON_CONFIG_H

#include <string>
#include <map>

namespace aie {

class Config {
public:
    Config();
    ~Config();

    /* Load configuration file at 'path'. Returns 0 on success. */
    int load(const std::string &path);

    /* Retrieve a value, or default if missing */
    std::string get(const std::string &key, const std::string &def = "") const;

    bool has(const std::string &key) const;

private:
    std::map<std::string, std::string> data_;
    static std::string trim(const std::string &s);
};

} // namespace aie

#endif /* __AIE_OS_COMMON_CONFIG_H */
