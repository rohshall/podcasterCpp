#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <curl/curl.h>
#include <curl/easy.h>
#include <pugixml.hpp>
#include <toml++/toml.h>

namespace fs = std::filesystem;
using namespace std::string_view_literals;


static size_t write_data(void *contents, size_t size, size_t nmemb, void *userp)
{
    std::string s((char*)contents, size * nmemb);
    (*(std::ostream*)userp) << s;
    return size * nmemb;
}

class Downloader {
    private:
        CURL *curl_handle;

        void getUrlContents(const std::string &url, std::ostream *pOs) const {
            curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pOs);
            curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
            CURLcode res = curl_easy_perform(curl_handle);
            if (res != CURLE_OK) {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
                /* cleanup curl stuff */
                curl_easy_cleanup(curl_handle);
                throw std::runtime_error("curl_easy_perform() failed");
            }
        }
    public:
        Downloader() {
            curl_global_init(CURL_GLOBAL_ALL);
            /* init the curl session */
            curl_handle = curl_easy_init();
            if(!curl_handle) {
                std::cerr << "curl_easy_init() failed" << "\n";
                throw std::runtime_error("curl_easy_init() failed");
            }
        }

        virtual ~Downloader() {
            /* cleanup curl stuff */
            curl_easy_cleanup(curl_handle);
            curl_global_cleanup();
        }

        std::string getUrlContents(const std::string &url) const {
            std::stringstream ss;
            getUrlContents(url, &ss);
            return ss.str();
        }

        void downloadUrlContents(const std::string &url, const std::string &downloadPath) const {
            std::ofstream fileStream(downloadPath);
            if (!fileStream.is_open()) {
                std::cerr << "opening file for writing failed" << "\n";
                throw std::runtime_error("opening file for writing failed");
            }
            getUrlContents(url, &fileStream);
            fileStream.close();
        }
};


static std::optional<std::string> getLatestEpisodeUrl(const std::string &feed) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(feed.c_str());
    if (!result) {
        std::cerr << "Parsing of the rss feed failed: " << result << "\n";
        throw std::runtime_error("Parsing of the rss feed failed");
    }
    pugi::xml_node item = doc.first_element_by_path("//rss/channel/item");
    std::string title = item.child_value("title");
    pugi::xml_node enclosure = item.child("enclosure");
    std::string url = enclosure.attribute("url").value();

    return url;
}

class Config {
    private:
        const std::string media_dir;
        const std::map<std::string, std::string> podcasts;
        Config(const std::string& media_dir, const std::map<std::string, std::string> &podcasts)
            : media_dir(media_dir)
              , podcasts(podcasts) {
              }
        inline static const Config *pInstance;
    public:
        Config(Config &config) = delete;
        void operator=(const Config &config) = delete;

        std::pair<std::string, std::string> getPodcastInfo(const std::string &podcastId) const {
            std::string downloadUrl = podcasts.at(podcastId);
            std::string downloadDir = media_dir + "/" + podcastId;
            return std::make_pair(downloadUrl, downloadDir);
        }

        static const Config& instance() {
            if (pInstance == nullptr) {
                std::string home_dir = getenv("HOME");
                toml::table config;
                try {
                    config = toml::parse_file(home_dir + "/.podcasts.toml" );
                }
                catch (const toml::parse_error& err)
                {
                    std::cerr << "Parsing failed:\n" << err << "\n";
                    throw std::runtime_error("Parsing failed");
                }

                std::string media_dir = std::string(config["config"]["media_dir"].value_or(""sv));
                toml::table *podcasts_table = config["podcasts"].as_table();

                std::map<std::string, std::string> podcasts;
                for (auto&& [key, value] : *podcasts_table)
                {
                    std::string podKey = std::string(key);
                    std::string url = std::string(value.value_or(""sv));
                    podcasts.insert({podKey, url});
                }
                pInstance = new Config(media_dir, podcasts);
            }
            return *pInstance;
        }
};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "Usage <program name> <podcastKey>\n";
        exit(1);
    }
    const Config &config = Config::instance();

    std::pair<std::string, std::string> podcastInfo = config.getPodcastInfo(std::string(argv[1]));
    std::string podcastUrl = std::get<0>(podcastInfo);
    std::string downloadDir = std::get<1>(podcastInfo);
    const Downloader downloader;
    std::string feed = downloader.getUrlContents(podcastUrl);
    std::optional<std::string> episodeUrlOptional = getLatestEpisodeUrl(feed);
    if (episodeUrlOptional.has_value()) {
        std::string episodeUrl = episodeUrlOptional.value();
        fs::path episodePath = fs::path(episodeUrl);
        std::string fileName = episodePath.stem().string() + episodePath.extension().string();
        std::string downloadPath = downloadDir + "/" + fileName;
        std::cout << "Downloading " << fileName << " at " << downloadPath << "\n";
        downloader.downloadUrlContents(episodeUrl, downloadPath);
    } else {
        std::cerr << "Could not get the latest episode for" << argv[1] << "\n";
    }
}
