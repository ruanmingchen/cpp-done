#include "http_client.h"

int main()
{
    net::HttpClient client;
    std::string url = R"(www.xxxx.com)";
    client.initThreadpool(1);
    auto request = std::make_shared<net::HttpRequest>();
    request->setRequestType(net::HttpRequest::Type::GET);
    request->setUrl(url);
    request->setCallback([](const std::shared_ptr<net::HttpRequest>& request, const std::shared_ptr<net::HttpResponse>& response)
                         {
                             if (response->isSucceed()) {
                                 //TODO
                             }
                         });

    client.send(request);
}