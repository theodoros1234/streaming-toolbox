#include "mainwindow.h"
#include "pluginloader.h"
#include "chatsystem.h"

#include <QApplication>
#include <cstdlib>
#include <string>
#include <iostream>

void test(ChatInterface *chat_if) {
    ChatProvider *provider = chat_if->registerProvider("test_prv", "Test Provider");
    ChatChannel *channel = provider->registerChannel("test_ch", "Test Channel");
    std::cout << "Enter message: ";
    ChatMessage msg;
    msg.user_name = "theodoros_1234";
    while (std::cin >> msg.message) {
        std::cout << "Sending message" << std::endl;
        channel->push(msg);
        std::cout << "Enter message: ";
    }
    std::cout << "Stopping message sending thread" << std::endl;
    delete channel;
    delete provider;
}

void test_sub(ChatInterface *chat_if) {
    ChatSubscription *sub = chat_if->subscribe("test_prv", "test_ch");
    bool listen = true;
    while (listen) {
        std::vector<ChatMessage> messages = sub->pull();
        for (auto msg:messages) {
            std::cout << "[Test Sub] " << msg.user_name << ": " << msg.message << std::endl;
            if (msg.message == "unsub")
                listen = false;
        }
    }
    std::cout << "[Test Sub] Unsubbing" << std::endl;
    sub->unsubscribe();
    std::cout << "[Test Sub] Deleting sub" << std::endl;
    delete sub;
}

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    PluginLoader plugin_loader;
    ChatSystem chat_system;


    // Load user plugins
    char* home_path = getenv("HOME");
    if (home_path != NULL)
        plugin_loader.loadPlugins(std::string(home_path) + "/.local/share/streaming-toolbox/plugins");
    plugin_loader.activatePlugins();

    // Chat test
    std::thread t(test, (ChatInterface*) &chat_system);
    test_sub((ChatInterface*) &chat_system);
    t.join();

    // Start GUI
    w.getPlugins(&plugin_loader);
    w.show();
    return a.exec();
}
