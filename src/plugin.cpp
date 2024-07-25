#include "plugin.h"
#include "pluginlink.h"
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <dlfcn.h>

namespace fs = std::filesystem;
ChatInterface* Plugin::chat_if;

Plugin::Plugin(fs::path path) {
    this->path = path;

    // Load the plugin
    std::cout << "Loading plugin at " << path << std::endl;
    this->library = dlopen(path.c_str(), RTLD_NOW);
    if (this->library == NULL)
        // Plugin wasn't loaded
        throw std::runtime_error(dlerror());

    // Get needed functions from plugin
    void *ptr_exchange_info = dlsym(this->library, "plugin_exchange_info");
    void *ptr_activate = dlsym(this->library, "plugin_activate");
    void *ptr_deactivate = dlsym(this->library, "plugin_deactivate");

    if (!ptr_exchange_info || !ptr_activate || !ptr_deactivate)
        // Some required functions are missing from plugin
        throw std::runtime_error("Missing functions");

    this->functions.exchange_info = reinterpret_cast<plugin_info_t* (*)(plugin_interface_t*)>(ptr_exchange_info);
    this->functions.activate = reinterpret_cast<int (*)()>(ptr_activate);
    this->functions.deactivate = reinterpret_cast<void (*)()>(ptr_deactivate);

    // Exchange basic info with plugin
    this->info = this->functions.exchange_info(&Plugin::plugin_interface);
}

Plugin::~Plugin() {
    this->functions.deactivate();
    dlclose(this->library);
}

void Plugin::activate() {
    if (this->functions.activate())
        throw std::runtime_error("Couldn't activate");
}

std::string Plugin::getName() {
    return this->info->name;
}

std::string Plugin::getVersion() {
    return this->info->version;
}

std::string Plugin::getAuthor() {
    return this->info->author;
}

std::string Plugin::getDescription() {
    return this->info->description;
}

std::string Plugin::getAccentColor() {
    return this->info->accent_color;
}

std::string Plugin::getWebsite() {
    return this->info->website;
}

std::string Plugin::getCopyright() {
    return this->info->copyright;
}

std::string Plugin::getLicense() {
    return this->info->license;
}

std::filesystem::path Plugin::getPath() {
    return this->path;
}

QWidget* Plugin::getSettingsPage() {
    return this->info->settings_page;
}

plugin_basic_info_t Plugin::getInfo() {
    return plugin_basic_info_t {
        .name = info->name,
        .version = info->version,
        .author = info->author,
        .description = info->description,
        .accent_color = info->accent_color,
        .website = info->website,
        .copyright = info->copyright,
        .license = info->license,
        .path = path
    };
}

void Plugin::setInterfaces(ChatInterface *chat_if) {
    Plugin::chat_if = chat_if;
}

/********** Plugin interface functions **********/

/***** Chat *****/

// Coversions bewteen internal types and plugin types

ChatMessage convert(chat_message_t &in) {
    return {
        .provider_id = in.provider_id,
        .provider_name = in.provider_name,
        .channel_id = in.channel_id,
        .channel_name = in.channel_name,
        .user_id = in.user_id,
        .user_name = in.user_name,
        .user_color = in.user_color,
        .message = in.message,
        .timestamp = in.timestamp,
        .more_metadata = in.more_metadata
    };
}

chat_message_t convert(ChatMessage &in) {
    return {
        .provider_id = in.provider_id,
        .provider_name = in.provider_name,
        .channel_id = in.channel_id,
        .channel_name = in.channel_name,
        .user_id = in.user_id,
        .user_name = in.user_name,
        .user_color = in.user_color,
        .message = in.message,
        .timestamp = in.timestamp,
        .more_metadata = in.more_metadata
    };
}

chat_channel_info_t convert(ChatChannelInfo &in) {
    return {.id = in.id, .name = in.name};
}

chat_provider_info_t convert(ChatProviderInfo &in) {
    std::vector<chat_channel_info_t> channels;
    channels.reserve(in.channels.size());
    for (auto c:in.channels)
        channels.push_back(convert(c));
    return {.id=in.id, .name=in.name, .channel_count=in.channel_count, .channels=channels};
}

chat_if_channel_info_t convert(ChatInterfaceChannelInfo &in) {
    std::vector<chat_provider_info_t> providers;
    providers.reserve(in.providers.size());
    for (auto p:in.providers)
        providers.push_back(convert(p));
    return {.provider_count=in.provider_count, .channel_count=in.channel_count, .providers=providers};
}

ChatProvider* convert(chat_provider_t *in) {
    return reinterpret_cast<ChatProvider*>(in->ptr);
}

ChatChannel* convert(chat_channel_t *in) {
    return reinterpret_cast<ChatChannel*>(in->ptr);
}

ChatSubscription* convert(chat_subscription_t *in) {
    return reinterpret_cast<ChatSubscription*>(in->ptr);
}

// Chat Interface

chat_if_channel_info_t chat_if_get_channel_info() {
    ChatInterfaceChannelInfo info = Plugin::chat_if->getChannelInfo();
    return convert(info);
}

chat_provider_t chat_if_register_provider(std::string id, std::string name) {
    try {
        return {Plugin::chat_if->registerProvider(id, name)};
    } catch (std::exception &e) {
        std::cout << "Error: Could not register chat provider due to exception: " << e.what() << std::endl;
        return {nullptr};
    }
}

chat_subscription_t chat_if_subscribe(std::string provider_id, std::string channel_id) {
    try {
        return {Plugin::chat_if->subscribe(provider_id, channel_id)};
    } catch (std::exception &e) {
        std::cout << "Error: Could not subscribe to chat due to exception: " << e.what() << std::endl;
        return {nullptr};
    }
}

// Chat Provider

std::string chat_provider_get_id(chat_provider_t *provider) {
    return convert(provider)->getId();
}

std::string chat_provider_get_name(chat_provider_t *provider) {
    return convert(provider)->getName();
}

chat_provider_info_t chat_provider_get_info(chat_provider_t *provider) {
    ChatProviderInfo info = convert(provider)->getInfo();
    return convert(info);
}

chat_channel_t chat_provider_register_channel(chat_provider_t *provider, std::string id, std::string name) {
    try {
        return {convert(provider)->registerChannel(id, name)};
    } catch (std::exception &e) {
        std::cout << "Error: Could not register chat channel due to exception: " << e.what() << std::endl;
        return {nullptr};
    }
}

void chat_provider_delete(chat_provider_t *provider) {
    delete convert(provider);
    provider->ptr = nullptr;
}

std::string chat_channel_get_id(chat_channel_t *channel) {
    return convert(channel)->getId();
}

std::string chat_channel_get_name(chat_channel_t *channel) {
    return convert(channel)->getName();
}

std::string chat_channel_get_provider_id(chat_channel_t *channel) {
    return convert(channel)->getProviderId();
}

std::string chat_channel_get_provider_name(chat_channel_t *channel) {
    return convert(channel)->getProviderName();
}

chat_channel_info_t chat_channel_get_info(chat_channel_t *channel) {
    ChatChannelInfo info = convert(channel)->getInfo();
    return convert(info);
}

void chat_channel_push_one(chat_channel_t *channel, chat_message_t *message) {
    ChatMessage msg_converted = convert(*message);
    convert(channel)->push(msg_converted);
}

void chat_channel_push_many(chat_channel_t *channel, std::vector<chat_message_t> *messages) {
    std::vector<ChatMessage> messages_converted;
    messages_converted.reserve(messages->size());
    for (auto msg:*messages)
        messages_converted.push_back(convert(msg));
    convert(channel)->push(messages_converted);
}

void chat_channel_delete(chat_channel_t *channel) {
    delete convert(channel);
    channel->ptr = nullptr;
}

std::string chat_subscription_get_provider_id(chat_subscription_t *sub) {
    return convert(sub)->getProviderId();
}

std::string chat_subscription_get_channel_id(chat_subscription_t *sub) {
    return convert(sub)->getChannelId();
}

std::vector<chat_message_t> chat_subscription_pull(chat_subscription_t *sub) {
    std::vector<ChatMessage> messages = convert(sub)->pull();
    std::vector<chat_message_t> messages_converted;
    messages_converted.reserve(messages.size());
    for (auto msg:messages)
        messages_converted.push_back(convert(msg));
    return messages_converted;
}

void chat_subscription_unsubscribe(chat_subscription_t *sub) {
    convert(sub)->unsubscribe();
}

void chat_subscription_delete(chat_subscription_t *sub) {
    delete convert(sub);
    sub->ptr = nullptr;
}

plugin_interface_t Plugin::plugin_interface = {
    // Chat Interface
    .chat_if_get_channel_info = chat_if_get_channel_info,
    .chat_if_register_provider = chat_if_register_provider,
    .chat_if_subscribe = chat_if_subscribe,
    // Chat Provider
    .chat_provider_get_id = chat_provider_get_id,
    .chat_provider_get_name = chat_provider_get_name,
    .chat_provider_get_info = chat_provider_get_info,
    .chat_provider_register_channel = chat_provider_register_channel,
    .chat_provider_delete = chat_provider_delete,
    // Chat Channel
    .chat_channel_get_id = chat_channel_get_id,
    .chat_channel_get_name = chat_channel_get_name,
    .chat_channel_get_provider_id = chat_channel_get_provider_id,
    .chat_channel_get_provider_name = chat_channel_get_provider_name,
    .chat_channel_get_info = chat_channel_get_info,
    .chat_channel_push_one = chat_channel_push_one,
    .chat_channel_push_many = chat_channel_push_many,
    .chat_channel_delete = chat_channel_delete,
    // Chat Subscription
    .chat_subscription_get_provider_id = chat_subscription_get_provider_id,
    .chat_subscription_get_channel_id = chat_subscription_get_channel_id,
    .chat_subscription_pull = chat_subscription_pull,
    .chat_subscription_unsubscribe = chat_subscription_unsubscribe,
    .chat_subscription_delete = chat_subscription_delete
};
