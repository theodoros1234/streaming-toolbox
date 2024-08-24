#include "chat_tab.h"
#include "ui_chat_tab.h"

#include <QScrollBar>

using namespace strtb;
using namespace strtb::gui;

chat_tab::chat_tab(chat::interface *chat_if, QWidget *parent)
    : QWidget(parent), log("GUI: Chat tab"), ui(new Ui::chat_tab) {
    // Set up UI and objects
    ui->setupUi(this);
    this->chat_view_scrollbar = this->ui->chatView->verticalScrollBar();

    // Signals and slots
    QObject::connect(this->chat_view_scrollbar, &QScrollBar::valueChanged, this, &chat_tab::view_scrolled);
    QObject::connect(this->chat_view_scrollbar, &QScrollBar::rangeChanged, this, &chat_tab::view_scroll_height_changed);
    QObject::connect(this->ui->scrollToBottom, &QPushButton::clicked, this, &chat_tab::scroll_to_bottom);

    // Try subscribing to chat and creating thread that listens to it
    try {
        this->chat_sub = chat_if->subscribe("", "");
        this->chat_sub_thread = new chat_subscription_thread(&this->log, this->chat_sub);
        QObject::connect(this->chat_sub_thread, &chat_subscription_thread::messages_received, this, &chat_tab::message_received, Qt::QueuedConnection);
        this->chat_sub_thread->start();
    } catch (std::exception &e) {
        this->log.put(logging::ERROR, {"Could not set up chat monitoring: ", e.what()});
    }
}

chat_tab::~chat_tab() {
    // Unsubscribe from chat
    if (this->chat_sub) {
        this->chat_sub->unsubscribe();
        if (this->chat_sub_thread)
            this->chat_sub_thread->wait();
        delete this->chat_sub_thread;
        delete this->chat_sub;
    }
    // Delete UI
    delete ui;
}

void chat_tab::view_scrolled(int value) {
    bool new_value = value >= this->chat_view_scrollbar->maximum() - 10;
    if (new_value != this->scrolled_to_bottom) {
        this->scrolled_to_bottom = new_value;
        // If value changed, enable or disable the "scroll to bottom" button accordingly
        this->ui->scrollToBottom->setEnabled(!new_value);
    }
}

void chat_tab::view_scroll_height_changed(int min, int max) {
    // Scroll to bottom if previously scrolled to bottom
    if (this->scrolled_to_bottom)
        this->chat_view_scrollbar->setValue(max);
}

void chat_tab::scroll_to_bottom() {
    this->chat_view_scrollbar->setValue(this->chat_view_scrollbar->maximum());
}

void chat_tab::message_received(std::vector<QString> messages) {
    for (auto &msg : messages)
        this->ui->chatView->append(msg);
}

