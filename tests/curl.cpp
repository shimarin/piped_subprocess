#include <sstream>
#include <memory>

#include <curl/curl.h>
#include <libxml/parser.h>

#include "../piped_subprocess.h"

static const char* url = 
    "http://ftp.iij.ad.jp/pub/linux/centos-vault/6.10/os/x86_64/repodata/8a106b58d2b45b4757ebba9f431cfcd6197392b5ee7640ab288b062fa754822c-primary.xml.gz";

int main()
{
    std::shared_ptr<xmlDoc> doc;
    piped_subprocess::ENSURE_OK(piped_subprocess::exec("gunzip", {"-c"}, {
        feeder:[](int fd) {
            std::shared_ptr<CURL> curl(curl_easy_init(), curl_easy_cleanup);
            curl_easy_setopt(curl.get(), CURLOPT_URL, url);
            curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, +[](char *buffer, size_t size, size_t nmemb, void *pfd){
                write(*((int*)pfd), buffer, size * nmemb);
                return size * nmemb;
            });
            curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &fd);
            auto res = curl_easy_perform(curl.get());
            if (res != CURLE_OK) throw std::runtime_error(curl_easy_strerror(res));
            long http_code = 0;
            curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code != 200) throw std::runtime_error("Server error: status code=" + std::to_string(http_code));
        },
        receiver:[&doc](int fd) {
            doc = std::shared_ptr<xmlDoc>(xmlReadFd(fd, "", NULL, 0), xmlFreeDoc);
        }
    }));

    if (!doc) throw std::runtime_error("Document not parsed");
    //else
    for (auto child = xmlDocGetRootElement(doc.get())->children; child; child = child->next) {
        if (child->type != XML_ELEMENT_NODE || std::string("package") != (const char*)child->name) continue;
        for (auto grandchild = child->children; grandchild; grandchild = grandchild->next) {
            if (grandchild->type != XML_ELEMENT_NODE || std::string("name") != (const char*)grandchild->name) continue;
            std::cout << grandchild->children->content << std::endl;
        }
    }

    return 0;
}