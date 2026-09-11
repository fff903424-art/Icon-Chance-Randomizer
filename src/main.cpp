#include <Geode/Geode.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <hiimjustin000.more_icons/include/MoreIcons.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace geode::prelude;
using namespace more_icons;

namespace icr {
    struct PoolEntry {
        enum class Kind { Vanilla, Custom } kind;
        int id = 0;
        std::string name;
        double weight = 0.0;
    };

    std::string trim(std::string value) {
        auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char c) {
            return !isSpace(static_cast<unsigned char>(c));
        }));
        value.erase(std::find_if(value.rbegin(), value.rend(), [&](char c) {
            return !isSpace(static_cast<unsigned char>(c));
        }).base(), value.end());
        return value;
    }

    bool parseInt(std::string_view text, int& out) {
        if (text.empty()) return false;
        auto result = std::from_chars(text.data(), text.data() + text.size(), out);
        return result.ec == std::errc{} && result.ptr == text.data() + text.size();
    }

    bool parseWeight(std::string_view text, double& out) {
        std::string owned(text);
        if (owned.empty()) return false;
        char* end = nullptr;
        out = std::strtod(owned.c_str(), &end);
        return end != owned.c_str() && *end == '\0' && std::isfinite(out) && out > 0.0;
    }

    std::vector<PoolEntry> parsePool(std::string const& raw) {
        std::vector<PoolEntry> result;

        std::size_t start = 0;
        while (start <= raw.size()) {
            auto comma = raw.find(',', start);
            auto token = trim(raw.substr(start, comma == std::string::npos
                ? std::string::npos : comma - start));

            if (!token.empty()) {
                auto equals = token.rfind('=');
                if (equals == std::string::npos) {
                    log::warn("ICR: ignored '{}': missing '='", token);
                } else {
                    auto left = trim(token.substr(0, equals));
                    auto weightText = trim(token.substr(equals + 1));
                    double weight = 0.0;

                    if (!parseWeight(weightText, weight)) {
                        log::warn("ICR: ignored '{}': invalid weight", token);
                    } else if (left.rfind("vanilla:", 0) == 0) {
                        int id = 0;
                        auto idText = trim(left.substr(8));
                        if (!parseInt(idText, id) || id <= 0) {
                            log::warn("ICR: ignored '{}': invalid vanilla cube ID", token);
                        } else {
                            result.push_back(PoolEntry{PoolEntry::Kind::Vanilla, id, {}, weight});
                        }
                    } else if (left.rfind("custom:", 0) == 0) {
                        auto name = trim(left.substr(7));
                        if (name.empty()) {
                            log::warn("ICR: ignored '{}': empty More Icons name", token);
                        } else {
                            result.push_back(PoolEntry{PoolEntry::Kind::Custom, 0, std::move(name), weight});
                        }
                    } else {
                        log::warn("ICR: ignored '{}': use vanilla:ID or custom:NAME", token);
                    }
                }
            }

            if (comma == std::string::npos) break;
            start = comma + 1;
        }

        return result;
    }

    bool enabled() {
        return Mod::get()->getSettingValue<bool>("enabled");
    }

    void applyEntry(PoolEntry const& entry) {
        auto gm = GameManager::get();
        if (entry.kind == PoolEntry::Kind::Vanilla) {
            gm->setPlayerFrame(entry.id);
            if (Mod::get()->getSettingValue<bool>("debug")) {
                log::info("ICR: selected vanilla cube {}", entry.id);
            }
            return;
        }

        auto* icon = more_icons::getIcon(entry.name, IconType::Cube);
        if (!icon) {
            log::warn("ICR: More Icons cube '{}' was not found", entry.name);
            return;
        }

        more_icons::setIcon(icon, IconType::Cube);
        if (Mod::get()->getSettingValue<bool>("debug")) {
            log::info("ICR: selected More Icons cube '{}'", entry.name);
        }
    }

    void randomize() {
        if (!enabled()) return;

        auto raw = Mod::get()->getSettingValue<std::string>("pool");
        auto parsed = parsePool(raw);
        if (parsed.empty()) {
            log::warn("ICR: pool is empty or contains no valid entries");
            return;
        }

        std::vector<PoolEntry const*> valid;
        std::vector<double> weights;
        valid.reserve(parsed.size());
        weights.reserve(parsed.size());

        for (auto const& entry : parsed) {
            if (entry.kind == PoolEntry::Kind::Custom &&
                !more_icons::getIcon(entry.name, IconType::Cube)) {
                log::warn("ICR: skipping missing More Icons cube '{}'", entry.name);
                continue;
            }
            valid.push_back(&entry);
            weights.push_back(entry.weight);
        }

        if (valid.empty()) {
            log::warn("ICR: no usable entries remain in the pool");
            return;
        }

        static std::random_device rd;
        static std::mt19937_64 rng(rd());
        std::discrete_distribution<std::size_t> dist(weights.begin(), weights.end());
        applyEntry(*valid[dist(rng)]);
    }

    void savePool(std::string const& pool) {
        Mod::get()->setSettingValue("pool", pool);
    }
}

class CubeChancePopup final : public geode::Popup {
    geode::TextInput* m_poolInput = nullptr;

    bool setup(std::string const&) {
        this->setTitle("Cube Chances");

        auto explanation = CCLabelBMFont::create(
            "Format: vanilla:ID=weight, custom:NAME=weight",
            "goldFont.fnt"
        );
        explanation->setScale(0.42f);
        m_mainLayer->addChildAtPosition(explanation, Anchor::Top, ccp(0, -30));

        m_poolInput = geode::TextInput::create(420.f, "Cube chance pool", "bigFont.fnt");
        m_poolInput->setLabel("Pool");
        m_poolInput->setMaxCharCount(4000);
        m_poolInput->setString(
            Mod::get()->getSettingValue<std::string>("pool"), false
        );
        m_mainLayer->addChildAtPosition(m_poolInput, Anchor::Center, ccp(0, 8));

        auto example = CCLabelBMFont::create(
            "Example: vanilla:1=50, vanilla:7=10, custom:MyCube=40",
            "goldFont.fnt"
        );
        example->setScale(0.36f);
        m_mainLayer->addChildAtPosition(example, Anchor::Bottom, ccp(0, 40));

        auto saveSprite = ButtonSprite::create("Save");
        auto saveButton = CCMenuItemSpriteExtra::create(
            saveSprite, this, menu_selector(CubeChancePopup::onSave)
        );
        saveButton->setID("save-button");
        m_buttonMenu->addChildAtPosition(saveButton, Anchor::Bottom, ccp(-70, 16));

        auto randomizeSprite = ButtonSprite::create("Save + Roll");
        auto randomizeButton = CCMenuItemSpriteExtra::create(
            randomizeSprite, this, menu_selector(CubeChancePopup::onSaveAndRandomize)
        );
        randomizeButton->setID("save-roll-button");
        m_buttonMenu->addChildAtPosition(randomizeButton, Anchor::Bottom, ccp(70, 16));

        return true;
    }

    void onSave(CCObject*) {
        icr::savePool(m_poolInput->getString());
        this->onClose(nullptr);
    }

    void onSaveAndRandomize(CCObject*) {
        icr::savePool(m_poolInput->getString());
        icr::randomize();
        this->onClose(nullptr);
    }

public:
    static CubeChancePopup* create() {
        auto ret = new CubeChancePopup();
        if (ret && ret->init(480.f, 250.f, "GJ_square01.png")) {
            ret->autorelease();
            if (!ret->setup("")) {
                ret->release();
                return nullptr;
            }
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(IconChanceGarageLayer, GJGarageLayer) {
    bool init() {
        if (!GJGarageLayer::init()) return false;

        auto buttonSprite = ButtonSprite::create("Cube Chances");
        buttonSprite->setScale(0.55f);
        auto button = CCMenuItemSpriteExtra::create(
            buttonSprite,
            this,
            menu_selector(IconChanceGarageLayer::openCubeChances)
        );
        button->setID("icon-chance-randomizer/cube-chances");

        auto menu = CCMenu::create();
        menu->setID("icon-chance-randomizer/menu");
        menu->addChild(button);
        menu->setPosition(ccp(65.f, 42.f));
        this->addChild(menu);
        return true;
    }

    void openCubeChances(CCObject*) {
        CubeChancePopup::create()->show();
    }
};

class $modify(IconChancePlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay) {
        if (!PlayLayer::init(level, useReplay)) return false;

        if (Mod::get()->getSettingValue<bool>("randomize_on_restart")) {
            icr::randomize();
        }
        return true;
    }
};
