#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "component.hpp"
#include "preset_manager.hpp"
#include "update_gui.hpp"

#if __has_include("BinaryData.h")
#include "BinaryData.h"
#define HAS_BINARY_DATA 1
#else
#define HAS_BINARY_DATA 0
#endif

namespace pluginshared {
class PresetPanel
    : public juce::Component
    , juce::Button::Listener {
public:
    class TitleButton : public juce::Component {
    public:
        TitleButton() {
            addAndMakeVisible(title_);
            addAndMakeVisible(version_);
            addAndMakeVisible(simd_inst_);

#if HAS_BINARY_DATA
            static auto typeface =
                juce::Typeface::createSystemTypefaceFor(BinaryData::FSEX300_ttf, BinaryData::FSEX300_ttfSize);
            auto font = juce::FontOptions{typeface};
            title_.setFont(font);
#endif

            title_.addMouseListener(this, true);
            version_.addMouseListener(this, true);
            simd_inst_.addMouseListener(this, true);

            SetLabelColor(false);
        }

        ~TitleButton() override {
            title_.removeMouseListener(this);
            version_.removeMouseListener(this);
            simd_inst_.removeMouseListener(this);
        }

        void paint(juce::Graphics& g) override {
            auto bounds = getLocalBounds().toFloat();
            auto center = bounds.getCentre();

            float radius = center.getDistanceFrom(bounds.getTopLeft());

            juce::ColourGradient gradient(ui::light_green_bg, center.getX(), center.getY(), ui::green_bg,
                                          center.getX() + radius, center.getY(), true);
            g.setGradientFill(gradient);
            g.fillAll();
        }

        juce::Rectangle<int> GetBoundsFit(int h) {
            float height = static_cast<float>(h);
            float title_w = juce::TextLayout::getStringWidth(title_.getFont().withHeight(height), title_.getText());
            float version_w =
                juce::TextLayout::getStringWidth(version_.getFont().withHeight(height / 2), version_.getText());
            float simd_w =
                juce::TextLayout::getStringWidth(simd_inst_.getFont().withHeight(height / 2), simd_inst_.getText());
            float post_w = std::max(simd_w, version_w);
            float w = title_w + post_w;
            return juce::Rectangle<float>{0.0f, 0.0f, w, height}.toNearestInt();
        }

        void resized() override {
            auto b = getLocalBounds().toFloat();
            float height = b.getHeight();
            float font_heiht = height * 0.9f;
            title_.setFont(title_.getFont().withHeight(font_heiht));
            version_.setFont(version_.getFont().withHeight(height / 2));
            simd_inst_.setFont(simd_inst_.getFont().withHeight(height / 2));

            float title_w = juce::TextLayout::getStringWidth(title_.getFont(), title_.getText());
            title_.setBounds(b.removeFromLeft(title_w).toNearestInt());

            version_.setBounds(b.removeFromTop(b.getHeight() / 2).toNearestInt());
            simd_inst_.setBounds(b.toNearestInt());
        }

        void mouseDown(const juce::MouseEvent& e) override {
            juce::ignoreUnused(e);

            if (on_click_) {
                on_click_();
            }
        }

        void mouseEnter(const juce::MouseEvent& e) override {
            juce::ignoreUnused(e);
            SetLabelColor(true);
        }

        void mouseExit(const juce::MouseEvent& e) override {
            juce::ignoreUnused(e);
            SetLabelColor(false);
        }

        void SetLabelColor(bool hover) {
            juce::Colour color = ui::black_bg;
            if (hover) {
                color = ui::line_fore;
            }

            title_.setColour(juce::Label::ColourIds::textColourId, color);
            version_.setColour(juce::Label::ColourIds::textColourId, color);
            simd_inst_.setColour(juce::Label::ColourIds::textColourId, color);
        }

        std::function<void()> on_click_;
        juce::Label title_;
        juce::Label version_;
        juce::Label simd_inst_;
    };

    PresetPanel(PresetManager& pm)
        : presetManager(pm) {
        configureButton(saveButton, "Save");
        configureButton(deleteButton, "Delete");
        configureButton(previousPresetButton, "<");
        configureButton(nextPresetButton, ">");
        ui::SetLableBlack(preset_name_);
        preset_name_.addMouseListener(this, false);
        preset_name_.setText(presetManager.getCurrentPreset(), juce::dontSendNotification);
        addAndMakeVisible(preset_name_);
        preset_menu_.setLookAndFeel(ui::GetLookAndFeel());
        loadPresetList();

        options_button_.title_.setText(JucePlugin_Name, juce::dontSendNotification);
        options_button_.version_.setText(JucePlugin_VersionString, juce::dontSendNotification);
        options_button_.on_click_ = [this] {
            juce::PopupMenu menu;

            juce::String plugin_name;
            plugin_name << JucePlugin_Name << ' ' << JucePlugin_VersionString;
            menu.addItem(plugin_name, false, false, [] {});

            if (presetManager.GetUpdateData().HaveNewVersion()) {
                menu.addItem("new version", [url = juce::URL{presetManager.GetUpdateData().GetPluginReleaseUrl()}] {
                    url.launchInDefaultBrowser();
                });
            }
            else {
                menu.addItem("check update", [this] { CheckUpdate(); });
            }

            menu.addItem("init patch", [this] {
                preset_name_.setText(PresetManager::kDefaultPresetName, juce::dontSendNotification);
                presetManager.loadDefaultPatch();
            });

            // scale
            juce::PopupMenu scale_menu;
            scale_menu.addItem("100%", [this] { TrySetParentScale(1.0f); });
            scale_menu.addItem("125%", [this] { TrySetParentScale(1.25f); });
            scale_menu.addItem("150%", [this] { TrySetParentScale(1.5f); });
            scale_menu.addItem("175%", [this] { TrySetParentScale(1.75f); });
            scale_menu.addItem("200%", [this] { TrySetParentScale(2.0f); });
            scale_menu.addItem("300%", [this] { TrySetParentScale(3.0f); });
            menu.addSubMenu("scale", std::move(scale_menu));

            if (on_menu_showup) {
                on_menu_showup(menu);
            }

            juce::PopupMenu::Options op;
            menu.setLookAndFeel(ui::GetLookAndFeel());
            menu.showMenuAsync(op.withMousePosition());
        };
        addAndMakeVisible(options_button_);
    }

    ~PresetPanel() override {
        saveButton.removeListener(this);
        deleteButton.removeListener(this);
        previousPresetButton.removeListener(this);
        nextPresetButton.removeListener(this);
        preset_name_.removeMouseListener(this);
        preset_menu_.setLookAndFeel(nullptr);
    }

    void SetDspInstName(const char* name) {
        if (name != nullptr) {
            options_button_.simd_inst_.setText(name, juce::dontSendNotification);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        if (event.originalComponent != &preset_name_) return;
        preset_menu_.showMenuAsync(juce::PopupMenu::Options{}.withMousePosition(), [this](int id) {
            if (id == 1) {
                presetManager.loadDefaultPatch();
                preset_name_.setText(PresetManager::kDefaultPresetName, juce::dontSendNotification);
            }
            else if (id > 1) {
                const auto& all_items = presetManager.getAllPresets();
                int idx = id - 2;
                if (idx >= all_items.size()) return;

                const auto& name = all_items[idx];
                presetManager.loadPreset(name);
                preset_name_.setText(name, juce::dontSendNotification);
            }
        });
    }

    static int GetButtonWidth(juce::TextButton& button, int height) {
        constexpr int pad = 4;
        return pad * 2
             + static_cast<int>(juce::TextLayout::getStringWidth(
                 button.getLookAndFeel().getTextButtonFont(button, height), button.getButtonText()));
    }

    void resized() override {
        auto container = getLocalBounds();
        options_button_.setBounds(
            container.removeFromLeft(options_button_.GetBoundsFit(container.getHeight()).getWidth()));

        deleteButton.setBounds(
            container.removeFromRight(GetButtonWidth(deleteButton, container.getHeight())).reduced(2));
        saveButton.setBounds(container.removeFromRight(GetButtonWidth(saveButton, container.getHeight())).reduced(2));
        previousPresetButton.setBounds(container.removeFromLeft(container.getHeight()).reduced(2));
        nextPresetButton.setBounds(container.removeFromRight(container.getHeight()).reduced(2));
        preset_name_.setBounds(container.reduced(2));
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(ui::green_bg);
    }

    std::function<void(juce::PopupMenu&)> on_menu_showup;
private:
    void buttonClicked(juce::Button* button) override {
        if (button == &saveButton) {
            fileChooser =
                std::make_unique<juce::FileChooser>("Please enter the name of the preset to save",
                                                    PresetManager::defaultDirectory, "*." + PresetManager::extension);
            fileChooser->launchAsync(juce::FileBrowserComponent::saveMode, [&](const juce::FileChooser& chooser) {
                const auto resultFile = chooser.getResult();
                const auto preset_name = resultFile.getFileNameWithoutExtension();
                if (preset_name == PresetManager::kDefaultPresetName) {
                    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Invalid name",
                                                                "default preset name is invalid");
                    return;
                }
                presetManager.savePreset(preset_name);
                loadPresetList();
            });
        }
        else if (button == &previousPresetButton) {
            auto [index, name] = presetManager.loadPreviousPreset();
            preset_name_.setText(name, juce::dontSendNotification);
        }
        else if (button == &nextPresetButton) {
            auto [index, name] = presetManager.loadNextPreset();
            preset_name_.setText(name, juce::dontSendNotification);
        }
        else if (button == &deleteButton) {
            presetManager.deletePreset(presetManager.getCurrentPreset());
            loadPresetList();
            preset_name_.setText(presetManager.getCurrentPreset(), juce::dontSendNotification);
        }
    }

    void TrySetParentScale(float scale) {
        auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>();
        if (editor != nullptr) {
            editor->setScaleFactor(scale);
        }
    }

    void configureButton(juce::Button& button, const juce::String& buttonText) {
        button.setButtonText(buttonText);
        addAndMakeVisible(button);
        button.addListener(this);
    }

    void loadPresetList() {
        const auto currentPreset = presetManager.getCurrentPreset();
        preset_name_.setText(currentPreset, juce::dontSendNotification);
        const auto& allPresets = presetManager.getAllPresets();
        preset_menu_.clear();
        preset_menu_.addItem(1, "init patch");
        preset_menu_.addSeparator();
        for (int id = 2; const auto& name : allPresets) {
            preset_menu_.addItem(id++, name);
        }
    }

    void CheckUpdate() {
        presetManager.GetUpdateData().BeginCheck();
        auto* diaglog = new UpdateMessageDialog(presetManager.GetUpdateData());
        diaglog->enterModalState(true, nullptr, true);
    }

    PresetManager& presetManager;
    ui::FlatButton saveButton, deleteButton, previousPresetButton, nextPresetButton;
    juce::Label preset_name_{"", PresetManager::kDefaultPresetName};
    juce::PopupMenu preset_menu_;
    std::unique_ptr<juce::FileChooser> fileChooser;
    TitleButton options_button_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetPanel)
};
} // namespace pluginshared
