#include "gui/main_window.h"
#include "plugins/loader.h"
#include "../libstrtb/chat/system.h"
#include "../libstrtb/logging.h"
#include "../libstrtb/config/system.h"
#include "../libstrtb/version.h"
#include "../libstrtb/event/system.h"
#include "../libstrtb/uri.h"

#include <QApplication>
#include <QGuiApplication>
#include <cstdlib>
#include <string>
#include <QMessageBox>

using namespace strtb;

int main(int argc, char *argv[]) {
    char* home_path = getenv("HOME");

    // Init QApplication
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("Streaming Toolbox");
    QGuiApplication::setApplicationDisplayName("Streaming Toolbox");

    // Init logging system
    logging::add_output_stream(&std::clog, logging::INFO, logging::LINUX, true, logging::ANSI_ESCAPE_CODES);
    if (home_path != NULL)
        logging::add_output_file(std::string(home_path) + "/.local/share/streaming-toolbox/streaming-toolbox.log",
                                 logging::INFO, logging::LINUX, false, logging::NONE);
    logging::source log("Main", false);

    // Check libstrtb version
    if (!versions_equal(get_libstrtb_version(),
                        {.major=STRTB_SRC_VERSION_MAJOR, .minor=STRTB_SRC_VERSION_MINOR,
                         .patch=STRTB_SRC_VERSION_PATCH, .phase=STRTB_SRC_VERSION_PHASE})) {
        // Print to log
        log.put(logging::CRITICAL, {"Version mismatch between Streaming Toolbox (v",
                                    STRTB_SRC_VERSION_MAJOR, ".", STRTB_SRC_VERSION_MINOR, ".", STRTB_SRC_VERSION_PATCH, "-", STRTB_SRC_VERSION_PHASE,
                                    ") and libstrtb (", get_libstrtb_version_string(), ")."});
        // Show error message
        QMessageBox error_message_box;

        QString error_message = "Version mismatch between Streaming Toolbox (v";
        error_message.append(QString::number(STRTB_SRC_VERSION_MAJOR));
        error_message.append('.');
        error_message.append(QString::number(STRTB_SRC_VERSION_MINOR));
        error_message.append('.');
        error_message.append(QString::number(STRTB_SRC_VERSION_PATCH));
        error_message.append('-');
        error_message.append(STRTB_SRC_VERSION_PHASE);
        error_message.append(") and libstrtb (");
        error_message.append(get_libstrtb_version_string());
        error_message.append(").");
        error_message_box.setText(error_message);
        error_message_box.show();
        a.exec();
        // Close program
        return 1;
    }


    // Init config system
    std::filesystem::path config_path;
    if (home_path != NULL)
        config_path = std::filesystem::path(home_path) / ".config/streaming-toolbox";
    else
        config_path = std::filesystem::current_path() / "streaming-toolbox-config";
    config::system config_system(config_path);
    config::main = &config_system;

    // Init other things
    uri::known_tlds_load_str(uri::known_tlds_default, uri::known_tlds_default_length);
    event::system event_system;
    chat::system chat_system;
    chat::main = &chat_system;
    plugins::loader plugin_loader;

    // Load user plugins
    if (home_path != NULL)
        plugin_loader.load_plugins(std::string(home_path) + "/.local/share/streaming-toolbox/plugins");

    // Start GUI
    gui::main_window w(&plugin_loader);
    w.show();
    return a.exec();
}
