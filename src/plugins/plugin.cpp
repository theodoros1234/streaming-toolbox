#include "plugin.h"
#include "link.h"

#include <filesystem>
#include <stdexcept>
#include <dlfcn.h>

namespace fs = std::filesystem;

namespace plugins {

chat::interface* plugin::chat_if;

plugin::plugin(fs::path path) : log_if("Plugin Interface"), log_pl("Plugin (not initialized)") {
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

    this->functions.exchange_info = reinterpret_cast<strtb_plugin::plugin_info (*)(strtb_plugin::plugin_interface, strtb_plugin::plugin_instance)>(ptr_exchange_info);
    this->functions.activate = reinterpret_cast<int (*)()>(ptr_activate);
    this->functions.deactivate = reinterpret_cast<void (*)()>(ptr_deactivate);

    // Exchange basic info with plugin
    this->info = this->functions.exchange_info(plugin::plugin_interface, {.ptr=this});
    this->log_if = logging::source("Plugin Interface: " + this->info.name);
    this->log_pl = logging::source("Plugin: " + this->info.name);
    this->log_if.put(logging::DEBUG, {"Loaded"});
    this->activate();
}

plugin::~plugin() {
    // Unload plugin
    this->log_if.put(logging::DEBUG, {"Deactivating and unloading"});
    this->functions.deactivate();
    dlclose(this->library);

    // Clean up any objects abandoned by the plugin
    this->log_if.put(logging::DEBUG, {"Checking for abandoned objects"});
    // Chat subscriptions
    {
        std::lock_guard<std::mutex> guard(this->chat_subscriptions_lock);
        for (auto sub : this->chat_subscriptions) {
            this->log_if.put(logging::WARNING, {"Cleaning up abandoned chat subscription '", sub->get_provider_id(), ":", sub->get_channel_id(), "'"});
            sub->unsubscribe();
            delete sub;
        }
    }
    // Chat channels
    {
        std::lock_guard<std::mutex> guard(this->chat_channels_lock);
        for (auto channel : this->chat_channels) {
            this->log_if.put(logging::WARNING, {"Cleaning up abandoned chat channel '", channel->get_provider_id(), ":", channel->get_id(), "'"});
            delete channel;
        }
    }
    // Chat providers
    {
        std::lock_guard<std::mutex> guard(this->chat_providers_lock);
        for (auto provider : this->chat_providers) {
            this->log_if.put(logging::WARNING, {"Cleaning up abandoned chat provider '", provider->get_id(), "'"});
            delete provider;
        }
    }
}

void plugin::activate() {
    if (this->functions.activate()) {
        this->log_if.put(logging::ERROR, {"Couldn't activate"});
        throw std::runtime_error("Couldn't activate");
    }
}

std::string plugin::get_name() {
    return this->info.name;
}

std::string plugin::get_version() {
    return this->info.version;
}

std::string plugin::get_author() {
    return this->info.author;
}

std::string plugin::get_description() {
    return this->info.description;
}

std::string plugin::get_accent_color() {
    return this->info.accent_color;
}

std::string plugin::get_website() {
    return this->info.website;
}

std::string plugin::get_copyright() {
    return this->info.copyright;
}

std::string plugin::get_license() {
    return this->info.license;
}

std::filesystem::path plugin::get_path() {
    return this->path;
}

QWidget* plugin::get_settings_page() {
    return this->info.settings_page;
}

int plugin::get_api_version() {
    return this->api_version;
}

plugin_basic_info plugin::get_info() {
    return plugin_basic_info {
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

void plugin::set_interfaces(chat::interface *chat_if) {
    plugin::chat_if = chat_if;
}

}

/********** plugin interface functions **********/
namespace strtb_plugin {

logging::source global_log("Plugin Interface");

plugins::plugin* convert(plugin_instance in) {
    return reinterpret_cast<plugins::plugin*>(in.ptr);
}

/***** Chat *****/

// Coversions bewteen internal types and plugin types

chat::message convert(chat_message &in) {
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

chat_message convert(chat::message &in) {
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

chat_channel_info convert(chat::channel_info &in) {
    return {.id = in.id, .name = in.name};
}

chat_provider_info convert(chat::provider_info &in) {
    std::vector<chat_channel_info> channels;
    channels.reserve(in.channels.size());
    for (auto c : in.channels)
        channels.push_back(convert(c));
    return {.id=in.id, .name=in.name, .channel_count=in.channel_count, .channels=channels};
}

chat_if_channel_info convert(chat::interface_channel_info &in) {
    std::vector<chat_provider_info> providers;
    providers.reserve(in.providers.size());
    for (auto p : in.providers)
        providers.push_back(convert(p));
    return {.provider_count=in.provider_count, .channel_count=in.channel_count, .providers=providers};
}

chat::provider* convert(chat_provider in) {
    return reinterpret_cast<chat::provider*>(in.ptr);
}

chat::channel* convert(chat_channel in) {
    return reinterpret_cast<chat::channel*>(in.ptr);
}

chat::subscription* convert(chat_subscription in) {
    return reinterpret_cast<chat::subscription*>(in.ptr);
}

// Chat interface

chat_if_channel_info chat_if_get_channel_info() {
    chat::interface_channel_info info = plugins::plugin::chat_if->get_channel_info();
    return convert(info);
}

chat_provider chat_if_register_provider(plugin_instance plugin, std::string id, std::string name) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return {};
    }
    plugins::plugin *pl = convert(plugin);
    try {
        chat::provider *provider = plugins::plugin::chat_if->register_provider(id, name);
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

chat_subscription chat_if_subscribe(plugin_instance plugin, std::string provider_id, std::string channel_id) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return {};
    }
    plugins::plugin *pl = convert(plugin);
    try {
        chat::subscription *subscription = plugins::plugin::chat_if->subscribe(provider_id, channel_id);
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

std::string chat_provider_get_id(chat_provider provider) {
    if (!provider.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Provider pointer is null"});
        return "";
    }
    return convert(provider)->get_id();
}

std::string chat_provider_get_name(chat_provider provider) {
    if (!provider.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Provider pointer is null"});
        return "";
    }
    return convert(provider)->get_name();
}

chat_provider_info chat_provider_get_info(chat_provider provider) {
    if (!provider.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Provider pointer is null"});
        return {};
    }
    chat::provider_info info = convert(provider)->get_info();
    return convert(info);
}

chat_channel chat_provider_register_channel(plugin_instance plugin, chat_provider provider, std::string id, std::string name) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return {};
    }
    plugins::plugin *pl = convert(plugin);
    if (!provider.ptr) {
        pl->log_if.put(logging::ERROR, {__func__, ": Provider pointer is null"});
        return {};
    }
    try {
        chat::channel *channel = convert(provider)->register_channel(id, name);
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

void chat_provider_delete(plugin_instance plugin, chat_provider *provider) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return;
    }
    plugins::plugin *pl = convert(plugin);
    if (!provider) {
        pl->log_if.put(logging::ERROR, {__func__, ": Provider argument is null"});
        return;
    }
    if (!provider->ptr) {
        pl->log_if.put(logging::ERROR, {__func__, ": Provider pointer is null"});
        return;
    }
    chat::provider *provider_c = convert(*provider);
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

std::string chat_channel_get_id(chat_channel channel) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return "";
    }
    return convert(channel)->get_id();
}

std::string chat_channel_get_name(chat_channel channel) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return "";
    }
    return convert(channel)->get_name();
}

std::string chat_channel_get_provider_id(chat_channel channel) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return "";
    }
    return convert(channel)->get_provider_id();
}

std::string chat_channel_get_provider_name(chat_channel channel) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return "";
    }
    return convert(channel)->get_provider_name();
}

chat_channel_info chat_channel_get_info(chat_channel channel) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return {};
    }
    chat::channel_info info = convert(channel)->get_info();
    return convert(info);
}

void chat_channel_push_one(chat_channel channel, chat_message *message) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return;
    }
    if (!message) {
        global_log.put(logging::ERROR, {__func__, ": Message argument is null"});
        return;
    }
    chat::message msg_converted = convert(*message);
    convert(channel)->push(msg_converted);
}

void chat_channel_push_many(chat_channel channel, std::vector<chat_message> *messages) {
    if (!channel.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return;
    }
    if (!messages) {
        global_log.put(logging::ERROR, {__func__, ": Messages argument is null"});
        return;
    }
    std::vector<chat::message> messages_converted;
    messages_converted.reserve(messages->size());
    for (auto &msg : *messages)
        messages_converted.push_back(convert(msg));
    convert(channel)->push(messages_converted);
}

void chat_channel_delete(plugin_instance plugin, chat_channel *channel) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return;
    }
    plugins::plugin *pl = convert(plugin);
    if (!channel) {
        pl->log_if.put(logging::ERROR, {__func__, ": Channel argument is null"});
        return;
    }
    if (!channel->ptr) {
        pl->log_if.put(logging::ERROR, {__func__, ": Channel pointer is null"});
        return;
    }
    chat::channel *channel_c = convert(*channel);
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

std::string chat_subscription_get_provider_id(chat_subscription sub) {
    if (!sub.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Subscription pointer is null"});
        return "";
    }
    return convert(sub)->get_provider_id();
}

std::string chat_subscription_get_channel_id(chat_subscription sub) {
    if (!sub.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Subscription pointer is null"});
        return "";
    }
    return convert(sub)->get_channel_id();
}

std::vector<chat_message> chat_subscription_pull(chat_subscription sub) {
    if (!sub.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Subscription pointer is null"});
        return {};
    }
    std::vector<chat::message> messages = convert(sub)->pull();
    std::vector<chat_message> messages_converted;
    messages_converted.reserve(messages.size());
    for (auto msg : messages)
        messages_converted.push_back(convert(msg));
    return messages_converted;
}

void chat_subscription_unsubscribe(chat_subscription sub) {
    if (!sub.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Subscription pointer is null"});
        return;
    }
    convert(sub)->unsubscribe();
}

void chat_subscription_delete(plugin_instance plugin, chat_subscription *sub) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return;
    }
    plugins::plugin *pl = convert(plugin);
    if (!sub) {
        pl->log_if.put(logging::ERROR, {__func__, ": Subscription argument is null"});
        return;
    }
    if (!sub->ptr) {
        pl->log_if.put(logging::ERROR, {__func__, ": Subscription pointer is null"});
        return;
    }
    chat::subscription *sub_c = convert(*sub);
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

void log_put(plugin_instance plugin, log_level level, std::vector<std::string> message) {
    if (!plugin.ptr) {
        global_log.put(logging::ERROR, {__func__, ": Plugin instance pointer is null"});
        return;
    }
    std::vector<logging::message_part> message_cnv;
    message_cnv.reserve(message.size());
    for (auto &msg : message)
        message_cnv.emplace_back(msg);
    convert(plugin)->log_pl.put((logging::level)level, message_cnv);
}

}
/*******************/

strtb_plugin::plugin_interface plugins::plugin::plugin_interface = {
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


