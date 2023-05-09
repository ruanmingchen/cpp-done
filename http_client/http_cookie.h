#ifndef __HTTPCOOKIE_H__
#define __HTTPCOOKIE_H__

#include <string>
#include <vector>

namespace net
{

struct CookieInfo {
    std::string domain;
    bool tailMatch;
    std::string path;
    bool secure;
    std::string expires;
    std::string name;
    std::string value;
};

class HttpCookie
{
public:
    explicit HttpCookie(std::string fullPathFilename);
    ~HttpCookie() = default;

    void readFile();
    void writeFile();
    const std::vector<CookieInfo>* getCookies() const;
    const CookieInfo* getMatchCookie(const std::string& url) const;
    void updateOrAddCookie(const CookieInfo& cookie);

private:
    std::string getStringFromFile(const std::string& fullPathFilename);

private:
    std::string cookieFilename_;
    std::vector<CookieInfo> cookies_;
};

}  // namespace net
#endif  // __HTTPCOOKIE_H__