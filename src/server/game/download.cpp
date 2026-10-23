// npps4/game/download.py. The game data is complete on the device, so this is the "none" download backend
// (npps4/download/none.py) without any downloadable packages.
#include "app/idol.hpp"

namespace npps::game::download {

static DownloadUpdateResponse download_update(Context &, const DownloadUpdateRequest &request) {
    try {
        auto install_version = util::parse_sif_version(request.install_version);
        auto external_version = util::parse_sif_version(request.external_version);
        (void)std::min(external_version, install_version);
    } catch (const std::exception &e) {
        throw error::IdolError(200, 720, e.what());
    }
    // No update files and no injected server info.
    return {};
}
NPPS_ENDPOINT("download", "update", Auth, download_update, .check_version = false, .batchable = false);

static DownloadCommonResponse download_batch(Context &, const DownloadBatchRequest &) { return {}; }
NPPS_ENDPOINT("download", "batch", Auth, download_batch, .check_version = false, .batchable = false);

static DownloadCommonResponse download_event(Context &, const DownloadBatchRequest &) {
    // TODO
    return {};
}
NPPS_ENDPOINT("download", "event", Auth, download_event, .check_version = false, .batchable = false);

static DownloadCommonResponse download_additional(Context &, const DownloadAdditionalRequest &) { return {}; }
NPPS_ENDPOINT("download", "additional", Auth, download_additional, .check_version = false, .batchable = false);

static DownloadGetUrlResponse download_geturl(Context &context, const DownloadGetUrlRequest &request) {
    // download/none.py get_raw_files: every file points to "<request url>/missing"
    std::string host = "localhost";
    if (context.request) {
        if (const std::string *h = context.request->header("host")) host = *h;
    }
    std::string target = "http://" + host + (context.request ? context.request->target : std::string());
    target = (!target.empty() && target.back() == '/') ? target + "missing" : target + "/missing";
    return DownloadGetUrlResponse::make(std::vector<std::string>(request.path_list.size(), target));
}
NPPS_ENDPOINT("download", "getUrl", Auth, download_geturl, .check_version = false, .batchable = false);

}  // namespace npps::game::download
