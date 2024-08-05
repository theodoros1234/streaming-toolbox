#include "plugin.h"
#include "link.h"

#include <filesystem>
#include <stdexcept>
#include <dlfcn.h>

namespace fs = std::filesystem;

namespace plugins {

chat::ChatInterface* Plugin::chat_if;

Plugin::Plugin(fs::path path) : log_if("Plugin Interface"), log_pl("Plugin (not initialized)") {
    this->path = path;

    // Load the plugin
    this->log_if.put(logging::INFO, {"Loading ", path.filename()});
    this->library = dlopen(path.c_str(), RTLD_NOW);
    if (this->library == NULL) {
        // Plugin wasn't loaded
        throw std::runtime_error(dlerror());
    }

    // Check API version
    void *ptr_get_api_version = dlsym(this->library, "plugin_get_api_version");
    void *ptr_send_api_version = dlsym(this->library, "plugin_send_api_version");
    if (!ptr_get_api_version || !ptr_send_api_version) {
        // API check functions are missing from the plugin
        this->log_if.put(logging::ERROR, {"Failed loading ", path.filename(), ": Couldn't check API version due to missing functions"});
        dlclose(this->library);
        throw std::runtime_error("Couldn't check API version due to missing functions");
    }
    this->functions.get_api_version = reinterpret_cast<int (*)()>(ptr_get_api_version);
    this->functions.send_api_version = reinterpret_cast<bool (*)(int)>(ptr_send_api_version);
    this->api_version = this->functions.get_api_version();
    if (this->api_version < STRTB_PLUGIN_API_VERSION) {
        // Streaming Toolbox doesn't support plugin's API version
        this->log_if.put(logging::ERROR, {"Failed loading ", path.filename(), ": Plugin's API version not supported by Streaming Toolbox"});
        dlclose(this->library);
        throw std::runtime_error("Plugin's API version not supported by Streaming Toolbox");
    }
    if (!this->functions.send_api_version(STRTB_PLUGIN_API_VERSION)) {
        // Plugin doesn't support plugin's API version
        this->log_if.put(logging::ERROR, {"Failed loading ", path.filename(), ": Streaming Toolbox's API version not supported by plugin"});
        dlclose(this->library);
        throw std::runtime_error("Streaming Toolbox's API version not supported by plugin");
    }

    // Get needed functions from plugin
    void *ptr_exchange_info = dlsym(this->library, "plugin_exchange_info");
    void *ptr_activate = dlsym(this->library, "plugin_activate");
    void *ptr_deactivate = dlsym(this->library, "plugin_deactivate");

    if (!ptr_exchange_info || !ptr_activate || !ptr_deactivate) {
        // Some required functions are missing from plugin
        this->log_if.put(logging::ERROR, {"Failed loading ", path.filename(), ": Missing functions"});
        dlclose(this->library);
        throw std::runtime_error("Missing functions");
    }

    this->functions.exchange_info = reinterpret_cast<strtb_plugin::plugin_info_t (*)(strtb_plugin::plugin_interface_t, strtb_plugin::plugin_instance_t)>(ptr_exchange_info);
    this->functions.activate = reinterpret_cast<int (*)()>(ptr_activate);
    this->functions.deactivate = reinterpret_cast<void (*)()>(ptr_deactivate);

    // Exchange basic info with plugin
    this->info = this->functions.exchange_info(Plugin::plugin_interface, {.ptr=this});
    this->log_if = logging::LogSource("Plugin Interface: " + this->info.name);
    this->log_pl = logging::LogSource("Plugin: " + this->info.name);
    this->log_if.put(logging::DEBUG, {"Loaded"});
    this->activate();
}

Plugin::~Plugin() {
    // Unload plugin
    this->log_if.put(logging::DEBUG, {"Deactivating and unloading"});
    this->functions.deactivate();
    dlclose(this->library);

    // Clean up any objects abandoned by the plugin
    this->log_if.put(logging::DEBUG, {"Checking for abandoned objects"});
    // Chat subscriptions
    {
        std::lock_guard<std::mutex> guard(this->chat_subscriptions_lock);
        for (auto sub:this->chat_subscriptions) {
            this->log_if.put(logging::WARNING, {"Cleaning up abandoned chat subscription '", sub->getProviderId(), ":", sub->getChannelId(), "'"});
            sub->unsubscribe();
            delete sub;
        }
    }
    // Chat channels
    {
        std::lock_guard<std::mutex> guard(this->chat_channels_lock);
        for (auto channel:this->chat_channels) {
            this->log_if.put(logging::WARNING, {"Cleaning up abandoned chat channel '", channel->getProviderId(), ":", channel->getId(), "'"});
            delete channel;
        }
    }
    // Chat providers
    {
        std::lock_guard<std::mutex> guard(this->chat_providers_lock);
        for (auto provider:this->chat_providers) {
            this->log_if.put(logging::WARNING, {"Cleaning up abandoned chat provider '", provider->getId(), "'"});
            delete provider;
        }
    }
}

void Plugin::activate() {
    if (this->functions.activate()) {
        this->log_if.put(logging::ERROR, {"Couldn't activate"});
        throw std::runtime_error("Couldn't activate");
    }
}

std::string Plugin::getName() {
    return this->info.name;
}

std::string Plugin::getVersion() {
    return this->info.version;
}

std::string Plugin::getAuthor() {
    return this->info.author;
}

std::string Plugin::getDescription() {
    return this->info.description;
}

std::string Plugin::getAccentColor() {
    return this->info.accent_color;
}

std::string Plugin::getWebsite() {
    return this->info.website;
}

std::string Plugin::getCopyright() {
    return this->info.copyright;
}

std::string Plugin::getLicense() {
    return this->info.license;
}

std::filesystem::path Plugin::getPath() {
    return this->path;
}

QWidget* Plugin::getSettingsPage() {
    return this->info.settings_page;
}

int Plugin::getAPIVersion() {
    return this->api_version;
}

plugin_basic_info_t Plugin::getInfo() {
    return plugin_basic_info_t {
        .name = info.name,
        .version = info.version,
        .author = info.author,
        .description = info.description,
        .accent_color = info.accent_color,
        .website = info.website,
        .copyright = info.copyright,
        .license = info.license,
        .path = path,
        .api_version = api_version
    };
}

void Plugin::setInterfaces(chat::ChatInterface *chat_if) {
    Plugin::chat_if = chat_if;
}

}

/********** Plugin interface functions **********/
namespace strtb_plugin {

logging::LogSource global_log("Plugin Interface");

plugins::Plugin* convert(plugin_instance_t in) {
    return reinterpret_cast<plugins::Plugin*>(in.ptr);
}

/***** Chat *****/

// Coversions bewteen internal types and plugin types

chat::ChatMessage convert(chat_message_t &in) {
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

chat_message_t convert(chat::ChatMessage &in) {
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

chat_channel_info_t convert(chat::ChatChannelInfo &in) {
    return {.id = in.id, .name = in.name};
}

chat_provider_info_t convert(chat::ChatProviderInfo &in) {
    std::vector<chat_channel_info_t> channels;
    channels.reserve(in.channels.size());
    for (auto c:in.channels)
        channels.push_back(convert(c));
    return {.id=in.id, .name=in.name, .channel_count=in.channel_count, .channels=channels};
}

chat_if_channel_info_t convert(chat::ChatInterfaceChannelInfo &in) {
    std::vector<chat_provider_info_t> providers;
    providers.reserve(in.providers.size());
    for (auto p:in.providers)
        providers.push_back(convert(p));
    return {.provider_count=in.provider_count, .channel_count=in.channel_count, .providers=providers};
}

chat::ChatProvider* convert(chat_provider_t in) {
    return reinterpret_cast<chat::ChatProvider*>(in.ptr);
}

chat::ChatChannel* convert(chat_channel_t in) {
    return reinterpret_cast<chat::ChatChannel*>(in.ptr);
}

chat::ChatSubscription* convert(chat_subscription_t in) {
    return reinterpret_cast<chat::ChatSubscription*>(in.ptr);
}

// Chat interface

chat_if_channel_info_t chat_if_get_channel_info() {
    chat::ChatInterfaceChannelInfo info = plugins::Plugin::chat_if->getChannelInfo();
    return convert(info);
}

chat_provider_t chat_if_register_provider(plugin_instance_t plugin, std::string id, std::string name) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return {};
    }
    plugins::Plugin *pl = convert(plugin);
    try {
        chat::ChatProvider *provider = plugins::Plugin::chat_if->registerProvider(id, name);
        {
            std::lock_guard<std::mutex> guard(pl->chat_providers_lock);
            pl->chat_providers.insert(provider);
        }
        return {provider};
    } catch (std::exception &e) {
        pl->log_if.put(logging::ERROR, {"Exception in ", __func__, ": ", e.what()});
        return {nullptr};
    }
}

chat_subscription_t chat_if_subscribe(plugin_instance_t plugin, std::string provider_id, std::string channel_id) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return {};
    }
    plugins::Plugin *pl = convert(plugin);
    try {
        chat::ChatSubscription *subscription = plugins::Plugin::chat_if->subscribe(provider_id, channel_id);
        {
            std::lock_guard<std::mutex> guard(pl->chat_subscriptions_lock);
            pl->chat_subscriptions.insert(subscription);
        }
        return {subscription};
    } catch (std::exception &e) {
        pl->log_if.put(logging::ERROR, {"Exception in ", __func__, ": ", e.what()});
        return {nullptr};
    }
}

// Chat provider

std::string chat_provider_get_id(chat_provider_t provider) {
    if (!provider.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Provider pointer is null"});
        return "";
    }
    return convert(provider)->getId();
}

std::string chat_provider_get_name(chat_provider_t provider) {
    if (!provider.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Provider pointer is null"});
        return "";
    }
    return convert(provider)->getName();
}

chat_provider_info_t chat_provider_get_info(chat_provider_t provider) {
    if (!provider.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Provider pointer is null"});
        return {};
    }
    chat::ChatProviderInfo info = convert(provider)->getInfo();
    return convert(info);
}

chat_channel_t chat_provider_register_channel(plugin_instance_t plugin, chat_provider_t provider, std::string id, std::string name) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return {};
    }
    plugins::Plugin *pl = convert(plugin);
    if (!provider.ptr) {
        pl->log_if.put(logging::ERROR, {__func__, ": Provider pointer is null"});
        return {};
    }
    try {
        chat::ChatChannel *channel = convert(provider)->registerChannel(id, name);
        {
            std::lock_guard<std::mutex> guard(pl->chat_channels_lock);
            pl->chat_channels.insert(channel);
        }
        return {channel};
    } catch (std::exception &e) {
        pl->log_if.put(logging::ERROR, {"Exception in ", __func__, ": ", e.what()});
        return {nullptr};
    }
}

void chat_provider_delete(plugin_instance_t plugin, chat_provider_t *provider) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return;
    }
    plugins::Plugin *pl = convert(plugin);
    if (!provider) {
        pl->log_if.put(logging::ERROR, {__func__, ": Provider argument is null"});
        return;
    }
    if (!provider->ptr) {
        pl->log_if.put(logging::ERROR, {__func__, ": Provider pointer is null"});
        return;
    }
    chat::ChatProvider *provider_c = convert(*provider);
    int result;
    {
        std::lock_guard<std::mutex> guard(pl->chat_providers_lock);
        result = pl->chat_providers.erase(provider_c);
    }
    if (result)
        delete provider_c;
    else
        pl->log_if.put(logging::ERROR, {__func__, ": Tried to delete provider not owned by this plugin"});
    provider->ptr = nullptr;
}

// Chat channel

std::string chat_channel_get_id(chat_channel_t channel) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return "";
    }
    return convert(channel)->getId();
}

std::string chat_channel_get_name(chat_channel_t channel) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return "";
    }
    return convert(channel)->getName();
}

std::string chat_channel_get_provider_id(chat_channel_t channel) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return "";
    }
    return convert(channel)->getProviderId();
}

std::string chat_channel_get_provider_name(chat_channel_t channel) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return "";
    }
    return convert(channel)->getProviderName();
}

chat_channel_info_t chat_channel_get_info(chat_channel_t channel) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return {};
    }
    chat::ChatChannelInfo info = convert(channel)->getInfo();
    return convert(info);
}

void chat_channel_push_one(chat_channel_t channel, chat_message_t *message) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return;
    }
    if (!message) {
        global_log.put(logging::ERROR, {__func__, ": Message argument is null"});
        return;
    }
    chat::ChatMessage msg_converted = convert(*message);
    convert(channel)->push(msg_converted);
}

void chat_channel_push_many(chat_channel_t channel, std::vector<chat_message_t> *messages) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return;
    }
    if (!messages) {
        global_log.put(logging::ERROR, {__func__, ": Messages argument is null"});
        return;
    }
    std::vector<chat::ChatMessage> messages_converted;
    messages_converted.reserve(messages->size());
    for (auto &msg:*messages)
        messages_converted.push_back(convert(msg));
    convert(channel)->push(messages_converted);
}

void chat_channel_delete(plugin_instance_t plugin, chat_channel_t *channel) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return;
    }
    plugins::Plugin *pl = convert(plugin);
    if (!channel) {
        pl->log_if.put(logging::ERROR, {__func__, ": Channel argument is null"});
        return;
    }
    if (!channel->ptr) {
        pl->log_if.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return;
    }
    chat::ChatChannel *channel_c = convert(*channel);
    int result;
    {
        std::lock_guard<std::mutex> guard(pl->chat_channels_lock);
        result = pl->chat_channels.erase(channel_c);
    }
    if (result)
        delete convert(*channel);
    else
        pl->log_if.put(logging::ERROR, {__func__, ": Tried to delete channel not owned by this plugin"});
    channel->ptr = nullptr;
}

// Chat subscription

std::string chat_subscription_get_provider_id(chat_subscription_t sub) {
    if (!sub.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Subscription pointer is null"});
        return "";
    }
    return convert(sub)->getProviderId();
}

std::string chat_subscription_get_channel_id(chat_subscription_t sub) {
    if (!sub.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Subscription pointer is null"});
        return "";
    }
    return convert(sub)->getChannelId();
}

std::vector<chat_message_t> chat_subscription_pull(chat_subscription_t sub) {
    if (!sub.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Subscription pointer is null"});
        return {};
    }
    std::vector<chat::ChatMessage> messages = convert(sub)->pull();
    std::vector<chat_message_t> messages_converted;
    messages_converted.reserve(messages.size());
    for (auto msg:messages)
        messages_converted.push_back(convert(msg));
    return messages_converted;
}

void chat_subscription_unsubscribe(chat_subscription_t sub) {
    if (!sub.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Subscription pointer is null"});
        return;
    }
    convert(sub)->unsubscribe();
}

void chat_subscription_delete(plugin_instance_t plugin, chat_subscription_t *sub) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return;
    }
    plugins::Plugin *pl = convert(plugin);
    if (!sub) {
        pl->log_if.put(logging::ERROR, {__func__, ": Subscription argument is null"});
        return;
    }
    if (!sub->ptr) {
        pl->log_if.put(logging::ERROR, {__func__, ": Subscription pointer is null"});
        return;
    }
    chat::ChatSubscription *sub_c = convert(*sub);
    int result;
    {
        std::lock_guard<std::mutex> guard(pl->chat_subscriptions_lock);
        result = pl->chat_subscriptions.erase(sub_c);
    }
    if (result)
        delete convert(*sub);
    else
        pl->log_if.put(logging::ERROR, {__func__, ": Tried to delete subscription not owned by this plugin"});
    sub->ptr = nullptr;
}

/***** Logging *****/

void log_put(plugin_instance_t plugin, log_level level, std::vector<std::string> message) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return;
    }
    std::vector<logging::LogMessagePart> message_cnv;
    message_cnv.reserve(message.size());
    for (auto &msg:message)
        message_cnv.emplace_back(msg);
    convert(plugin)->log_pl.put((logging::level)level, message_cnv);
}

}
/*******************/

strtb_plugin::plugin_interface_t plugins::Plugin::plugin_interface = {
    // Chat Interface
    .chat_if_get_channel_info = strtb_plugin::chat_if_get_channel_info,
    .chat_if_register_provider = strtb_plugin::chat_if_register_provider,
    .chat_if_subscribe = strtb_plugin::chat_if_subscribe,
    // Chat Provider
    .chat_provider_get_id = strtb_plugin::chat_provider_get_id,
    .chat_provider_get_name = strtb_plugin::chat_provider_get_name,
    .chat_provider_get_info = strtb_plugin::chat_provider_get_info,
    .chat_provider_register_channel = strtb_plugin::chat_provider_register_channel,
    .chat_provider_delete = strtb_plugin::chat_provider_delete,
    // Chat Channel
    .chat_channel_get_id = strtb_plugin::chat_channel_get_id,
    .chat_channel_get_name = strtb_plugin::chat_channel_get_name,
    .chat_channel_get_provider_id = strtb_plugin::chat_channel_get_provider_id,
    .chat_channel_get_provider_name = strtb_plugin::chat_channel_get_provider_name,
    .chat_channel_get_info = strtb_plugin::chat_channel_get_info,
    .chat_channel_push_one = strtb_plugin::chat_channel_push_one,
    .chat_channel_push_many = strtb_plugin::chat_channel_push_many,
    .chat_channel_delete = strtb_plugin::chat_channel_delete,
    // Chat Subscription
    .chat_subscription_get_provider_id = strtb_plugin::chat_subscription_get_provider_id,
    .chat_subscription_get_channel_id = strtb_plugin::chat_subscription_get_channel_id,
    .chat_subscription_pull = strtb_plugin::chat_subscription_pull,
    .chat_subscription_unsubscribe = strtb_plugin::chat_subscription_unsubscribe,
    .chat_subscription_delete = strtb_plugin::chat_subscription_delete,
    // Logging
    .log_put = strtb_plugin::log_put
};


